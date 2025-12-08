#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <ctime>

namespace {

bool SHOULD_STOP = false;
int BOARD_WIDTH = 20;
int BOARD_HEIGHT = 20;
std::vector<std::vector<int>> BOARD; // 0 = empty, 1 = me, 2 = opponent

enum log_type : std::uint8_t {
  UNKNOWN,
  ERROR,
  MESSAGE,
  DEBUG,
};

const std::unordered_map<log_type, std::string_view> LOG_TYPE_MAPPINGS{
    {UNKNOWN, "UNKNOWN"},
    {ERROR, "ERROR"},
    {MESSAGE, "MESSAGE"},
    {DEBUG, "DEBUG"},
};

auto send_log(log_type type, const std::string_view &msg) -> void {
  std::cout << LOG_TYPE_MAPPINGS.at(type) << " " << msg << std::endl;
}

// --- Game Logic Helpers ---

struct Point {
    int x;
    int y;
};

void init_board(int size) {
    BOARD_WIDTH = size;
    BOARD_HEIGHT = size;
    BOARD.assign(BOARD_WIDTH, std::vector<int>(BOARD_HEIGHT, 0));
}

void update_board(int x, int y, int player) {
    if (x >= 0 && x < BOARD_WIDTH && y >= 0 && y < BOARD_HEIGHT) {
        BOARD[x][y] = player;
    }
}

Point parse_coordinates(const std::string& s) {
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

Point find_best_move() {
    // Simple random logic for now
    int attempts = 0;
    while (attempts < 1000) {
        int x = rand() % BOARD_WIDTH;
        int y = rand() % BOARD_HEIGHT;
        if (BOARD[x][y] == 0) {
            return {x, y};
        }
        attempts++;
    }
    // Fallback: search linearly
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        for (int y = 0; y < BOARD_HEIGHT; ++y) {
            if (BOARD[x][y] == 0) return {x, y};
        }
    }
    return {-1, -1};
}

// --- Command Handlers ---

auto handle_about([[maybe_unused]] std::string &cmd) -> void {
  constexpr std::string_view bot_name = "pbrain-gomoku-ai";
  std::cout << "name=\"" << bot_name << "\", version=\"1.0\", author=\"Mael\", country=\"FR\"" << std::endl;
}

auto handle_start(std::string &cmd) -> void {
  // Format: START [size]
  std::stringstream ss(cmd);
  std::string temp;
  int size;
  ss >> temp >> size;
  if (ss.fail()) size = 20; // Default fallback

  init_board(size);
  std::cout << "OK" << std::endl;
}

auto handle_end([[maybe_unused]] std::string &cmd) -> void {
  SHOULD_STOP = true;
}

auto handle_info([[maybe_unused]] std::string &cmd) -> void {
  // Handle info - ignored for now
}

auto handle_begin([[maybe_unused]] std::string &cmd) -> void {
  Point p = find_best_move();
  update_board(p.x, p.y, 1); // 1 is us
  std::cout << p.x << "," << p.y << std::endl;
}

auto handle_turn(std::string &cmd) -> void {
  // Format: TURN X,Y
  // cmd looks like "TURN 10,10"
  std::stringstream ss(cmd);
  std::string temp, coords;
  ss >> temp >> coords;

  Point opp = parse_coordinates(coords);
  if (opp.x != -1) {
      update_board(opp.x, opp.y, 2); // 2 is opponent
  }

  Point p = find_best_move();
  update_board(p.x, p.y, 1); // 1 is us
  std::cout << p.x << "," << p.y << std::endl;
}

auto handle_board([[maybe_unused]] std::string &cmd) -> void {
    // Reconstruct board from scratch
    init_board(BOARD_WIDTH); // Clear board
    std::string entry;
    while (std::getline(std::cin, entry)) {
        // Handle Windows CR
        if (!entry.empty() && entry.back() == '\r') entry.pop_back();
        if (entry == "DONE") break;

        // Parse "x,y,player"
        size_t c1 = entry.find(',');
        size_t c2 = entry.find(',', c1 + 1);
        if (c1 != std::string::npos && c2 != std::string::npos) {
            try {
                int x = std::stoi(entry.substr(0, c1));
                int y = std::stoi(entry.substr(c1 + 1, c2 - c1 - 1));
                int player = std::stoi(entry.substr(c2 + 1));
                update_board(x, y, player);
            } catch (...) {}
        }
    }
    // Play after board reconstruction
    Point p = find_best_move();
    update_board(p.x, p.y, 1);
    std::cout << p.x << "," << p.y << std::endl;
}

struct CommandMapping {
  std::string cmd;
  std::function<void(std::string &)> func;
};

const std::array<CommandMapping, 7> COMMAND_MAPPINGS{{
    {
        "ABOUT",
        handle_about,
    },
    {
        "START",
        handle_start,
    },
    {
        "END",
        handle_end,
    },
    {
        "INFO",
        handle_info,
    },
    {
        "BEGIN",
        handle_begin,
    },
    {
        "TURN",
        handle_turn,
    },
    {
        "BOARD",
        handle_board,
    },
}};

auto handle_command(std::string &cmd) -> void {
    // Remove potential carriage return
    if (!cmd.empty() && cmd.back() == '\r') cmd.pop_back();
    if (cmd.empty()) return;

    for (const auto &i : COMMAND_MAPPINGS) {
        if (cmd.rfind(i.cmd, 0) == 0) {
            i.func(cmd);
            return;
        }
    }
    send_log(UNKNOWN, "command is not implemented");
}

} // namespace

int main() {
    // Unbuffered output is crucial for protocol communication
    std::cout.setf(std::ios::unitbuf);
    srand(static_cast<unsigned>(time(NULL)));

    for (std::string line; !SHOULD_STOP && std::getline(std::cin, line);) {
        handle_command(line);
    }
    return 0;
}