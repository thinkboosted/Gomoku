#include "GomokuAI.hpp"
#include <cstdlib>
#include <ctime>
#include <algorithm>

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

Point GomokuAI::find_best_move() {
    // Random move logic
    int attempts = 0;
    while (attempts < 1000) {
        int x = rand() % width;
        int y = rand() % height;
        if (board[x][y] == 0) {
            return {x, y};
        }
        attempts++;
    }
    // Fallback: search linearly
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            if (board[x][y] == 0) return {x, y};
        }
    }
    return {-1, -1};
}
