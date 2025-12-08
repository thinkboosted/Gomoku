#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <ctime>

// Simple structure to represent a point
struct Point {
    int x;
    int y;
};

class GomokuAI {
public:
    int width;
    int height;
    std::vector<std::vector<int>> board; // 0 = empty, 1 = me, 2 = opponent

    GomokuAI() : width(20), height(20) {
        srand(time(NULL));
    }

    void init(int size) {
        width = size;
        height = size;
        board.assign(width, std::vector<int>(height, 0));
    }

    // Function to parse coordinates "x,y"
    Point parseCoordinates(const std::string& s) {
        size_t commaPos = s.find(',');
        if (commaPos == std::string::npos) return {-1, -1};
        int x = std::stoi(s.substr(0, commaPos));
        int y = std::stoi(s.substr(commaPos + 1));
        return {x, y};
    }

    // Simple random move logic (to be improved!)
    Point findBestMove() {
        // Try to find a random empty spot
        int attempts = 0;
        while (attempts < 1000) {
            int x = rand() % width;
            int y = rand() % height;
            if (board[x][y] == 0) {
                return {x, y};
            }
            attempts++;
        }
        // Fallback: search linearly
        for (int x = 0; x < width; ++x) {
            for (int y = 0; y < height; ++y) {
                if (board[x][y] == 0) return {x, y};
            }
        }
        return {-1, -1}; // Should not happen if board not full
    }

    void updateBoard(int x, int y, int player) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            board[x][y] = player;
        }
    }

    void run() {
        std::string line;
        while (std::getline(std::cin, line)) {
            // Remove carriage return if present (Windows compatibility)
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            std::stringstream ss(line);
            std::string command;
            ss >> command;

            if (command == "START") {
                int size;
                ss >> size;
                init(size);
                std::cout << "OK" << std::endl;
            }
            else if (command == "BEGIN") {
                // We start first
                Point p = findBestMove();
                updateBoard(p.x, p.y, 1); // 1 is us
                std::cout << p.x << "," << p.y << std::endl;
            }
            else if (command == "TURN") {
                std::string coords;
                ss >> coords;
                Point opp = parseCoordinates(coords);
                updateBoard(opp.x, opp.y, 2); // 2 is opponent

                Point p = findBestMove();
                updateBoard(p.x, p.y, 1); // 1 is us
                std::cout << p.x << "," << p.y << std::endl;
            }
            else if (command == "BOARD") {
                // Reconstruct board from scratch
                board.assign(width, std::vector<int>(height, 0));
                std::string entry;
                while (std::getline(std::cin, entry)) {
                    if (!entry.empty() && entry.back() == '\r') entry.pop_back();
                    if (entry == "DONE") break;

                    // Parse "x,y,player"
                    size_t c1 = entry.find(',');
                    size_t c2 = entry.find(',', c1 + 1);
                    if (c1 != std::string::npos && c2 != std::string::npos) {
                        int x = std::stoi(entry.substr(0, c1));
                        int y = std::stoi(entry.substr(c1 + 1, c2 - c1 - 1));
                        int player = std::stoi(entry.substr(c2 + 1));
                        updateBoard(x, y, player); // 1 or 2
                    }
                }
                // Play after board reconstruction
                Point p = findBestMove();
                updateBoard(p.x, p.y, 1);
                std::cout << p.x << "," << p.y << std::endl;
            }
            else if (command == "ABOUT") {
                std::cout << "name=\"MyGomokuAI\", version=\"1.0\", author=\"Mael\", country=\"FR\"" << std::endl;
            }
            else if (command == "END") {
                break;
            }
            // INFO commands are ignored for now
        }
    }
};

int main() {
    // Unbuffered I/O is safer for pipes
    std::cout.setf(std::ios::unitbuf);

    GomokuAI ai;
    ai.run();
    return 0;
}
