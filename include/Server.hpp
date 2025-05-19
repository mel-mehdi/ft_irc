#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include <vector>
#include <poll.h>
#include <netinet/in.h>
#include "Client.hpp"
#include "Channel.hpp"

class Server {
private:
    int port;
    std::string password;
    int serverSocket;
    std::vector<pollfd> pollFds;
    std::map<int, Client*> clients;
    std::map<std::string, Channel*> channels;
    
    // Private methods
    void setupSocket();
    void acceptClient();
    void handleClientData(int clientFd);
    void removeClient(int clientFd);
    void executeCommand(Client* client, const std::string& command);
    
public:
    Server(int port, const std::string& password);
    ~Server();
    
    void start();
    void broadcast(const std::string& message, int excludeFd = -1);
    void sendToClient(int clientFd, const std::string& message);
    
    // Channel management
    Channel* getChannel(const std::string& name);
    Channel* createChannel(const std::string& name, Client* creator);
    void removeChannel(const std::string& name);
    
    // Client management
    Client* getClientByNickname(const std::string& nickname);
    
    // Getters
    const std::string& getPassword() const;
};

#endif