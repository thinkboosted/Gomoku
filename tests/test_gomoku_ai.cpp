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

static void test_block_open_three_over_filler() {
    GomokuAI ai;
    ai.init(9);

    // Opponent has an open three vertical at x=4 (positions y=1,2,3). Ends y=0 and y=4 are open.
    place(ai, {{4,1},{4,2},{4,3}}, 2);

    // Add neutral stones near center to tempt proximity/centrality.
    place(ai, {{2,2},{2,3},{3,2}}, 1);

    Point p = ai.find_best_move();
    bool blocks_top = (p.x == 4 && p.y == 0);
    bool blocks_bottom = (p.x == 4 && p.y == 4);
    assert((blocks_top || blocks_bottom) && "Should block opponent open three instead of a neutral move");
}

static void test_block_open_or_hidden_four() {
    GomokuAI ai;
    ai.init(10);

    // Opponent has an open four horizontally at y=5 (ends at x=2 and x=7 are open).
    place(ai, {{3,5},{4,5},{5,5},{6,5}}, 2);

    // Our own stones that could tempt a neutral/central move.
    place(ai, {{1,1},{1,2},{2,2}}, 1);

    Point p = ai.find_best_move();
    bool block_left = (p.x == 2 && p.y == 5);
    bool block_right = (p.x == 7 && p.y == 5);
    assert((block_left || block_right) && "Must block opponent open/hidden fours before other plays");
}

int main() {
    test_center_start();
    test_immediate_win();
    test_block_opponent_win();
    test_avoid_weak_closed_four();
    test_avoid_neutral_filler();
    test_block_open_three_over_filler();
    test_block_open_or_hidden_four();
    std::cout << "All tests passed\n";
    return 0;
}
