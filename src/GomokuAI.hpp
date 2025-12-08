#pragma once

#include <vector>
#include <string>

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

    int width;
    int height;
    std::vector<std::vector<int>> board; // 0 = empty, 1 = me, 2 = opponent
};
