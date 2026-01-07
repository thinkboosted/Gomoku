#pragma once

#include <string>
#include <string_view>
#include "GomokuAI.hpp"

class Protocol {
public:
    Protocol();
    void run();

protected:
    GomokuAI ai;

    void handle_start(std::string& cmd);
    void handle_turn(std::string& cmd);
    void handle_begin(std::string& cmd);
    void handle_about(std::string& cmd);

private:
    bool should_stop;
    int timeout_turn = 1000;
    int timeout_match = 100000;
    int time_left = 2147483647;

    void handle_command(std::string& cmd);

    void handle_board(std::string& cmd);
    void handle_info(std::string& cmd);
    void handle_end(std::string& cmd);

    void send_log(const std::string_view& type, const std::string_view& msg);
};
