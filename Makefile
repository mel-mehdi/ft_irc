all:
	c++ -std=c++98 -Wall -Wextra -Werror src/main.cpp src/Server.cpp src/Channel.cpp src/Client.cpp src/Command.cpp src/utils.cpp -o ircserv
clean:
	rm -f ircserv

fclean: clean

re: fclean all