#include "../src/GomokuAI.hpp"
#include <cassert>
#include <iostream>

// Simple helpers to set up boards quickly.
static void place(GomokuAI& ai, std::initializer_list<std::pair<int,int>> coords, int player) {
    for (auto [x,y] : coords) ai.update_board(x, y, player);
}

static void test_center_start() {
    GomokuAI ai;
    ai.init(10);
    Point p = ai.find_best_move();
    assert(p.x == 5 && p.y == 5 && "Empty board should start at center");
}

static void test_immediate_win() {
    GomokuAI ai;
    ai.init(10);
    // Four of ours in a row, gap at (4,5)
    place(ai, {{0,5},{1,5},{2,5},{3,5}}, 1);
    Point p = ai.find_best_move();
    assert(p.x == 4 && p.y == 5 && "Should complete 5 in a row to win");
}

static void test_block_opponent_win() {
    GomokuAI ai;
    ai.init(10);
    // Opponent threatens 5, we must block at (4,4)
    place(ai, {{0,4},{1,4},{2,4},{3,4}}, 2);
    Point p = ai.find_best_move();
    assert(p.x == 4 && p.y == 4 && "Should block opponent's immediate win");
}

int main() {
    test_center_start();
    test_immediate_win();
    test_block_opponent_win();
    std::cout << "All tests passed\n";
    return 0;
}
