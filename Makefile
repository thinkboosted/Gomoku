NAME    =   pbrain-gomoku-ai

SRC     =   src/main.cpp \
            src/Protocol.cpp \
            src/GomokuAI.cpp

OBJ     =   $(SRC:.cpp=.o)

CXX     =   g++

CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -I./src

all:    $(NAME)

$(NAME):    $(OBJ)
	$(CXX) $(OBJ) -o $(NAME)

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re