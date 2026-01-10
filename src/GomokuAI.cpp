#include "GomokuAI.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
#include <limits>
#include <array>
#include <cstring>

// --- CONSTANTS & CONFIG ---
constexpr int INF = 1000000000;
constexpr int SCORE_WIN = 100000000;

struct TTEntry {
    uint64_t key;
    int depth;
    int value;
    int flag; // 0: Exact, 1: Lowerbound, 2: Upperbound
    int best_move_idx;
};

constexpr int TT_SIZE = 1 << 20; 
TTEntry TT[TT_SIZE];

int DIRS[4]; 
int killer_moves[100][2];
int history_moves[3][400];

std::chrono::steady_clock::time_point start_time;
int time_limit_ms;
bool time_out_flag;

// --- HELPERS ---

void clear_tt() {
    std::memset(TT, 0, sizeof(TT));
}

void clear_history() {
    std::memset(killer_moves, -1, sizeof(killer_moves));
    std::memset(history_moves, 0, sizeof(history_moves));
}

bool check_time() {
    if (time_out_flag) return true;
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() >= time_limit_ms) {
        time_out_flag = true;
    }
    return time_out_flag;
}

// --- ZOBRIST ---

static uint64_t splitmix64(uint64_t& x) {
    uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

void GomokuAI::init_zobrist() {
    zobrist.resize(width * height * 3);
    uint64_t seed = 0x123456789ABCDEFULL;
    for (auto& z : zobrist) z = splitmix64(seed);
}

inline uint64_t GomokuAI::zobrist_at(int idx, int player) const {
    return zobrist[idx * 3 + player];
}

// --- GOMOKU CLASS ---

GomokuAI::GomokuAI() : width(20), height(20), min_x(10), max_x(10), min_y(10), max_y(10) {}

void GomokuAI::init(int size) {
    width = size;
    height = size;
    board.assign(width * height, 0);
    DIRS[0] = 1;          // Right
    DIRS[1] = width;      // Down
    DIRS[2] = width + 1;  // Diag Down-Right
    DIRS[3] = width - 1;  // Diag Down-Left
    
    min_x = size; max_x = 0;
    min_y = size; max_y = 0;
    
    init_zobrist();
    hash_key = 0;
    clear_tt();
    clear_history();
}

Point GomokuAI::parse_coordinates(const std::string& s) {
    size_t c = s.find(',');
    if (c == std::string::npos) return {-1, -1};
    return {std::stoi(s.substr(0, c)), std::stoi(s.substr(c + 1))};
}

void GomokuAI::update_board(int x, int y, int player) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    int idx = y * width + x;
    
    if (board[idx] != player) {
        if (board[idx] != 0) hash_key ^= zobrist_at(idx, board[idx]); 
        board[idx] = player;
        if (player != 0) {
            hash_key ^= zobrist_at(idx, player); 
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
    }
}

// --- ENGINE ---

bool check_win(const std::vector<int>& board, int idx, int w, int h, int player) {
    // 4 Directions
    // We check centered at idx.
    int x = idx % w;
    int y = idx / w;

    // Dir 1: Horizontal (1,0)
    {
        int count = 1;
        for (int i = 1; i < 5; ++i) { if (x + i >= w || board[y * w + (x + i)] != player) break; count++; }
        for (int i = 1; i < 5; ++i) { if (x - i < 0 || board[y * w + (x - i)] != player) break; count++; }
        if (count >= 5) return true;
    }
    // Dir 2: Vertical (0,1)
    {
        int count = 1;
        for (int i = 1; i < 5; ++i) { if (y + i >= h || board[(y + i) * w + x] != player) break; count++; }
        for (int i = 1; i < 5; ++i) { if (y - i < 0 || board[(y - i) * w + x] != player) break; count++; }
        if (count >= 5) return true;
    }
    // Dir 3: Diag (1,1)
    {
        int count = 1;
        for (int i = 1; i < 5; ++i) { if (x + i >= w || y + i >= h || board[(y + i) * w + (x + i)] != player) break; count++; }
        for (int i = 1; i < 5; ++i) { if (x - i < 0 || y - i < 0 || board[(y - i) * w + (x - i)] != player) break; count++; }
        if (count >= 5) return true;
    }
    // Dir 4: Anti-Diag (-1,1)
    {
        int count = 1;
        for (int i = 1; i < 5; ++i) { if (x - i < 0 || y + i >= h || board[(y + i) * w + (x - i)] != player) break; count++; }
        for (int i = 1; i < 5; ++i) { if (x + i >= w || y - i < 0 || board[(y - i) * w + (x + i)] != player) break; count++; }
        if (count >= 5) return true;
    }
    return false;
}

int eval_state(const GomokuAI& ai, int player) {
    const int W_LIVE_4 = 100000;
    const int W_DEAD_4 = 2000;
    const int W_LIVE_3 = 2000;
    const int W_DEAD_3 = 100;
    const int W_LIVE_2 = 100;
    
    int total_score = 0;
    
    int sx = std::max(0, ai.min_x - 1);
    int ex = std::min(ai.width - 1, ai.max_x + 1);
    int sy = std::max(0, ai.min_y - 1);
    int ey = std::min(ai.height - 1, ai.max_y + 1);

    auto eval_line_dir = [&](int cx, int cy, int dx, int dy) -> int {
        int idx = cy * ai.width + cx;
        int p = ai.board[idx];
        if (p == 0) return 0;
        
        int px = cx - dx; 
        int py = cy - dy;
        bool is_start = true;
        if (px >= 0 && px < ai.width && py >= 0 && py < ai.height) {
            if (ai.board[py * ai.width + px] == p) is_start = false;
        }
        if (!is_start) return 0;

        int count = 0;
        int tx = cx;
        int ty = cy;
        while (tx >= 0 && tx < ai.width && ty >= 0 && ty < ai.height && ai.board[ty * ai.width + tx] == p) {
            count++;
            tx += dx;
            ty += dy;
        }
        
        bool open_head = false;
        if (px >= 0 && px < ai.width && py >= 0 && py < ai.height && ai.board[py * ai.width + px] == 0) open_head = true;
        
        bool open_tail = false;
        if (tx >= 0 && tx < ai.width && ty >= 0 && ty < ai.height && ai.board[ty * ai.width + tx] == 0) open_tail = true;
        
        int val = 0;
        if (count >= 5) val = SCORE_WIN;
        else if (count == 4) {
            if (open_head && open_tail) val = W_LIVE_4;
            else if (open_head || open_tail) val = W_DEAD_4;
        } else if (count == 3) {
             if (open_head && open_tail) val = W_LIVE_3;
             else if (open_head || open_tail) val = W_DEAD_3;
        } else if (count == 2) {
             if (open_head && open_tail) val = W_LIVE_2;
        }

        return (p == player) ? val : -val;
    };

    for (int y = sy; y <= ey; ++y) {
        for (int x = sx; x <= ex; ++x) total_score += eval_line_dir(x, y, 1, 0);
    }
    for (int y = sy; y <= ey; ++y) {
        for (int x = sx; x <= ex; ++x) total_score += eval_line_dir(x, y, 0, 1);
    }
    for (int y = sy; y <= ey; ++y) {
        for (int x = sx; x <= ex; ++x) total_score += eval_line_dir(x, y, 1, 1);
    }
    for (int y = sy; y <= ey; ++y) {
        for (int x = sx; x <= ex; ++x) total_score += eval_line_dir(x, y, -1, 1);
    }
    
    return total_score;
}

int negamax(GomokuAI& ai, int depth, int alpha, int beta, int player, int ply) {
    if ((ply & 127) == 0 && check_time()) return 0;

    int opponent = (player == 1) ? 2 : 1;
    uint64_t key = ai.get_hash_key();
    int tt_idx = key % TT_SIZE;
    TTEntry& tte = TT[tt_idx];

    if (tte.key == key && tte.depth >= depth) {
        if (tte.flag == 0) return tte.value; 
        if (tte.flag == 1 && tte.value >= beta) return tte.value; 
        if (tte.flag == 2 && tte.value <= alpha) return tte.value; 
    }

    if (depth == 0) {
        return eval_state(ai, player);
    }

    std::vector<std::pair<int, int>> moves;
    moves.reserve(40);
    
    int sx = std::max(0, ai.min_x - 2);
    int ex = std::min(ai.width - 1, ai.max_x + 2);
    int sy = std::max(0, ai.min_y - 2);
    int ey = std::min(ai.height - 1, ai.max_y + 2);

    int w = ai.width;
    
    for (int y = sy; y <= ey; ++y) {
        for (int x = sx; x <= ex; ++x) {
            int idx = y * w + x;
            if (ai.board[idx] != 0) continue;
            
            bool rel = false;
            for (int dy=-2; dy<=2; ++dy) {
                for (int dx=-2; dx<=2; ++dx) {
                     if (dx==0 && dy==0) continue;
                     int nx = x+dx; int ny = y+dy;
                     if (nx>=0 && nx<ai.width && ny>=0 && ny<ai.height) {
                         if (ai.board[ny*w+nx] != 0) { rel = true; break; }
                     }
                }
                if (rel) break;
            }
            
            if (rel) {
                int score = 0;
                if (idx == tte.best_move_idx) score += 1000000; 
                if (killer_moves[ply][0] == idx) score += 900000;
                else if (killer_moves[ply][1] == idx) score += 800000;
                score += history_moves[player][idx];
                int dist = std::abs(x - ai.width/2) + std::abs(y - ai.height/2);
                score -= dist;
                moves.push_back({score, idx});
            }
        }
    }
    
    if (moves.empty()) return eval_state(ai, player);

    std::sort(moves.rbegin(), moves.rend());

    int best_val = -INF;
    int best_move = -1;
    int type = 2; // Upperbound

    for (const auto& mv : moves) {
        int idx = mv.second;
        
        ai.update_board(idx % w, idx / w, player);
        
        if (check_win(ai.board, idx, w, ai.height, player)) {
            ai.update_board(idx % w, idx / w, 0); 
            best_val = SCORE_WIN - ply; 
            best_move = idx;
            type = 0;
            alpha = best_val;
            break; 
        }

        int val = -negamax(ai, depth - 1, -beta, -alpha, opponent, ply + 1);
        
        ai.update_board(idx % w, idx / w, 0);

        if (time_out_flag) return 0;

        if (val > best_val) {
            best_val = val;
            best_move = idx;
        }
        
        alpha = std::max(alpha, best_val);
        if (alpha >= beta) {
            type = 1; // Lowerbound
            if (ply < 100) {
                if (killer_moves[ply][0] != idx) {
                    killer_moves[ply][1] = killer_moves[ply][0];
                    killer_moves[ply][0] = idx;
                }
            }
            history_moves[player][idx] += depth * depth;
            break;
        }
    }

    if (!time_out_flag) {
        tte.key = key;
        tte.depth = depth;
        tte.value = best_val;
        tte.flag = type;
        tte.best_move_idx = best_move;
    }

    return best_val;
}

Point GomokuAI::find_best_move(int time_limit) {
    start_time = std::chrono::steady_clock::now();
    time_limit_ms = time_limit - 100; 
    time_out_flag = false;
    
    int center = (height/2) * width + (width/2);
    if (board[center] == 0) {
        bool empty = true;
        for (int c : board) if(c != 0) { empty = false; break; }
        if (empty) return {width/2, height/2};
    }
    
    Point best = {-1, -1};
    int max_depth = 20;
    
    for (int depth = 1; depth <= max_depth; ++depth) {
        int alpha = -INF;
        int beta = INF;
        
        int best_val = -INF;
        int current_best_idx = -1;
        
        std::vector<std::pair<int, int>> moves;
        int sx = std::max(0, min_x - 2); int ex = std::min(width - 1, max_x + 2);
        int sy = std::max(0, min_y - 2); int ey = std::min(height - 1, max_y + 2);
        int w = width;
        for (int y = sy; y <= ey; ++y) {
            for (int x = sx; x <= ex; ++x) {
                int idx = y * w + x;
                if (board[idx] != 0) continue;
                bool rel = false;
                for (int dy=-2; dy<=2; ++dy) { for (int dx=-2; dx<=2; ++dx) {
                     if (dx==0 && dy==0) continue;
                     int nx = x+dx; int ny = y+dy;
                     if (nx>=0 && nx<width && ny>=0 && ny<height && board[ny*w+nx] != 0) { rel = true; break; }
                }}
                if (rel) {
                    int score = 0;
                    if (depth > 1 && best.x != -1 && (y*w+x) == (best.y*width + best.x)) score += 10000000;
                    score += history_moves[1][idx];
                    moves.push_back({score, idx});
                }
            }
        }
        std::sort(moves.rbegin(), moves.rend());
        
        if (moves.empty()) {
             for(int i=0; i<width*height; ++i) if(board[i]==0) return {i%width, i/width};
        }

        for (const auto& mv : moves) {
            int idx = mv.second;
            update_board(idx % w, idx / w, 1);
            
            if (check_win(board, idx, w, height, 1)) {
                 update_board(idx % w, idx / w, 0);
                 return {idx % width, idx / width}; 
            }
            
            int val = -negamax(*this, depth - 1, -beta, -alpha, 2, 1);
            
            update_board(idx % w, idx / w, 0);
            
            if (time_out_flag) break;
            
            if (val > best_val) {
                best_val = val;
                current_best_idx = idx;
            }
            alpha = std::max(alpha, best_val);
        }

        if (time_out_flag) break;
        
        if (current_best_idx != -1) {
            best = {current_best_idx % width, current_best_idx / width};
            if (best_val >= SCORE_WIN - 100) return best;
        }
    }
    
    return best;
}
