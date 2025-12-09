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

static void test_avoid_weak_closed_four() {
    GomokuAI ai;
    ai.init(10);

    // Our horizontal three with opponent glued to one end: playing at (4,5) would only make a closed four.
    place(ai, {{5,5},{6,5},{7,5}}, 1);
    place(ai, {{8,5}}, 2); // blocks one side

    // A better offensive option: extend vertical chain to an open three at (2,4).
    place(ai, {{2,2},{2,3}}, 1);
    place(ai, {{2,0}}, 2); // close the lower end so (2,1) is weaker

    Point p = ai.find_best_move();
    assert(p.x == 2 && p.y == 4 && "Should prefer creating an open three over pushing a blocked four");
}

static void test_avoid_neutral_filler() {
    GomokuAI ai;
    ai.init(10);

    // We have a promising vertical extension: playing (1,3) makes a closed three (and can become open later).
    place(ai, {{1,1},{1,2}}, 1);

    // Add some stones near center to tempt proximity/centrality without real threat.
    place(ai, {{5,5}}, 2);
    place(ai, {{5,6}}, 1);

    Point p = ai.find_best_move();
    assert(p.x == 1 && p.y == 3 && "Should pick the meaningful extension over neutral central filler");
}

int main() {
    test_center_start();
    test_immediate_win();
    test_block_opponent_win();
    test_avoid_weak_closed_four();
    test_avoid_neutral_filler();
    std::cout << "All tests passed\n";
    return 0;
}
