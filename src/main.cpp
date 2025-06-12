#include <iostream>
#include <cstdlib>
#include <signal.h>
#include "../include/Server.hpp"

Server* g_server = NULL;

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutting down IRC server..." << std::endl;
        delete g_server;
        exit(0);
    }
}

void is_valid_port(char *str)
{
	for(int i = 0; str[i]; i++)
	{
		if(isspace(str[i]))
			i++;
		if(str[i] == '+')
			i++;
		if(!isdigit(str[i]) )
		{
			std::cout << "Error: Port invalid!\n";
			exit(1);
		}
	}
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
        return 1;
    }
    is_valid_port(argv[1]);
    int port = std::atoi(argv[1]);
    std::string password = argv[2];
    
    if (port <= 0 || port > 65535) {
        std::cerr << "Error: Port must be between 1 and 65535" << std::endl;
        return 1;
    }
    
    // Set up signal handling
    signal(SIGINT, signalHandler);
    // signal(SIGTERM, signalHandler);
    
    try {
        g_server = new Server(port, password);
        g_server->start();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        delete g_server;
        return 1;
    }
    
    delete g_server;
    return 0;
}