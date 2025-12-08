#pragma once

#include <string>
#include <string_view>
#include "GomokuAI.hpp"

class Protocol {
public:
    Protocol();
    void run();

private:
    GomokuAI ai;
    bool should_stop;

    void handle_command(std::string& cmd);

    void handle_start(std::string& cmd);
    void handle_turn(std::string& cmd);
    void handle_begin(std::string& cmd);
    void handle_board(std::string& cmd);
    void handle_info(std::string& cmd);
    void handle_end(std::string& cmd);
    void handle_about(std::string& cmd);

    void send_log(const std::string_view& type, const std::string_view& msg);
};
