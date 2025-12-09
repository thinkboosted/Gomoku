NAME    =   pbrain-gomoku-ai

SRC     =   src/main.cpp \
            src/Protocol.cpp \
            src/GomokuAI.cpp

OBJ     =   $(SRC:.cpp=.o)

CXX     =   g++

CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -I./src

TEST_NAME = tests/test_gomoku_ai
TEST_SRC  = tests/test_gomoku_ai.cpp src/GomokuAI.cpp
TEST_OBJ  = $(TEST_SRC:.cpp=.o)

all:    $(NAME)

$(NAME):    $(OBJ)
	$(CXX) $(OBJ) -o $(NAME)

$(TEST_NAME): $(TEST_OBJ)
	$(CXX) $(TEST_OBJ) -o $(TEST_NAME)

test: $(TEST_NAME)
	@./$(TEST_NAME)

clean:
	rm -f $(OBJ)
	rm -f $(TEST_OBJ)

fclean: clean
	rm -f $(NAME)
	rm -f $(TEST_NAME)

re: fclean all

.PHONY: all clean fclean re