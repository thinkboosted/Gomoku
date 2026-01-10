#include "../src/Protocol.hpp"
#include "../src/GomokuAI.hpp"
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

// Mock Protocol class for testing
class TestableProtocol : public Protocol {
public:
    // Make private methods accessible for testing
    using Protocol::handle_start;
    using Protocol::handle_turn;
    using Protocol::handle_begin;
    using Protocol::handle_about;

    // Access to the AI for verification
    GomokuAI& get_ai() { return ai; }
};

static void test_start_valid_size() {
    TestableProtocol protocol;
    std::string cmd = "START 15";

    // Capture output
    std::streambuf* old = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());

    protocol.handle_start(cmd);

    std::cout.rdbuf(old);

    std::string output = ss.str();
    assert(output.find("OK") != std::string::npos && "Should respond with OK for valid size");
    assert(protocol.get_ai().width == 15 && "Board size should be 15");
}

static void test_start_minimum_size() {
    TestableProtocol protocol;
    std::string cmd = "START 5";

    // Capture output
    std::streambuf* old = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());

    protocol.handle_start(cmd);

    std::cout.rdbuf(old);

    std::string output = ss.str();
    assert(output.find("OK") != std::string::npos && "Size 5 should be accepted");
    assert(protocol.get_ai().width == 5 && "Board size should be 5");
}

static void test_start_unsupported_size_4() {
    TestableProtocol protocol;
    std::string cmd = "START 4";

    // Capture output
    std::streambuf* old = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());

    protocol.handle_start(cmd);

    std::cout.rdbuf(old);

    std::string output = ss.str();
    assert(output.find("ERROR unsupported size") != std::string::npos &&
           "Should respond with ERROR unsupported size for size < 5");
}

static void test_start_unsupported_size_1() {
    TestableProtocol protocol;
    std::string cmd = "START 1";

    // Capture output
    std::streambuf* old = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());

    protocol.handle_start(cmd);

    std::cout.rdbuf(old);

    std::string output = ss.str();
    assert(output.find("ERROR unsupported size") != std::string::npos &&
           "Should respond with ERROR unsupported size for size < 5");
}

static void test_start_unsupported_size_0() {
    TestableProtocol protocol;
    std::string cmd = "START 0";

    // Capture output
    std::streambuf* old = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());

    protocol.handle_start(cmd);

    std::cout.rdbuf(old);

    std::string output = ss.str();
    assert(output.find("ERROR unsupported size") != std::string::npos &&
           "Should respond with ERROR unsupported size for size < 5");
}

static void test_start_large_size() {
    TestableProtocol protocol;
    std::string cmd = "START 19";

    // Capture output
    std::streambuf* old = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());

    protocol.handle_start(cmd);

    std::cout.rdbuf(old);

    std::string output = ss.str();
    assert(output.find("OK") != std::string::npos && "Large size should be accepted");
    assert(protocol.get_ai().width == 19 && "Board size should be 19");
}

static void test_start_default_size() {
    TestableProtocol protocol;
    std::string cmd = "START";

    // Capture output
    std::streambuf* old = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());

    protocol.handle_start(cmd);

    std::cout.rdbuf(old);

    std::string output = ss.str();
    assert(output.find("OK") != std::string::npos && "Should use default size when not specified");
    assert(protocol.get_ai().width == 20 && "Default board size should be 20");
}

static void test_about_command() {
    TestableProtocol protocol;
    std::string cmd = "ABOUT";

    // Capture output
    std::streambuf* old = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());

    protocol.handle_about(cmd);

    std::cout.rdbuf(old);

    std::string output = ss.str();
    assert(output.find("pbrain-gomoku-ai") != std::string::npos &&
           "ABOUT should contain engine name");
    assert(output.find("version") != std::string::npos &&
           "ABOUT should contain version");
    assert(output.find("author") != std::string::npos &&
           "ABOUT should contain author");
}

static void test_turn_updates_opponent() {
    TestableProtocol protocol;

    // Initialize board
    std::string start_cmd = "START 10";
    protocol.handle_start(start_cmd);

    // Capture output
    std::streambuf* old = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());

    std::string turn_cmd = "TURN 5,5";
    protocol.handle_turn(turn_cmd);

    std::cout.rdbuf(old);

    // Verify opponent move was placed and our move was made
    int width = protocol.get_ai().width;
    assert(protocol.get_ai().board[5 * width + 5] == 2 &&
           "Opponent move at (5,5) should be placed");
}

static void test_begin_plays_first_move() {
    TestableProtocol protocol;

    // Initialize board with default 20x20
    std::string start_cmd = "START 20";
    protocol.handle_start(start_cmd);

    // Capture output
    std::streambuf* old = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());

    std::string begin_cmd = "BEGIN";
    protocol.handle_begin(begin_cmd);

    std::cout.rdbuf(old);

    std::string output = ss.str();
    // Output should be coordinates
    assert(output.find(",") != std::string::npos &&
           "BEGIN should output coordinates");
}

int main() {
    std::cout << "Testing Protocol..." << std::endl;

    test_start_valid_size();
    std::cout << "✓ Valid size test passed" << std::endl;

    test_start_minimum_size();
    std::cout << "✓ Minimum size (5) test passed" << std::endl;

    test_start_unsupported_size_4();
    std::cout << "✓ Unsupported size (4) test passed" << std::endl;

    test_start_unsupported_size_1();
    std::cout << "✓ Unsupported size (1) test passed" << std::endl;

    test_start_unsupported_size_0();
    std::cout << "✓ Unsupported size (0) test passed" << std::endl;

    test_start_large_size();
    std::cout << "✓ Large size test passed" << std::endl;

    test_start_default_size();
    std::cout << "✓ Default size test passed" << std::endl;

    test_about_command();
    std::cout << "✓ ABOUT command test passed" << std::endl;

    test_turn_updates_opponent();
    std::cout << "✓ TURN updates opponent test passed" << std::endl;

    test_begin_plays_first_move();
    std::cout << "✓ BEGIN plays first move test passed" << std::endl;

    std::cout << "\nAll Protocol tests passed!" << std::endl;
    return 0;
}
