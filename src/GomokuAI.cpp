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
constexpr int TIMEOUT_SCORE = -2000000000; // Sentinel value
constexpr int TIME_CHECK_STRIDE = 4096;    // Check time every N nodes

struct TTEntry {
    uint64_t key;
    int depth;
    int value;
    int flag; // 0: Exact, 1: Lowerbound, 2: Upperbound
    int best_move_idx;
};

constexpr int TT_SIZE = 1 << 20;
TTEntry TT[TT_SIZE];

int killer_moves[100][2];
int history_moves[3][400]; // [player][idx]

std::chrono::steady_clock::time_point start_time;
int time_limit_ms;
int guard_time_ms;
bool time_out_flag;
uint64_t nodes_visited;

// --- HELPERS ---

void clear_tt() {
    std::memset(TT, 0, sizeof(TT));
}

void clear_history() {
    std::memset(killer_moves, -1, sizeof(killer_moves));
    std::memset(history_moves, 0, sizeof(history_moves));
}

bool check_time() {
    ++nodes_visited;
    if ((nodes_visited & (TIME_CHECK_STRIDE - 1)) != 0) {
        return time_out_flag;
    }
    if (time_out_flag) return true;

    auto now = std::chrono::steady_clock::now();
    int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count());
    
    if (elapsed >= guard_time_ms) {
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
            // Dynamic bounds update
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
    }
}

// --- EVALUATION & CHECKS ---

bool check_win(const std::vector<int>& board, int idx, int w, int h, int player) {
    int x = idx % w;
    int y = idx / w;

    // Directions: Horizontal, Vertical, Diagonal, Anti-Diagonal
    int dx[] = {1, 0, 1, -1};
    int dy[] = {0, 1, 1, 1};

    for (int d = 0; d < 4; ++d) {
        int count = 1;
        for (int i = 1; i < 5; ++i) {
            int nx = x + i * dx[d];
            int ny = y + i * dy[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h || board[ny * w + nx] != player) break;
            count++;
        }
        for (int i = 1; i < 5; ++i) {
            int nx = x - i * dx[d];
            int ny = y - i * dy[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h || board[ny * w + nx] != player) break;
            count++;
        }
        if (count >= 5) return true;
    }
    return false;
}

int eval_state(const GomokuAI& ai, int player) {
    // Simplified Evaluation Function
    const int W_LIVE_4 = 100000;
    const int W_DEAD_4 = 2000;
    const int W_LIVE_3 = 2000;
    const int W_DEAD_3 = 100;
    const int W_LIVE_2 = 100;

    int total_score = 0;
    int sx = std::max(0, ai.min_x - 1); int ex = std::min(ai.width - 1, ai.max_x + 1);
    int sy = std::max(0, ai.min_y - 1); int ey = std::min(ai.height - 1, ai.max_y + 1);

    auto eval_line = [&](int cx, int cy, int dx, int dy) -> int {
        int idx = cy * ai.width + cx;
        int p = ai.board[idx];
        if (p == 0) return 0;
        
        // Only evaluate start of lines to avoid duplication
        int px = cx - dx, py = cy - dy;
        if (px >= 0 && px < ai.width && py >= 0 && py < ai.height && ai.board[py * ai.width + px] == p) return 0;

        int count = 0;
        int tx = cx, ty = cy;
        while (tx >= 0 && tx < ai.width && ty >= 0 && ty < ai.height && ai.board[ty * ai.width + tx] == p) {
            count++; tx += dx; ty += dy;
        }

        bool open_head = (px >= 0 && px < ai.width && py >= 0 && py < ai.height && ai.board[py * ai.width + px] == 0);
        bool open_tail = (tx >= 0 && tx < ai.width && ty >= 0 && ty < ai.height && ai.board[ty * ai.width + tx] == 0);

        int val = 0;
        if (count >= 5) val = SCORE_WIN;
        else if (count == 4) val = (open_head && open_tail) ? W_LIVE_4 : (open_head || open_tail ? W_DEAD_4 : 0);
        else if (count == 3) val = (open_head && open_tail) ? W_LIVE_3 : (open_head || open_tail ? W_DEAD_3 : 0);
        else if (count == 2 && open_head && open_tail) val = W_LIVE_2;

        return (p == player) ? val : -val;
    };

    for (int y = sy; y <= ey; ++y) {
        for (int x = sx; x <= ex; ++x) {
            total_score += eval_line(x, y, 1, 0);
            total_score += eval_line(x, y, 0, 1);
            total_score += eval_line(x, y, 1, 1);
            total_score += eval_line(x, y, -1, 1);
        }
    }
    return total_score;
}

// --- MOVE ORDERING ---

// Scores a single move for sorting. Higher is better.
int score_move(const GomokuAI& ai, int idx, int player, int ply) {
    int score = 0;
    
    // 0. Killer Move Bonus
    if (killer_moves[ply][0] == idx) score += 50000;
    else if (killer_moves[ply][1] == idx) score += 40000;

    // 1. History Heuristic
    score += history_moves[player][idx];

    // 2. Centrality (Tie-breaker)
    int x = idx % ai.width;
    int y = idx / ai.width;
    int dist = std::abs(x - ai.width/2) + std::abs(y - ai.height/2);
    score -= dist * 10;

    // 3. Tactical Analysis (Immediate Threats)
    // "What if I play here?" vs "What if Opponent plays here?"
    
    int dx[] = {1, 0, 1, -1};
    int dy[] = {0, 1, 1, 1};
    int opp = (player == 1) ? 2 : 1;

    for (int k = 0; k < 4; ++k) {
        // My potential patterns (Attack)
        int my_count = 1;
        for (int i = 1; i < 5; ++i) {
            int nx = x + i * dx[k], ny = y + i * dy[k];
            if (nx < 0 || nx >= ai.width || ny < 0 || ny >= ai.height || ai.board[ny * ai.width + nx] != player) break;
            my_count++;
        }
        for (int i = 1; i < 5; ++i) {
            int nx = x - i * dx[k], ny = y - i * dy[k];
            if (nx < 0 || nx >= ai.width || ny < 0 || ny >= ai.height || ai.board[ny * ai.width + nx] != player) break;
            my_count++;
        }

        // Opponent potential patterns (Defense/Block)
        int opp_count = 1;
        for (int i = 1; i < 5; ++i) {
            int nx = x + i * dx[k], ny = y + i * dy[k];
            if (nx < 0 || nx >= ai.width || ny < 0 || ny >= ai.height || ai.board[ny * ai.width + nx] != opp) break;
            opp_count++;
        }
        for (int i = 1; i < 5; ++i) {
            int nx = x - i * dx[k], ny = y - i * dy[k];
            if (nx < 0 || nx >= ai.width || ny < 0 || ny >= ai.height || ai.board[ny * ai.width + nx] != opp) break;
            opp_count++;
        }

        // Weighting: Win > Block Win > Create 4 > Block 4
        if (my_count >= 5) score += 100000000;      // WIN NOW
        else if (opp_count >= 5) score += 90000000; // BLOCK WIN (Must do)
        else if (my_count == 4) score += 500000;    // Create 4
        else if (opp_count == 4) score += 400000;   // Block 4
        else if (my_count == 3) score += 10000;
        else if (opp_count == 3) score += 8000;
    }

    return score;
}

// Generates and sorts moves based on proximity to existing stones and heuristics
std::vector<std::pair<int, int>> get_sorted_moves(const GomokuAI& ai, int player, int ply, int best_tt_move = -1) {
    std::vector<std::pair<int, int>> moves;
    moves.reserve(64);

    int sx = std::max(0, ai.min_x - 2);
    int ex = std::min(ai.width - 1, ai.max_x + 2);
    int sy = std::max(0, ai.min_y - 2);
    int ey = std::min(ai.height - 1, ai.max_y + 2);
    int w = ai.width;

    for (int y = sy; y <= ey; ++y) {
        for (int x = sx; x <= ex; ++x) {
            int idx = y * w + x;
            if (ai.board[idx] != 0) continue;

            // Only consider moves within 2 distance of an existing stone
            bool neighbor_found = false;
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < w && ny >= 0 && ny < ai.height) {
                        if (ai.board[ny * w + nx] != 0) {
                            neighbor_found = true;
                            break;
                        }
                    }
                }
                if (neighbor_found) break;
            }

            if (neighbor_found) {
                int score = score_move(ai, idx, player, ply);
                if (idx == best_tt_move) score += 200000000; // PV move receives massive bonus
                moves.push_back({score, idx});
            }
        }
    }
    
    // Sort descending (best moves first)
    std::sort(moves.rbegin(), moves.rend());
    return moves;
}

// --- SEARCH ---

int negamax(GomokuAI& ai, int depth, int alpha, int beta, int player, int ply) {
    if (time_out_flag || check_time()) return TIMEOUT_SCORE;

    int opponent = (player == 1) ? 2 : 1;
    uint64_t key = ai.get_hash_key();
    TTEntry& tte = TT[key % TT_SIZE];

    if (tte.key == key && tte.depth >= depth) {
        if (tte.flag == 0) return tte.value;
        if (tte.flag == 1 && tte.value >= beta) return tte.value;
        if (tte.flag == 2 && tte.value <= alpha) return tte.value;
    }

    if (depth == 0) return eval_state(ai, player);

    int tt_move = (tte.key == key) ? tte.best_move_idx : -1;
    auto moves = get_sorted_moves(ai, player, ply, tt_move);

    if (moves.empty()) return eval_state(ai, player);

    int best_val = -INF;
    int best_move = -1;
    int flag = 2; // Upperbound

    for (const auto& mv : moves) {
        int idx = mv.second;
        
        ai.update_board(idx % ai.width, idx / ai.width, player);
        
        // Immediate win check optimization
        if (check_win(ai.board, idx, ai.width, ai.height, player)) {
            ai.update_board(idx % ai.width, idx / ai.width, 0);
            best_val = SCORE_WIN - ply; // Prefer faster wins
            best_move = idx;
            flag = 0; // Exact
            alpha = best_val;
            break; 
        }

        int val = -negamax(ai, depth - 1, -beta, -alpha, opponent, ply + 1);
        ai.update_board(idx % ai.width, idx / ai.width, 0);

        if (time_out_flag) return TIMEOUT_SCORE;

        if (val > best_val) {
            best_val = val;
            best_move = idx;
        }

        alpha = std::max(alpha, best_val);
        if (alpha >= beta) {
            flag = 1; // Lowerbound
            if (ply < 100) { // Update Killers
                killer_moves[ply][1] = killer_moves[ply][0];
                killer_moves[ply][0] = idx;
            }
            history_moves[player][idx] += depth * depth;
            break; 
        }
    }

    if (!time_out_flag) {
        tte.key = key;
        tte.depth = depth;
        tte.value = best_val;
        tte.flag = flag;
        tte.best_move_idx = best_move;
    }

    return best_val;
}

Point GomokuAI::find_best_move(int time_limit) {
    // 1. Initialization
    start_time = std::chrono::steady_clock::now();
    time_limit_ms = std::max(100, time_limit - 50); // Safety buffer
    guard_time_ms = std::min(4800, std::max(0, time_limit_ms - 200)); 
    nodes_visited = 0;
    time_out_flag = false;

    // Center start if empty
    int center_idx = (height / 2) * width + (width / 2);
    if (board[center_idx] == 0) {
        bool empty = true;
        for (int c : board) if (c != 0) { empty = false; break; }
        if (empty) return {width / 2, height / 2};
    }

    // 2. Safe Fallback Initialization (Depth 1 equivalent / Tactical)
    Point best_move_global = {-1, -1};
    
    // Quick scan for immediate winning/blocking moves (Depth 1 equivalent)
    auto initial_moves = get_sorted_moves(*this, 1, 0);
    if (!initial_moves.empty()) {
        best_move_global = {initial_moves[0].second % width, initial_moves[0].second / width};
    } else {
        // Should not happen unless board full
        for(int i=0; i<width*height; ++i) if(board[i]==0) return {i%width, i/width};
        return {0,0};
    }

    // 3. Iterative Deepening Loop
    int max_depth = 20;
    for (int depth = 1; depth <= max_depth; ++depth) {
        int best_val_this_depth = -INF;
        int best_move_idx_this_depth = -1;
        
        // Use consistent move ordering
        auto moves = get_sorted_moves(*this, 1, 0, -1);
        
        int alpha = -INF;
        int beta = INF;

        for (const auto& mv : moves) {
            int idx = mv.second;
            update_board(idx % width, idx / width, 1);

            // Win check
            if (check_win(board, idx, width, height, 1)) {
                update_board(idx % width, idx / width, 0);
                return {idx % width, idx / width}; // Return immediately on sure win
            }

            int val = -negamax(*this, depth - 1, -beta, -alpha, 2, 1);
            update_board(idx % width, idx / width, 0);

            // CRITICAL: Timeout Check
            if (time_out_flag || val == TIMEOUT_SCORE) {
                time_out_flag = true;
                break; // Break the move loop
            }

            if (val > best_val_this_depth) {
                best_val_this_depth = val;
                best_move_idx_this_depth = idx;
            }
            alpha = std::max(alpha, best_val_this_depth);
        }

        // CRITICAL: Fallback Logic
        if (time_out_flag) {
            // Do NOT update best_move_global
            // Break the ID loop
            break;
        } else {
            // Depth completed successfully, commit this move as the new best
            if (best_move_idx_this_depth != -1) {
                best_move_global = {best_move_idx_this_depth % width, best_move_idx_this_depth / width};
                
                // If we found a winning sequence, no need to search deeper
                if (best_val_this_depth >= SCORE_WIN - 1000) return best_move_global;
            }
        }
    }

    return best_move_global;
}