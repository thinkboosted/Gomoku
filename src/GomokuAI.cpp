#include "GomokuAI.hpp"
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cmath>

GomokuAI::GomokuAI() : width(20), height(20) {
    srand(static_cast<unsigned>(time(NULL)));
}

void GomokuAI::init(int size) {
    width = size;
    height = size;
    board.assign(width, std::vector<int>(height, 0));
}

void GomokuAI::update_board(int x, int y, int player) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        board[x][y] = player;
    }
}

Point GomokuAI::parse_coordinates(const std::string& s) {
    size_t commaPos = s.find(',');
    if (commaPos == std::string::npos) return {-1, -1};
    try {
        int x = std::stoi(s.substr(0, commaPos));
        int y = std::stoi(s.substr(commaPos + 1));
        return {x, y};
    } catch (...) {
        return {-1, -1};
    }
}

// Helper to count consecutive stones in a specific direction
int count_consecutive(const std::vector<std::vector<int>>& board, int x, int y, int dx, int dy, int player) {
    int count = 0;
    int w = board.size();
    int h = board[0].size();
    for (int i = 1; i < 5; ++i) {
        int nx = x + dx * i;
        int ny = y + dy * i;
        if (nx >= 0 && nx < w && ny >= 0 && ny < h && board[nx][ny] == player) count++;
        else break;
    }
    return count;
}

// Helper to evaluate a full line (forward and backward) through a point
int evaluate_dir(const std::vector<std::vector<int>>& board, int x, int y, int dx, int dy, int player) {
    return 1 + count_consecutive(board, x, y, dx, dy, player) + count_consecutive(board, x, y, -dx, -dy, player);
}

int GomokuAI::evaluate_position(int x, int y, int me, int opponent) {
    int score = 0;
    int dirs[4][2] = {{1,0}, {0,1}, {1,1}, {1,-1}};

    // We assign weights to patterns. 
    // Attack (me) is prioritized (1M) to ensure we take the win if available.
    // Defense (opponent) is high (500k) to force a block if they are about to win.
    for(auto& d : dirs) {
        // Offensive Score
        int my_len = evaluate_dir(board, x, y, d[0], d[1], me);
        if (my_len >= 5) return 1000000;
        if (my_len == 4) score += 10000;
        if (my_len == 3) score += 1000;
        if (my_len == 2) score += 100;

        // Defensive Score
        int op_len = evaluate_dir(board, x, y, d[0], d[1], opponent);
        if (op_len >= 5) return 500000;
        if (op_len == 4) score += 8000;
        if (op_len == 3) score += 500;
    }

    // Centrality heuristic: Playing near the center offers more growth potential in all directions.
    int cx = width / 2;
    int cy = height / 2;
    int dist = std::abs(x - cx) + std::abs(y - cy);
    score += (40 - dist);

    return score;
}

int GomokuAI::evaluate_line(int x, int y, int player) {
    int max_len = 0;
    int dirs[4][2] = {{1,0}, {0,1}, {1,1}, {1,-1}};
    for(auto& d : dirs) {
        int len = evaluate_dir(board, x, y, d[0], d[1], player);
        if (len > max_len) max_len = len;
    }
    return max_len;
}

Point GomokuAI::find_best_move() {
    int best_score = -1;
    Point best_move = {-1, -1};

    // Strategy: If the board is empty, the center is statistically the best start.
    bool empty = true;
    for(const auto& row : board) {
        for(int cell : row) {
            if(cell != 0) {
                empty = false;
                break;
            }
        }
        if(!empty) break;
    }
    if (empty) return {width/2, height/2};

    // Infer player IDs based on stone count to determine turn order
    int stones1 = 0;
    int stones2 = 0;
    for(const auto& row : board) {
        for(int cell : row) {
            if (cell == 1) stones1++;
            else if (cell == 2) stones2++;
        }
    }
    int me = (stones1 > stones2) ? 2 : 1;
    if (stones1 == stones2) me = 1;
    int opponent = (me == 1) ? 2 : 1;

    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            if (board[x][y] != 0) continue;

            int score = 0;

            // Priority 1: Win immediately
            if (evaluate_line(x, y, me) >= 5) score += 100000;

            // Priority 2: Block immediate loss
            if (evaluate_line(x, y, opponent) >= 5) score += 50000;

            // Priority 3: Strategic value (Open 4, Open 3, Center)
            score += evaluate_position(x, y, me, opponent);

            if (score > best_score) {
                best_score = score;
                best_move = {x, y};
            }
        }
    }

    // Fallback: If no clear strategic move is found, play closest to center to maintain pressure.
    if (best_move.x == -1) {
        int min_dist = 10000;
        for (int x = 0; x < width; ++x) {
            for (int y = 0; y < height; ++y) {
                if (board[x][y] == 0) {
                    int dist = std::abs(x - width/2) + std::abs(y - height/2);
                    if (dist < min_dist) {
                        min_dist = dist;
                        best_move = {x, y};
                    }
                }
            }
        }
    }

    return best_move;
}