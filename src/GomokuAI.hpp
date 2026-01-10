#pragma once

#include <vector>
#include <string>
#include <cstdint>

struct Point {
    int x;
    int y;
};

class GomokuAI {
public:
    GomokuAI();
    void init(int size);
    void update_board(int x, int y, int player);
    Point find_best_move(int time_limit = 1000);
    Point parse_coordinates(const std::string& s);
    uint64_t get_hash_key() const { return hash_key; }

    int width;
    int height;
    std::vector<int> board; // 1D array: board[y * width + x]

    // Active bounds for optimization
    int min_x, max_x, min_y, max_y;

private:
    uint64_t hash_key = 0;
    std::vector<uint64_t> zobrist;

    void init_zobrist();
    uint64_t zobrist_at(int idx, int player) const;
};
