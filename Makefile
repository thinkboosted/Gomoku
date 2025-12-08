NAME    =   pbrain-gomoku-ai

SRC     =   src/main.cpp

OBJ     =   $(SRC:.cpp=.o)

CXX     =   g++

CXXFLAGS = -Wall -Wextra -Werror -std=c++17

all:    $(NAME)

$(NAME):    $(OBJ)
	$(CXX) $(OBJ) -o $(NAME)

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
