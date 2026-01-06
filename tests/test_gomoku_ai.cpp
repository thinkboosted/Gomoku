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

/*
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
*/

/*
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
*/

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

static void test_defensive_forced_win_block() {
    GomokuAI ai;
    ai.init(10);

    // Opponent setup: two coupled threats that win in 2 plies if they start at (4,4).
    // Diagonal stones create dual threat after (4,4) is played: either extension to (5,5) or (3,3).
    place(ai, {{2,2},{3,3},{5,5},{6,6}}, 2);

    // Some of our stones to create alternative attractive moves near center.
    place(ai, {{4,6},{4,5}}, 1);

    Point p = ai.find_best_move();
    assert(p.x == 4 && p.y == 4 && "Should preempt opponent 2-ply forced win starter");
}

/*
static void test_prefer_double_threat_attack() {
    GomokuAI ai;
    ai.init(10);

    // Move at (4,4) creates two open threes (horizontal and vertical).
    place(ai, {{2,4},{3,4}}, 1);          // horizontal chain
    place(ai, {{4,2},{4,3}}, 1);          // vertical chain

    // Distractor stones near center to tempt proximity-only moves.
    place(ai, {{5,5},{6,6}}, 1);
    place(ai, {{6,4}}, 2); // mild block on the right to avoid easy win

    Point p = ai.find_best_move();
    assert(p.x == 4 && p.y == 4 && "Should choose the fork creating double open threes");
}
*/

static void test_prefer_open_four_over_defense() {
    GomokuAI ai;
    ai.init(10);

    // We have an open three horizontally at y=5: ends (0,5) and (4,5) are open.
    place(ai, {{1,5},{2,5},{3,5}}, 1);

    // Opponent also has an open three horizontally at y=1: ends (0,1) and (4,1) are open.
    place(ai, {{1,1},{2,1},{3,1}}, 2);

    // Best play is to create an open four (immediate winning threat), not to block their open three.
    Point p = ai.find_best_move();
    bool makes_open_four_left = (p.x == 0 && p.y == 5);
    bool makes_open_four_right = (p.x == 4 && p.y == 5);
    assert((makes_open_four_left || makes_open_four_right) && "Should prefer creating an open four over defending an open three");
}

// 1. Strict Adjacent Block for Open Four threat (formerly ..000..)
static void test_strict_adjacent_block_open_three() {
    GomokuAI ai;
    ai.init(20);
    // Opponent has Open Three ..000..
    place(ai, {{5,5}, {6,5}, {7,5}}, 2); 

    Point p = ai.find_best_move();
    bool strict_block = (p.x == 4 && p.y == 5) || (p.x == 8 && p.y == 5);
    assert(strict_block && "Must block Open Three adjacently to prevent Open Four");
}

// 2. Strict Adjacent Block for Blocked Three (formerly X000..)
static void test_strict_adjacent_block_blocked_three() {
    GomokuAI ai;
    ai.init(20);
    // Opponent has Blocked Three X000..
    place(ai, {{4,5}}, 1); 
    place(ai, {{5,5}, {6,5}, {7,5}}, 2);

    Point p = ai.find_best_move();
    assert(p.x == 8 && p.y == 5 && "Must block Blocked Three adjacently");
}

// 3. Block Fork 3-3 (Double Threat)
/*
static void test_block_fork_3_3() {
    GomokuAI ai;
    ai.init(20);
    // Opponent setups a fork at 10,10.
    place(ai, {{10,8}, {10,9}}, 2);
    place(ai, {{11,10}, {12,10}}, 2);
    
    // Minor distraction
    place(ai, {{2,2}, {2,3}}, 2);

    Point p = ai.find_best_move();
    assert(p.x == 10 && p.y == 10 && "Must block the intersection of a Fork 3-3");
}
*/

// 4. Block Diagonal Broken Three
static void test_block_diagonal_broken_three() {
    GomokuAI ai;
    ai.init(20);
    // Diagonal Broken Three: O . O O
    place(ai, {{5,5}, {7,7}, {8,8}}, 2);

    Point p = ai.find_best_move();
    assert(p.x == 6 && p.y == 6 && "Must block the gap in a Diagonal Broken Three");
}

// 5. Create Open Four (Attack Priority)
static void test_create_open_four_priority() {
    GomokuAI ai;
    ai.init(20);
    // I have Open Three: . X X X .
    place(ai, {{5,5}, {6,5}, {7,5}}, 1);
    
    // Opponent has Open Three elsewhere: . O O O .
    place(ai, {{5,10}, {6,10}, {7,10}}, 2);

    Point p = ai.find_best_move();
    bool attack = (p.x == 4 && p.y == 5) || (p.x == 8 && p.y == 5);
    assert(attack && "Must prioritize creating Open Four over blocking Open Three");
}

// 6. Block Open Four (Defense Priority)
static void test_block_open_four_priority() {
    GomokuAI ai;
    ai.init(20);
    // Opponent has Broken Four (gap) that leads to 5.
    place(ai, {{5,5}, {6,5}, {8,5}, {9,5}}, 2);
    
    // I have Open Three.
    place(ai, {{5,10}, {6,10}, {7,10}}, 1);

    Point p = ai.find_best_move();
    assert(p.x == 7 && p.y == 5 && "Must block immediate Win threat over creating Open Four");
}

int main() {
    test_center_start();
    test_immediate_win();
    test_block_opponent_win();
    // test_avoid_weak_closed_four();
    // test_avoid_neutral_filler();
    test_block_open_three_over_filler();
    test_block_open_or_hidden_four();
    test_defensive_forced_win_block();
    // test_prefer_double_threat_attack();
    test_prefer_open_four_over_defense();
    
    test_strict_adjacent_block_open_three();
    test_strict_adjacent_block_blocked_three();
    // test_block_fork_3_3();
    test_block_diagonal_broken_three();
    test_create_open_four_priority();
    test_block_open_four_priority();

    std::cout << "All tests passed\n";
    return 0;
}
