#include "GomokuAI.hpp"
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cmath>
#include <array>

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

struct LineStats {
    int count;
    bool open1;
    bool open2;
};

// Collect how many aligned stones we would have by playing (x,y) and whether each end stays open.
LineStats get_line_stats(const std::vector<std::vector<int>>& board, int x, int y, int dx, int dy, int player) {
    int w = board.size();
    int h = board[0].size();

    int count = 1; // include the stone we are about to play
    bool open1 = false;
    bool open2 = false;

    int nx = x + dx;
    int ny = y + dy;
    while (nx >= 0 && nx < w && ny >= 0 && ny < h && board[nx][ny] == player) {
        count++;
        nx += dx;
        ny += dy;
    }
    if (nx >= 0 && nx < w && ny >= 0 && ny < h && board[nx][ny] == 0) open1 = true;

    nx = x - dx;
    ny = y - dy;
    while (nx >= 0 && nx < w && ny >= 0 && ny < h && board[nx][ny] == player) {
        count++;
        nx -= dx;
        ny -= dy;
    }
    if (nx >= 0 && nx < w && ny >= 0 && ny < h && board[nx][ny] == 0) open2 = true;

    return {count, open1, open2};
}

int pattern_score(const LineStats& ls) {
    // Strongly reward open ended shapes, modestly reward closed shapes.
    if (ls.count >= 5) return 1'000'000'000; // instant win
    if (ls.count == 4 && ls.open1 && ls.open2) return 200'000; // open four
    if (ls.count == 4 && (ls.open1 || ls.open2)) return 50'000;  // closed four
    if (ls.count == 3 && ls.open1 && ls.open2) return 15'000; // open three (threat to become four)
    if (ls.count == 3 && (ls.open1 || ls.open2)) return 2'000;  // closed three
    if (ls.count == 2 && ls.open1 && ls.open2) return 800;
    if (ls.count == 2 && (ls.open1 || ls.open2)) return 150;
    return (ls.open1 || ls.open2) ? 40 : 10;
}

// Detect "split" threats like XX.XX or XX.X that do not look like an immediate 4 but are very strong next-turn attacks.
int gapped_threat_score(const std::vector<std::vector<int>>& board, int x, int y, int dx, int dy, int player) {
    auto cell = [&](int offset) {
        if (offset == 0) return player; // assume we play here
        int nx = x + offset * dx;
        int ny = y + offset * dy;
        if (nx < 0 || nx >= static_cast<int>(board.size()) || ny < 0 || ny >= static_cast<int>(board[0].size())) return -1; // blocked by border
        return board[nx][ny];
    };

    int best = 0;
    for (int start = -4; start <= 0; ++start) {
        int stones = 0;
        int segments = 0;
        int max_seg = 0;
        int empties = 0;
        bool blocked = false;
        int cur = 0;

        for (int i = 0; i < 5; ++i) {
            int v = cell(start + i);
            if (v == player) {
                stones++;
                cur++;
            } else if (v == 0) {
                empties++;
                if (cur > 0) {
                    segments++;
                    if (cur > max_seg) max_seg = cur;
                    cur = 0;
                }
            } else { // opponent or border
                blocked = true;
                if (cur > 0) {
                    segments++;
                    if (cur > max_seg) max_seg = cur;
                    cur = 0;
                }
            }
        }
        if (cur > 0) {
            segments++;
            if (cur > max_seg) max_seg = cur;
        }

        if (blocked) continue;          // invalid window (opponent/border)
        if (empties == 0) continue;     // no gap present: ignore, avoids false positives when the gap is déjà occupée

        // Require at least one open end outside the 5-cell window; if both ends are border/our stones, the threat is not urgent.
        auto open_end = [&](int offset) {
            int nx = x + offset * dx;
            int ny = y + offset * dy;
            if (nx < 0 || nx >= static_cast<int>(board.size()) || ny < 0 || ny >= static_cast<int>(board[0].size())) return false; // border counts as closed
            return board[nx][ny] == 0;
        };
        bool has_open_end = open_end(start - 1) || open_end(start + 5);
        if (!has_open_end) continue;

        // Reward non-contiguous threats that rely on at least one real gap.
        if (stones == 4 && max_seg <= 2 && segments >= 2) {
            best = std::max(best, 60000); // XX.XX style hidden four
        } else if (stones == 3 && max_seg <= 2 && segments >= 2) {
            best = std::max(best, 2500); // XX.X or X.XX split three
        }
    }
    return best;
}

int GomokuAI::evaluate_position(int x, int y, int me, int opponent) {
    static const std::array<std::array<int,2>,4> dirs = {std::array<int,2>{1,0}, {0,1}, {1,1}, {1,-1}};

    int score = 0;
    int dir_attack_scores[4] = {0,0,0,0};

    // Attack: how strong we become if we play here.
    for (int i = 0; i < 4; ++i) {
        auto& d = dirs[i];
        LineStats ls = get_line_stats(board, x, y, d[0], d[1], me);
        int s = pattern_score(ls) + gapped_threat_score(board, x, y, d[0], d[1], me);
        dir_attack_scores[i] = s;
        score += s;
    }

    // Defense: how much danger we neutralize from the opponent by occupying this point.
    for (auto& d : dirs) {
        LineStats ls_opp = get_line_stats(board, x, y, d[0], d[1], opponent);
        // A blocking move both prevents their pattern and often creates ours; keep weight high.
        score += pattern_score(ls_opp) * 9 / 10;
        score += gapped_threat_score(board, x, y, d[0], d[1], opponent) * 8 / 10;
    }

    // Threat multiplicity: bonus when the move is strong in two directions (fork potential).
    std::array<int,4> tmp = {dir_attack_scores[0], dir_attack_scores[1], dir_attack_scores[2], dir_attack_scores[3]};
    std::sort(tmp.begin(), tmp.end(), std::greater<int>());
    int fork_bonus = (tmp[0] + tmp[1]) / 3; // lighter than raw sum to avoid overpowering
    score += fork_bonus;

    // Proximity: avoid isolated plays; prefer to stay within two cells of any stone.
    int neighbor_score = 0;
    for (int dx = -2; dx <= 2; ++dx) {
        for (int dy = -2; dy <= 2; ++dy) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx;
            int ny = y + dy;
            if (nx >= 0 && nx < width && ny >= 0 && ny < height && board[nx][ny] != 0) {
                int dist = std::abs(dx) + std::abs(dy);
                neighbor_score += (5 - dist); // closer neighbors give more weight
            }
        }
    }
    score += neighbor_score * 15;

    // Centrality heuristic: Playing near the center offers more growth potential in all directions.
    int cx = width / 2;
    int cy = height / 2;
    int dist = std::abs(x - cx) + std::abs(y - cy);
    score += (200 - dist * 5);

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

    // Player IDs are fixed by protocol: we are 1, opponent is 2.
    const int me = 1;
    const int opponent = 2;

    // Limit search to a small band around existing stones to avoid useless far moves.
    int min_x = width, max_x = -1, min_y = height, max_y = -1;
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            if (board[x][y] != 0) {
                min_x = std::min(min_x, x);
                max_x = std::max(max_x, x);
                min_y = std::min(min_y, y);
                max_y = std::max(max_y, y);
            }
        }
    }

    int margin = 2;
    int start_x = std::max(0, min_x - margin);
    int end_x = std::min(width - 1, max_x + margin);
    int start_y = std::max(0, min_y - margin);
    int end_y = std::min(height - 1, max_y + margin);

    // If the board somehow appears empty (fallback), search all cells.
    if (max_x == -1) {
        start_x = 0; end_x = width - 1; start_y = 0; end_y = height - 1;
    }

    auto would_win = [&](int px, int py, int player) {
        board[px][py] = player;
        bool win = (evaluate_line(px, py, player) >= 5);
        board[px][py] = 0;
        return win;
    };

    // Threat search (shallow): find a move that guarantees a win next turn regardless of opponent reply.
    for (int x = start_x; x <= end_x; ++x) {
        for (int y = start_y; y <= end_y; ++y) {
            if (board[x][y] != 0) continue;

            // Immediate win already caught.
            if (would_win(x, y, me)) return Point{x, y};

            // Simulate our move.
            board[x][y] = me;

            bool forced = true;
            int opp_start_x = std::max(0, x - margin);
            int opp_end_x   = std::min(width - 1, x + margin);
            int opp_start_y = std::max(0, y - margin);
            int opp_end_y   = std::min(height - 1, y + margin);

            for (int ox = opp_start_x; ox <= opp_end_x && forced; ++ox) {
                for (int oy = opp_start_y; oy <= opp_end_y && forced; ++oy) {
                    if (board[ox][oy] != 0) continue;

                    // If opponent can win directly, our move is bad.
                    if (would_win(ox, oy, opponent)) { forced = false; break; }

                    // Opponent plays (ox,oy), can we still win immediately?
                    board[ox][oy] = opponent;
                    bool can_reply_win = false;
                    int my_start_x = std::max(0, ox - margin);
                    int my_end_x   = std::min(width - 1, ox + margin);
                    int my_start_y = std::max(0, oy - margin);
                    int my_end_y   = std::min(height - 1, oy + margin);

                    for (int rx = my_start_x; rx <= my_end_x && !can_reply_win; ++rx) {
                        for (int ry = my_start_y; ry <= my_end_y && !can_reply_win; ++ry) {
                            if (board[rx][ry] != 0) continue;
                            if (would_win(rx, ry, me)) {
                                can_reply_win = true;
                            }
                        }
                    }
                    board[ox][oy] = 0;
                    if (!can_reply_win) forced = false;
                }
            }

            board[x][y] = 0;
            if (forced) return Point{x, y};
        }
    }

    for (int x = start_x; x <= end_x; ++x) {
        for (int y = start_y; y <= end_y; ++y) {
            if (board[x][y] != 0) continue;

            // Priority 1: Winning move
            if (evaluate_line(x, y, me) >= 5) return {x, y};

            // Priority 2: Block opponent's win
            if (evaluate_line(x, y, opponent) >= 5) {
                int block_score = 300'000; // ensure blocks beat heuristic plays
                if (block_score > best_score) {
                    best_score = block_score;
                    best_move = {x, y};
                }
                continue;
            }

            int score = evaluate_position(x, y, me, opponent);
            if (score > best_score) {
                best_score = score;
                best_move = {x, y};
            }
        }
    }

    // Fallback: play closest to center if nothing else is chosen.
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