#include "GomokuAI.hpp"
#include <algorithm>
#include <cmath>
#include <array>
#include <chrono>
#include <unordered_map>
#include <cstdint>

GomokuAI::GomokuAI() : width(20), height(20) {
}

namespace {

static uint64_t splitmix64(uint64_t& x) {
    uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

} // namespace

void GomokuAI::init_zobrist() {
    zobrist.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 3ULL, 0ULL);
    uint64_t seed = 0xC0FFEE123456789ULL ^ static_cast<uint64_t>(width * 1315423911u + height * 2654435761u);
    for (size_t i = 0; i < zobrist.size(); ++i) {
        zobrist[i] = splitmix64(seed);
    }
}

uint64_t GomokuAI::zobrist_at(int x, int y, int player) const {
    return zobrist[(static_cast<size_t>(x) * static_cast<size_t>(height) + static_cast<size_t>(y)) * 3ULL + static_cast<size_t>(player)];
}

void GomokuAI::init(int size) {
    width = size;
    height = size;
    board.assign(width, std::vector<int>(height, 0));
    init_zobrist();
    hash_key = 0;
}

void GomokuAI::update_board(int x, int y, int player) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        int prev = board[x][y];
        if (prev == player) return;
        if (prev >= 0 && prev <= 2) hash_key ^= zobrist_at(x, y, prev);
        if (player >= 0 && player <= 2) hash_key ^= zobrist_at(x, y, player);
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

namespace detail {

// Count aligned stones for `player` in one direction (excluding origin), up to 4 steps.
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

// Evaluate the length of a line through (x,y) by looking both directions (includes the move).
int evaluate_dir(const std::vector<std::vector<int>>& board, int x, int y, int dx, int dy, int player) {
    return 1 + count_consecutive(board, x, y, dx, dy, player) + count_consecutive(board, x, y, -dx, -dy, player);
}

// Compute max line length through (x,y) for `player` across 4 directions.
int evaluate_line_local(const std::vector<std::vector<int>>& board, int x, int y, int player) {
    int max_len = 0;
    int dirs[4][2] = {{1,0}, {0,1}, {1,1}, {1,-1}};
    for(auto& d : dirs) {
        int len = evaluate_dir(board, x, y, d[0], d[1], player);
        if (len > max_len) max_len = len;
    }
    return max_len;
}

// Aggregated info for a line: stones count and openness at each end.
struct LineStats {
    int count;
    bool open1;
    bool open2;
};

// Search bounds for pruning: window around existing stones.
struct Bounds {
    int start_x;
    int end_x;
    int start_y;
    int end_y;
};

struct Move {
    int x;
    int y;
    int score;
};

// Check if coordinates are inside board limits.
bool is_inside(const std::vector<std::vector<int>>& board, int x, int y) {
    return x >= 0 && y >= 0 && x < static_cast<int>(board.size()) && y < static_cast<int>(board[0].size());
}

// True if no stones are placed.
bool board_is_empty(const std::vector<std::vector<int>>& board) {
    for (const auto& row : board) {
        for (int cell : row) if (cell != 0) return false;
    }
    return true;
}

int count_stones(const std::vector<std::vector<int>>& board) {
    int stones = 0;
    for (const auto& row : board) {
        for (int cell : row) stones += (cell != 0);
    }
    return stones;
}

// Compute a rectangular window covering all stones, expanded by `margin`.
Bounds compute_bounds(const std::vector<std::vector<int>>& board, int margin) {
    int w = board.size();
    int h = board.empty() ? 0 : board[0].size();
    int min_x = w, max_x = -1, min_y = h, max_y = -1;
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) {
            if (board[x][y] != 0) {
                min_x = std::min(min_x, x);
                max_x = std::max(max_x, x);
                min_y = std::min(min_y, y);
                max_y = std::max(max_y, y);
            }
        }
    }

    Bounds b;
    if (max_x == -1) { // empty board fallback
        b.start_x = 0; b.end_x = w - 1; b.start_y = 0; b.end_y = h - 1;
    } else {
        b.start_x = std::max(0, min_x - margin);
        b.end_x   = std::min(w - 1, max_x + margin);
        b.start_y = std::max(0, min_y - margin);
        b.end_y   = std::min(h - 1, max_y + margin);
    }
    return b;
}

Bounds expand_bounds(const Bounds& b, int expand, int width, int height) {
    Bounds out;
    out.start_x = std::max(0, b.start_x - expand);
    out.end_x = std::min(width - 1, b.end_x + expand);
    out.start_y = std::max(0, b.start_y - expand);
    out.end_y = std::min(height - 1, b.end_y + expand);
    return out;
}

// Side-to-move salt for TT keys (avoid collisions between same position with different side).
constexpr uint64_t TT_SIDE_P1 = 0xA0761D6478BD642FULL;
constexpr uint64_t TT_SIDE_P2 = 0xE7037ED1A0B428DBULL;

struct TTEntry {
    int depth;
    int value;
    int best_x;
    int best_y;
};

} // namespace detail

namespace {

constexpr int INF = 1'000'000'000;
constexpr int WIN_SCORE = 900'000'000;

inline uint64_t tt_key(uint64_t base, int player_to_move) {
    return base ^ (player_to_move == 1 ? detail::TT_SIDE_P1 : detail::TT_SIDE_P2);
}

} // namespace

namespace detail {

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

// Score contiguous patterns (open/closed 2/3/4/5) for attack/defense.
int pattern_score(const LineStats& ls) {
    if (ls.count >= 5) return 1'000'000'000; // instant win
    if (ls.count == 4 && ls.open1 && ls.open2) return 200'000; // open four
    if (ls.count == 4 && (ls.open1 || ls.open2)) return 50'000;  // closed four
    if (ls.count == 3 && ls.open1 && ls.open2) return 15'000; // open three (threat to become four)
    if (ls.count == 3 && (ls.open1 || ls.open2)) return 2'000;  // closed three
    if (ls.count == 2 && ls.open1 && ls.open2) return 800;
    if (ls.count == 2 && (ls.open1 || ls.open2)) return 150;
    return (ls.open1 || ls.open2) ? 40 : 10;
}

// Score split threats (XX.XX, XX.X) requiring a real gap and at least one open end.
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

// Encourage distant, open-ended pairings like X000.X that keep both ends flexible and can mislead opponents.
int spaced_extension_bonus(const std::vector<std::vector<int>>& board, int x, int y, int dx, int dy, int player) {
    int best = 0;

    auto scan_dir = [&](int step_dir) {
        // Keep this coherent with our typical search window (margin 2/3): gap=4 rarely falls inside bounds.
        for (int gap = 3; gap <= 3; ++gap) { // 3 steps away: X00X
            int nx = x + step_dir * gap * dx;
            int ny = y + step_dir * gap * dy;
            if (!is_inside(board, nx, ny)) continue;
            if (board[nx][ny] != player) continue;

            bool clear = true;
            for (int i = 1; i < gap; ++i) {
                int cx = x + step_dir * i * dx;
                int cy = y + step_dir * i * dy;
                if (!is_inside(board, cx, cy) || board[cx][cy] != 0) { clear = false; break; }
            }
            if (!clear) continue;

            bool open_far = is_inside(board, nx + step_dir * dx, ny + step_dir * dy) && board[nx + step_dir * dx][ny + step_dir * dy] == 0;
            bool open_near = is_inside(board, x - step_dir * dx, y - step_dir * dy) && board[x - step_dir * dx][y - step_dir * dy] == 0;

            // Keep the far end open to really form X000.X; otherwise it's just a buried gap inside our own chain.
            if (!open_far) continue;

            int base = 250;
            if (open_near) base += 50; // extra value when both ends stay open
            best = std::max(best, base);
        }
    };

    scan_dir(1);
    scan_dir(-1);
    return best;
}

// Find a move that guarantees an immediate win on the next turn, regardless of opponent reply (shallow threat search).
Point threat_search_forced_win(std::vector<std::vector<int>>& board, const Bounds& bounds, int margin, int me, int opponent, int width, int height) {
    (void)margin;
    (void)width;
    (void)height;
    auto would_win = [&](int px, int py, int player) {
        board[px][py] = player;
        bool win = (evaluate_line_local(board, px, py, player) >= 5);
        board[px][py] = 0;
        return win;
    };

    for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
        for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
            if (board[x][y] != 0) continue;

            if (would_win(x, y, me)) return Point{x, y};

            board[x][y] = me;

            bool forced = true;

            // IMPORTANT: Do not restrict opponent replies to a tiny local window.
            // Doing so creates false positives ("forced" sequences) and makes the AI over-defend.
            // Using the global search bounds keeps it fast while capturing key defenses/blocks.
            for (int ox = bounds.start_x; ox <= bounds.end_x && forced; ++ox) {
                for (int oy = bounds.start_y; oy <= bounds.end_y && forced; ++oy) {
                    if (board[ox][oy] != 0) continue;

                    if (would_win(ox, oy, opponent)) { forced = false; break; }

                    board[ox][oy] = opponent;
                    bool can_reply_win = false;

                    // Same rationale: our winning reply might be slightly outside a local window.
                    for (int rx = bounds.start_x; rx <= bounds.end_x && !can_reply_win; ++rx) {
                        for (int ry = bounds.start_y; ry <= bounds.end_y && !can_reply_win; ++ry) {
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
    return Point{-1, -1};
}

} // namespace detail

namespace {

static std::vector<detail::Move> generate_candidates(
    GomokuAI& ai,
    const detail::Bounds& bounds,
    int player,
    int opponent,
    int max_moves)
{
    std::vector<detail::Move> moves;
    moves.reserve(static_cast<size_t>(max_moves) * 2ULL);

    for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
        for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
            if (ai.board[x][y] != 0) continue;

            int s = ai.evaluate_position(x, y, player, opponent);
            int self_len = detail::evaluate_line_local(ai.board, x, y, player);
            int opp_len = detail::evaluate_line_local(ai.board, x, y, opponent);
            if (self_len >= 5) s += WIN_SCORE;
            if (opp_len >= 5) s += WIN_SCORE / 2; // urgent block
            moves.push_back({x, y, s});
        }
    }

    std::sort(moves.begin(), moves.end(), [](const detail::Move& a, const detail::Move& b) {
        return a.score > b.score;
    });

    if (static_cast<int>(moves.size()) > max_moves) moves.resize(static_cast<size_t>(max_moves));
    return moves;
}

static int evaluate_state_fixed(
    GomokuAI& ai,
    const detail::Bounds& bounds,
    int root_me,
    int root_opp)
{
    // Cheap evaluation: best move potential for player minus best move potential for opponent.
    // Keep it small and stable for alpha-beta.
    auto self_moves = generate_candidates(ai, bounds, root_me, root_opp, 10);
    auto opp_moves = generate_candidates(ai, bounds, root_opp, root_me, 10);

    int best_self = self_moves.empty() ? 0 : self_moves.front().score;
    int best_opp = opp_moves.empty() ? 0 : opp_moves.front().score;

    // If root opponent has an immediate win threat, reflect it strongly.
    for (const auto& m : opp_moves) {
        if (detail::evaluate_line_local(ai.board, m.x, m.y, root_opp) >= 5) {
            return -WIN_SCORE / 2;
        }
    }

    return best_self - (best_opp * 9 / 10);
}

static int negamax(
    GomokuAI& ai,
    const detail::Bounds& bounds,
    int depth,
    int alpha,
    int beta,
    int player_to_move,
    int opponent,
    int root_me,
    int root_opp,
    std::unordered_map<uint64_t, detail::TTEntry>& tt,
    const std::chrono::steady_clock::time_point& deadline,
    bool& time_up)
{
    if (time_up || std::chrono::steady_clock::now() >= deadline) {
        time_up = true;
        return 0;
    }

    if (depth == 0) {
        int eval = evaluate_state_fixed(ai, bounds, root_me, root_opp);
        int color = (player_to_move == root_me) ? 1 : -1;
        return color * eval;
    }

    uint64_t key = tt_key(ai.get_hash_key(), player_to_move);
    auto it = tt.find(key);
    if (it != tt.end() && it->second.depth >= depth) {
        return it->second.value;
    }

    int best_value = -INF;
    int best_x = -1;
    int best_y = -1;

    auto moves = generate_candidates(ai, bounds, player_to_move, opponent, 12);
    if (moves.empty()) {
        int eval = evaluate_state_fixed(ai, bounds, root_me, root_opp);
        int color = (player_to_move == root_me) ? 1 : -1;
        return color * eval;
    }

    for (const auto& mv : moves) {
        if (std::chrono::steady_clock::now() >= deadline) {
            time_up = true;
            break;
        }

        // Apply move
        ai.update_board(mv.x, mv.y, player_to_move);

        int value;
        if (detail::evaluate_line_local(ai.board, mv.x, mv.y, player_to_move) >= 5) {
            value = WIN_SCORE - (5 - depth);
        } else {
            value = -negamax(ai, bounds, depth - 1, -beta, -alpha, opponent, player_to_move, root_me, root_opp, tt, deadline, time_up);
        }

        // Undo
        ai.update_board(mv.x, mv.y, 0);
        if (time_up) break;

        if (value > best_value) {
            best_value = value;
            best_x = mv.x;
            best_y = mv.y;
        }
        alpha = std::max(alpha, value);
        if (alpha >= beta) break;
    }

    if (!time_up && best_x != -1) {
        tt[key] = detail::TTEntry{depth, best_value, best_x, best_y};
    }
    return best_value;
}

static Point search_best_move(
    GomokuAI& ai,
    const detail::Bounds& base_bounds,
    int me,
    int opponent,
    int time_limit_ms)
{
    detail::Bounds bounds = detail::expand_bounds(base_bounds, 1, ai.width, ai.height);
    auto start = std::chrono::steady_clock::now();
    auto deadline = start + std::chrono::milliseconds(time_limit_ms);

    std::unordered_map<uint64_t, detail::TTEntry> tt;
    tt.reserve(200'000);

    auto root_moves = generate_candidates(ai, bounds, me, opponent, 24);
    if (root_moves.empty()) return {-1, -1};

    Point best = {root_moves.front().x, root_moves.front().y};
    int best_val = -INF;

    bool time_up = false;
    for (int depth = 1; depth <= 5; ++depth) {
        if (std::chrono::steady_clock::now() >= deadline) break;

        int alpha = -INF;
        int beta = INF;
        int local_best_val = -INF;
        Point local_best = best;

        // Try principal variation move first (from previous iteration)
        std::stable_sort(root_moves.begin(), root_moves.end(), [&](const detail::Move& a, const detail::Move& b) {
            if (a.x == best.x && a.y == best.y) return true;
            if (b.x == best.x && b.y == best.y) return false;
            return a.score > b.score;
        });

        for (const auto& mv : root_moves) {
            if (std::chrono::steady_clock::now() >= deadline) {
                time_up = true;
                break;
            }

            ai.update_board(mv.x, mv.y, me);

            int value;
            if (detail::evaluate_line_local(ai.board, mv.x, mv.y, me) >= 5) {
                value = WIN_SCORE;
            } else {
                value = -negamax(ai, bounds, depth - 1, -beta, -alpha, opponent, me, me, opponent, tt, deadline, time_up);
            }

            ai.update_board(mv.x, mv.y, 0);
            if (time_up) break;

            if (value > local_best_val) {
                local_best_val = value;
                local_best = {mv.x, mv.y};
            }
            alpha = std::max(alpha, value);
        }

        if (!time_up) {
            best = local_best;
            best_val = local_best_val;
        } else {
            break;
        }
    }

    (void)best_val;
    return best;
}

} // namespace

int GomokuAI::evaluate_position(int x, int y, int me, int opponent) {
    // Heuristic mix: attack + defense + fork bonus + proximity + center bias.
    static const std::array<std::array<int,2>,4> dirs = {std::array<int,2>{1,0}, {0,1}, {1,1}, {1,-1}};

    int dir_attack_scores[4] = {0,0,0,0};
    int attack_total = 0;
    int defense_total = 0;
    bool meaningful_attack = false;
    bool meaningful_defense = false;

    // Detect whether a blocked end is specifically blocked by an adjacent opponent stone.
    auto blocked_by_adjacent_opponent = [&](int dx, int dy, bool forward) {
        int step_x = forward ? dx : -dx;
        int step_y = forward ? dy : -dy;
        int nx = x + step_x;
        int ny = y + step_y;
        while (nx >= 0 && nx < width && ny >= 0 && ny < height && board[nx][ny] == me) {
            nx += step_x;
            ny += step_y;
        }
        if (nx < 0 || nx >= width || ny < 0 || ny >= height) return false; // border is not “collé adversaire”
        return board[nx][ny] == opponent;
    };

    // Attack: how strong we become if we play here.
    int threat_dirs = 0; // count directions that create an immediate threat (open three or better)
    for (int i = 0; i < 4; ++i) {
        auto& d = dirs[i];
        detail::LineStats ls = detail::get_line_stats(board, x, y, d[0], d[1], me);
        int pattern = detail::pattern_score(ls);
        int gapped = detail::gapped_threat_score(board, x, y, d[0], d[1], me);
        int spaced = detail::spaced_extension_bonus(board, x, y, d[0], d[1], me);

        int s = pattern + gapped + spaced;

        // Track how many directions yield at least an open three / split three (for double-threat bonus).
        if (pattern >= 15'000 || gapped >= 60'000) {
            threat_dirs++;
        }

        // If playing here only makes a closed four whose blocked side is an adjacent opponent stone,
        // downweight it (opponent already sits on the only exit). Prefer other openings unless forked.
        if (ls.count == 4 && (ls.open1 ^ ls.open2)) {
            bool blocked_forward = (!ls.open1) && blocked_by_adjacent_opponent(d[0], d[1], true);
            bool blocked_backward = (!ls.open2) && blocked_by_adjacent_opponent(d[0], d[1], false);
            if (blocked_forward || blocked_backward) {
                s = std::min(s, 8'000); // favor autre ouverture sauf si compensé par un fork
            }
        }
        if (!meaningful_attack && (pattern >= 2'000 || gapped > 0 || spaced > 0)) {
            meaningful_attack = true;
        }
        dir_attack_scores[i] = s;
        attack_total += s;
    }

    // Defense: how much danger we neutralize from the opponent by occupying this point.
    for (auto& d : dirs) {
        detail::LineStats ls_opp = detail::get_line_stats(board, x, y, d[0], d[1], opponent);
        int p = detail::pattern_score(ls_opp);
        int g = detail::gapped_threat_score(board, x, y, d[0], d[1], opponent);
        // A blocking move both prevents their pattern and often creates ours; keep weight high.
        defense_total += p * 9 / 10;
        defense_total += g * 8 / 10;
        if (!meaningful_defense && (p >= 2'000 || g > 0)) {
            meaningful_defense = true;
        }
    }

    // Threat multiplicity: bonus when the move is strong in two directions (fork potential).
    std::array<int,4> tmp = {dir_attack_scores[0], dir_attack_scores[1], dir_attack_scores[2], dir_attack_scores[3]};
    std::sort(tmp.begin(), tmp.end(), std::greater<int>());
    int fork_bonus = (tmp[0] + tmp[1]) / 3; // lighter than raw sum to avoid overpowering
    int extra_score = fork_bonus;

    // Bonus for creating multiple immediate threats (double open three / split threat).
    if (threat_dirs >= 2) {
        extra_score += 40'000;
    } else if (threat_dirs == 1) {
        extra_score += 8'000;
    }

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
    neighbor_score = std::min(neighbor_score, 12);
    extra_score += neighbor_score * 10;

    // Centrality heuristic: Playing near the center offers more growth potential in all directions.
    // Scale down this effect as the game progresses.
    int stones = detail::count_stones(board);
    int cx = width / 2;
    int cy = height / 2;
    int dist = std::abs(x - cx) + std::abs(y - cy);
    int center_weight = (stones < 6 ? 120 : 40);
    int center_bonus = center_weight - dist * 4;
    if (center_bonus > 0) extra_score += center_bonus;

    // If the move neither creates nor blocks at least a (closed) three or gapped threat,
    // downweight soft bonuses so we avoid neutral filler moves.
    if (!meaningful_attack && !meaningful_defense) {
        extra_score /= 2;
    }

    return attack_total + defense_total + extra_score;
}

int GomokuAI::evaluate_line(int x, int y, int player) {
    return detail::evaluate_line_local(board, x, y, player);
}

Point GomokuAI::find_best_move() {
    // Main policy: center on empty board, forced-win scan, then heuristic best.
    int best_score = -1;
    Point best_move = {-1, -1};

    // Strategy: If the board is empty, the center is statistically the best start.
    if (detail::board_is_empty(board)) return {width/2, height/2};

    // Player IDs are fixed by protocol: we are 1, opponent is 2.
    const int me = 1;
    const int opponent = 2;
    static const std::array<std::array<int,2>,4> dirs = {std::array<int,2>{1,0}, {0,1}, {1,1}, {1,-1}};

    // Use a slightly larger margin on larger boards to not miss distant threats.
    int margin = (width > 12 ? 3 : 2);
    detail::Bounds b = detail::compute_bounds(board, margin);
    detail::Bounds guard_b = detail::expand_bounds(b, 1, width, height);

    // Mandatory tactics first: never let a deeper heuristic/threat-search override these.
    // 1) Win now
    for (int x = b.start_x; x <= b.end_x; ++x) {
        for (int y = b.start_y; y <= b.end_y; ++y) {
            if (board[x][y] != 0) continue;
            if (evaluate_line(x, y, me) >= 5) return {x, y};
        }
    }

    // 2) Block opponent win now
    for (int x = b.start_x; x <= b.end_x; ++x) {
        for (int y = b.start_y; y <= b.end_y; ++y) {
            if (board[x][y] != 0) continue;
            if (evaluate_line(x, y, opponent) >= 5) return {x, y};
        }
    }

    // 3) Block opponent hidden fours (urgent)
    for (int x = b.start_x; x <= b.end_x; ++x) {
        for (int y = b.start_y; y <= b.end_y; ++y) {
            if (board[x][y] != 0) continue;
            int opp_gapped = 0;
            for (auto& d : dirs) {
                opp_gapped = std::max(opp_gapped, detail::gapped_threat_score(board, x, y, d[0], d[1], opponent));
            }
            if (opp_gapped >= 60'000) return {x, y};
        }
    }

    // 4) Create our own open/hidden four: in practice this is a winning race condition.
    // This prevents the AI from blocking an opponent open three when we can instead
    // create an unavoidable threat (open four / XX.XX).
    for (int x = b.start_x; x <= b.end_x; ++x) {
        for (int y = b.start_y; y <= b.end_y; ++y) {
            if (board[x][y] != 0) continue;
            int my_pattern = 0;
            int my_gapped = 0;
            for (auto& d : dirs) {
                detail::LineStats ls_me = detail::get_line_stats(board, x, y, d[0], d[1], me);
                my_pattern = std::max(my_pattern, detail::pattern_score(ls_me));
                my_gapped = std::max(my_gapped, detail::gapped_threat_score(board, x, y, d[0], d[1], me));
            }
            if (my_pattern >= 200'000 || my_gapped >= 60'000) return {x, y};
        }
    }

    // Threat search (2-ply) after mandatory defense.
    Point forced = detail::threat_search_forced_win(board, b, margin, me, opponent, width, height);
    if (forced.x != -1) return forced;

    // Defensive threat search: if the opponent has a short forced win line, occupy its start.
    Point opp_forced = detail::threat_search_forced_win(board, b, margin, opponent, me, width, height);
    if (opp_forced.x != -1 && board[opp_forced.x][opp_forced.y] == 0) {
        return opp_forced;
    }

    // Main improvement: iterative-deepening alpha-beta search (time-bounded).
    // Keep budget conservative to avoid timeouts on dense boards.
    if (detail::count_stones(board) >= 10) {
        constexpr int SEARCH_TIME_MS = 1200;
        Point p = search_best_move(*this, b, me, opponent, SEARCH_TIME_MS);
        if (p.x != -1) return p;
    }

    for (int x = b.start_x; x <= b.end_x; ++x) {
        for (int y = b.start_y; y <= b.end_y; ++y) {
            if (board[x][y] != 0) continue;

            int score = evaluate_position(x, y, me, opponent);

            // Tactical guard: avoid playing a move that allows an immediate opponent win.
            // This also helps when bounds are tight and a winning reply is just outside b.
            board[x][y] = me;
            bool gives_immediate_win = false;
            for (int ox = guard_b.start_x; ox <= guard_b.end_x && !gives_immediate_win; ++ox) {
                for (int oy = guard_b.start_y; oy <= guard_b.end_y && !gives_immediate_win; ++oy) {
                    if (board[ox][oy] != 0) continue;
                    if (detail::evaluate_line_local(board, ox, oy, opponent) >= 5) {
                        gives_immediate_win = true;
                    }
                }
            }
            board[x][y] = 0;
            if (gives_immediate_win) score -= 500'000;

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