#include "../src/GomokuAI.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cassert>

// Utility to print board
void print_board(const GomokuAI& ai) {
    for (int y = 0; y < ai.height; ++y) {
        for (int x = 0; x < ai.width; ++x) {
            int cell = ai.board[y * ai.width + x];
            char c = (cell == 0 ? '.' : (cell == 1 ? 'X' : 'O'));
            std::cout << c << " ";
        }
        std::cout << "\n";
    }
}

// Setup a scenario from a list of coordinates
void setup_board(GomokuAI& ai, const std::vector<std::pair<int, int>>& my_stones, const std::vector<std::pair<int, int>>& opp_stones) {
    ai.init(20);
    for (auto p : my_stones) ai.update_board(p.first, p.second, 1);
    for (auto p : opp_stones) ai.update_board(p.first, p.second, 2);
}

bool run_test(const std::string& name, const std::vector<std::pair<int, int>>& my_stones, const std::vector<std::pair<int, int>>& opp_stones, std::pair<int, int> expected_move, bool strict = true) {
    GomokuAI ai;
    setup_board(ai, my_stones, opp_stones);
    
    std::cout << "TEST: " << name << "...\n";
    // print_board(ai);
    
    auto start = std::chrono::high_resolution_clock::now();
    Point best = ai.find_best_move(1000); // 1s max for tactical tests
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    bool passed = (best.x == expected_move.first && best.y == expected_move.second);
    
    if (!strict && !passed) {
        // Checking if the move is at least a win/block logic
        // For now, simple strict check
    }

    if (passed) {
        std::cout << " [PASS] Found (" << best.x << "," << best.y << ") in " << diff.count() << "s\n";
    } else {
        std::cout << " [FAIL] Found (" << best.x << "," << best.y << ") but expected (" << expected_move.first << "," << expected_move.second << ")\n";
    }
    return passed;
}

int main() {
    int passed = 0;
    int total = 0;

    // 1. Simple Win (4 aligned)
    // . X X X X . -> Play 5th
    total++;
    if (run_test("Immediate Win (Horizontal)", 
        {{5,5}, {6,5}, {7,5}, {8,5}},
        {{5,6}, {6,6}, {7,6}},
        {9,5})) passed++;

    // 2. Block Opponent Win
    // . O O O O . -> Block
    total++;
    if (run_test("Block Opponent Win (Vertical)", 
        {{1,1}},
        {{10,5}, {10,6}, {10,7}, {10,8}},
        {10,9})) passed++; // 10,4 or 10,9 valid, checking 10,9 (or logic needed if both valid)

    // 3. Open Three (Create Open 4)
    // . . X X X . . -> Play to make Open 4
    total++;
    if (run_test("Create Open Four", 
        {{5,5}, {6,5}, {7,5}},
        {{10,10}},
        {4,5})) passed++; // 4,5 or 8,5

    // 4. 3-3 Fork (The hard one for simple AIs)
    // We have two intersecting lines of 2. Playing the intersection makes double open 3.
    //   X
    //   .
    // X . . .
    // Target: Intersection
    total++;
    if (run_test("Fork 3-3 Attack", 
        {{5,5}, {5,6}, {6,7}, {7,7}}, // 5,5-5,6 (Vertical col 5) and 6,7-7,7 (Horizontal row 7). Intersection at 5,7
        {{1,1}},
        {5,7})) passed++;

    // 5. Block 3-3 Fork
    total++;
    if (run_test("Block 3-3 Fork", 
        {{1,1}},
        {{10,10}, {10,11}, {11,12}, {12,12}}, // Intersection at 10,12
        {10,12})) passed++;

    std::cout << "\nRESULT: " << passed << "/" << total << " tests passed.\n";
    
    return (passed == total) ? 0 : 1;
}
