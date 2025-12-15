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
    Point find_best_move();
    Point parse_coordinates(const std::string& s);
    uint64_t get_hash_key() const { return hash_key; }

    int width;
    int height;
    std::vector<std::vector<int>> board; 

    // Heuristic Evaluation Methods
    int evaluate_position(int x, int y, int me, int opponent);
    int evaluate_line(int x, int y, int player);

private:
    uint64_t hash_key = 0;
    std::vector<uint64_t> zobrist; // width * height * 3 (player 0..2)

    void init_zobrist();
    uint64_t zobrist_at(int x, int y, int player) const;
};