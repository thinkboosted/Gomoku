#include "Protocol.hpp"
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <functional>

Protocol::Protocol() : should_stop(false) {}

void Protocol::run() {
    std::cout.setf(std::ios::unitbuf); // Unbuffered output
    for (std::string line; !should_stop && std::getline(std::cin, line);) {
        // Remove potential carriage return
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        handle_command(line);
    }
}

void Protocol::send_log(const std::string_view& type, const std::string_view& msg) {
    std::cout << type << " " << msg << std::endl;
}

void Protocol::handle_start(std::string& cmd) {
    std::stringstream ss(cmd);
    std::string temp;
    int size;
    ss >> temp >> size;
    if (ss.fail()) size = 20;
    if (size < 5) {
        send_log("ERROR", "unsupported size");
        return;
    }
    ai.init(size);
    std::cout << "OK" << std::endl;
}

void Protocol::handle_turn(std::string& cmd) {
    std::stringstream ss(cmd);
    std::string temp, coords;
    ss >> temp >> coords;

    Point opp = ai.parse_coordinates(coords);
    if (opp.x != -1) {
        ai.update_board(opp.x, opp.y, 2); // 2 is opponent
    }

    Point p = ai.find_best_move(timeout_turn);
    ai.update_board(p.x, p.y, 1); // 1 is us // 1 is us
    std::cout << p.x << "," << p.y << std::endl;
}

void Protocol::handle_begin([[maybe_unused]] std::string& cmd) {
    Point p = ai.find_best_move(timeout_turn);
    ai.update_board(p.x, p.y, 1); // 1 is us
    std::cout << p.x << "," << p.y << std::endl;
}

void Protocol::handle_board([[maybe_unused]] std::string& cmd) {
    ai.init(ai.width); // Clear board
    std::string entry;
    while (std::getline(std::cin, entry)) {
        if (!entry.empty() && entry.back() == '\r') entry.pop_back();
        if (entry == "DONE") break;

        size_t c1 = entry.find(',');
        size_t c2 = entry.find(',', c1 + 1);
        if (c1 != std::string::npos && c2 != std::string::npos) {
            try {
                int x = std::stoi(entry.substr(0, c1));
                int y = std::stoi(entry.substr(c1 + 1, c2 - c1 - 1));
                int player = std::stoi(entry.substr(c2 + 1));
                ai.update_board(x, y, player);
            } catch (...) {}
        }
    }
    Point p = ai.find_best_move(timeout_turn);
    ai.update_board(p.x, p.y, 1); // 1 is us
    std::cout << p.x << "," << p.y << std::endl;
}

void Protocol::handle_info(std::string& cmd) {
    std::stringstream ss(cmd);
    std::string temp, key;
    ss >> temp; // INFO
    while (ss >> key) {
        if (key == "timeout_turn") {
            int val;
            ss >> val;
            if (!ss.fail()) timeout_turn = val;
        } else if (key == "timeout_match") {
            int val;
            ss >> val;
            if (!ss.fail()) timeout_match = val;
        } else if (key == "time_left") {
             int val;
            ss >> val;
            if (!ss.fail()) {
                // If we have a huge bank, we might want to use more than timeout_turn if allowed,
                // but usually timeout_turn is a hard cap per move.
                // If timeout_turn is 0 (unlimited), we use time_left / remaining_moves.
                if (timeout_turn == 0) {
                     timeout_turn = val / 25; // Estimate 25 moves remaining
                     if (timeout_turn < 100) timeout_turn = 100;
                } else if (val < timeout_turn) {
                    timeout_turn = val;
                }
            }
        } else {
             std::string val;
             ss >> val; // consume value
        }
    }
}

void Protocol::handle_end([[maybe_unused]] std::string& cmd) {
    should_stop = true;
}

void Protocol::handle_about([[maybe_unused]] std::string& cmd) {
    std::cout << "name=\"pbrain-gomoku-ai\", version=\"1.0\", author=\"Mael-Tristan\", country=\"FR\"" << std::endl;
}

void Protocol::handle_command(std::string& cmd) {
    if (cmd.rfind("START", 0) == 0) handle_start(cmd);
    else if (cmd.rfind("TURN", 0) == 0) handle_turn(cmd);
    else if (cmd.rfind("BEGIN", 0) == 0) handle_begin(cmd);
    else if (cmd.rfind("BOARD", 0) == 0) handle_board(cmd);
    else if (cmd.rfind("INFO", 0) == 0) handle_info(cmd);
    else if (cmd.rfind("END", 0) == 0) handle_end(cmd);
    else if (cmd.rfind("ABOUT", 0) == 0) handle_about(cmd);
    else send_log("UNKNOWN", "command not implemented");
}
