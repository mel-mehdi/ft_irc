#include "../include/Server.hpp"
#include "../include/utils.hpp"
#include "../include/Command.hpp"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>

Server::Server(int port, const std::string& password)
    : port(port), password(password), serverSocket(-1) {}

Server::~Server() {
    if (serverSocket != -1) close(serverSocket);
    
    std::map<int, Client*>::iterator it;
    for (it = clients.begin(); it != clients.end(); ++it)
        delete it->second;
    
    std::map<std::string, Channel*>::iterator it2;
    for (it2 = channels.begin(); it2 != channels.end(); ++it2)
        delete it2->second;
}

void Server::setupSocket() {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
        throw std::runtime_error("Failed to create socket");
    
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1 ||
        fcntl(serverSocket, F_SETFL, O_NONBLOCK) == -1)
        throw std::runtime_error("Failed to set socket options");
    
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1)
        throw std::runtime_error("Failed to bind socket");
    
    if (listen(serverSocket, 10) == -1)
        throw std::runtime_error("Failed to listen on socket");
    
    std::cout << "Server listening on port " << port << std::endl;
    
    pollfd pfd = {serverSocket, POLLIN, 0};
    pollFds.push_back(pfd);
}

void Server::acceptClient() {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    
    int clientFd = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (clientFd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            std::cerr << "Failed to accept client connection: " << strerror(errno) << std::endl;
        return;
    }
    
    if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1) {
        std::cerr << "Failed to set client socket to non-blocking mode" << std::endl;
        close(clientFd);
        return;
    }
    
    pollfd pfd = {clientFd, POLLIN, 0};
    pollFds.push_back(pfd);
    
    std::string clientIP = inet_ntoa(clientAddr.sin_addr);
    clients[clientFd] = new Client(clientFd, clientIP);
    
    std::cout << "New client connected from " << clientIP << " (fd: " << clientFd << ")" << std::endl;
}

void Server::removeClient(int clientFd) {
    if (clients.find(clientFd) == clients.end()) return;
    
    Client* client = clients[clientFd];
    
    // Remove client from all channels
    std::map<std::string, Channel*>::iterator chanIt;
    for (chanIt = channels.begin(); chanIt != channels.end(); ++chanIt)
        chanIt->second->removeClient(client);
    
    // Clean up empty channels
    std::vector<std::string> emptyChannels;
    for (chanIt = channels.begin(); chanIt != channels.end(); ++chanIt) {
        if (chanIt->second->getClients().empty())
            emptyChannels.push_back(chanIt->first);
    }
    
    for (size_t i = 0; i < emptyChannels.size(); ++i)
        removeChannel(emptyChannels[i]);
    
    std::cout << "Client disconnected (fd: " << clientFd << ")" << std::endl;
    delete client;
    clients.erase(clientFd);
    
    // Remove from poll array
    for (size_t i = 0; i < pollFds.size(); ++i) {
        if (pollFds[i].fd == clientFd) {
            pollFds.erase(pollFds.begin() + i);
            break;
        }
    }
    
    close(clientFd);
}

void Server::handleClientData(int clientFd) {
    Client* client = clients[clientFd];
    char buffer[1024];
    
    ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesRead <= 0) {
        if (bytesRead == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            if (bytesRead < 0) 
                std::cerr << "Error receiving data: " << strerror(errno) << std::endl;
            removeClient(clientFd);
        }
        return;
    }
    
    buffer[bytesRead] = '\0';
    client->appendBuffer(buffer);
    
    // Process completed commands
    std::vector<std::string> commands = client->getCompletedCommands();
    for (size_t i = 0; i < commands.size(); ++i)
        executeCommand(client, commands[i]);
}

void Server::start() {
    try {
        setupSocket();
        std::cout << "IRC Server started successfully!" << std::endl;
        
        while (true) {
            int ready = poll(pollFds.data(), pollFds.size(), -1);
            
            if (ready == -1) {
                if (errno == EINTR) continue;
                throw std::runtime_error("Poll failed");
            }
            
            if (pollFds[0].revents & POLLIN)
                acceptClient();
            
            for (size_t i = 1; i < pollFds.size(); ++i) {
                if (pollFds[i].revents & POLLIN)
                    handleClientData(pollFds[i].fd);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
}

void Server::broadcast(const std::string& message, int excludeFd) {
    std::map<int, Client*>::iterator it;
    for (it = clients.begin(); it != clients.end(); ++it) {
        if (it->first != excludeFd && it->second->isAuthenticated()) {
            sendToClient(it->first, message);
        }
    }
}

void Server::sendToClient(int clientFd, const std::string& message) {
    std::string fullMessage = message;
    if (fullMessage.find("\r\n") == std::string::npos)
        fullMessage += "\r\n";
    
    send(clientFd, fullMessage.c_str(), fullMessage.size(), 0);
}

Channel* Server::getChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = channels.find(name);
    return (it != channels.end()) ? it->second : NULL;
}

Channel* Server::createChannel(const std::string& name, Client* creator) {
    Channel* channel = new Channel(name, creator);
    channels[name] = channel;
    return channel;
}

void Server::removeChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = channels.find(name);
    if (it != channels.end()) {
        delete it->second;
        channels.erase(it);
        std::cout << "Channel " << name << " removed" << std::endl;
    }
}

Client* Server::getClientByNickname(const std::string& nickname) {
    std::map<int, Client*>::iterator it;
    for (it = clients.begin(); it != clients.end(); ++it) {
        if (it->second->getNickname() == nickname)
            return it->second;
    }
    return NULL;
}

const std::string& Server::getPassword() const {
    return password;
}

void Server::executeCommand(Client* client, const std::string& commandStr) {
    Command command(commandStr);
    std::string cmd = toUpper(command.getCommand());
    int fd = client->getFd();
    
    std::cout << "Client " << fd << " sent command: " << cmd << std::endl;
    
    if (!client->isAuthenticated()) {
        // Authentication commands
        if (cmd == "PASS") {
            if (command.getParams().empty()) {
                sendToClient(fd, ":server 461 PASS :Not enough parameters");
                return;
            }
            
            if (command.getParams()[0] == password) {
                client->setPassOk(true);
                std::cout << "Client " << fd << " password accepted" << std::endl;
            } else {
                sendToClient(fd, ":server 464 :Password incorrect");
            }
        } else if (cmd == "NICK") {
            if (command.getParams().empty()) {
                sendToClient(fd, ":server 431 :No nickname given");
                return;
            }
            
            std::string nickname = command.getParams()[0];
            if (getClientByNickname(nickname)) {
                sendToClient(fd, ":server 433 " + nickname + " :Nickname is already in use");
                return;
            }
            
            client->setNickname(nickname);
            std::cout << "Client " << fd << " set nickname to " << nickname << std::endl;
            
            checkAuthentication(client);
        } else if (cmd == "USER") {
            if (command.getParams().size() < 4) {
                sendToClient(fd, ":server 461 USER :Not enough parameters");
                return;
            }
            
            client->setUsername(command.getParams()[0]);
            client->setRealname(command.getParams()[3]);
            std::cout << "Client " << fd << " set username to " << command.getParams()[0] << std::endl;
            
            checkAuthentication(client);
        } else {
            sendToClient(fd, ":server 451 :You have not registered");
        }
    } else {
        // Authenticated commands
        if (cmd == "JOIN") handleJoin(client, command);
        else if (cmd == "PRIVMSG") handlePrivmsg(client, command);
        else if (cmd == "KICK") handleKick(client, command);
        else if (cmd == "PART") handlePart(client, command);
        else if (cmd == "TOPIC") handleTopic(client, command);
        else if (cmd == "MODE") handleMode(client, command);
        else if (cmd == "INVITE") handleInvite(client, command);
        else if (cmd == "QUIT") handleQuit(client, command);
        else if (cmd == "PING") {
            std::string token = command.getParams().empty() ? "" : command.getParams()[0];
            sendToClient(fd, "PONG server " + token);
        } else {
            sendToClient(fd, ":server 421 " + cmd + " :Unknown command");
        }
    }
}

void Server::checkAuthentication(Client* client) {
    if (client->isPassOk() && !client->getNickname().empty() && !client->getUsername().empty()) {
        client->setAuthenticated(true);
        sendToClient(client->getFd(), ":server 001 " + client->getNickname() + 
                   " :Welcome to the IRC server " + client->getNickname() + "!");
    }
}

void Server::handleJoin(Client* client, const Command& command) {
    if (command.getParams().empty()) {
        sendToClient(client->getFd(), ":server 461 JOIN :Not enough parameters");
        return;
    }
    
    std::string channelName = command.getParams()[0];
    if (channelName[0] != '#') channelName = "#" + channelName;
    
    Channel* channel = getChannel(channelName);
    if (!channel) {
        channel = createChannel(channelName, client);
        std::cout << "Channel " << channelName << " created by " << client->getNickname() << std::endl;
    } else {
        // Check join restrictions
        if (command.getParams().size() > 1 && channel->hasPassword() && 
            command.getParams()[1] != channel->getPassword()) {
            sendToClient(client->getFd(), ":server 475 " + channelName + 
                       " :Cannot join channel (+k) - wrong key");
            return;
        }
        
        if (channel->isInviteOnly() && !channel->isInvited(client)) {
            sendToClient(client->getFd(), ":server 473 " + channelName + 
                       " :Cannot join channel (+i) - you must be invited");
            return;
        }
        
        if (channel->hasUserLimit() && 
            static_cast<int>(channel->getClients().size()) >= channel->getUserLimit()) {
            sendToClient(client->getFd(), ":server 471 " + channelName + 
                       " :Cannot join channel (+l) - channel is full");
            return;
        }
    }
    
    // Add client to channel
    channel->addClient(client);
    
    // Notify channel and send channel info
    std::string joinMsg = ":" + client->getNickname() + "!" + client->getUsername() + 
                        "@" + client->getIp() + " JOIN " + channelName;
    channel->broadcast(joinMsg, NULL);
    
    if (!channel->getTopic().empty()) {
        sendToClient(client->getFd(), ":server 332 " + client->getNickname() + " " + 
                   channelName + " :" + channel->getTopic());
    }
    
    // Send user list
    std::string names = ":server 353 " + client->getNickname() + " = " + channelName + " :";
    std::vector<Client*> clients = channel->getClients();
    for (size_t i = 0; i < clients.size(); ++i) {
        if (channel->isOperator(clients[i])) names += "@";
        names += clients[i]->getNickname() + " ";
    }
    
    sendToClient(client->getFd(), names);
    sendToClient(client->getFd(), ":server 366 " + client->getNickname() + " " + 
               channelName + " :End of NAMES list");
}

void Server::handlePrivmsg(Client* client, const Command& command) {
    if (command.getParams().size() < 2) {
        sendToClient(client->getFd(), ":server 461 PRIVMSG :Not enough parameters");
        return;
    }
    
    std::string target = command.getParams()[0];
    std::string message = command.getParams()[1];
    std::string prefix = ":" + client->getNickname() + "!" + client->getUsername() + 
                       "@" + client->getIp();
    
    if (target[0] == '#') {
        // Channel message
        Channel* channel = getChannel(target);
        if (!channel) {
            sendToClient(client->getFd(), ":server 403 " + target + " :No such channel");
            return;
        }
        
        if (!channel->hasClient(client)) {
            sendToClient(client->getFd(), ":server 404 " + target + " :Cannot send to channel");
            return;
        }
        
        channel->broadcast(prefix + " PRIVMSG " + target + " :" + message, client);
    } else {
        // Private message
        Client* targetClient = getClientByNickname(target);
        if (!targetClient) {
            sendToClient(client->getFd(), ":server 401 " + target + " :No such nick/channel");
            return;
        }
        
        sendToClient(targetClient->getFd(), prefix + " PRIVMSG " + target + " :" + message);
    }
}

void Server::handleKick(Client* client, const Command& command) {
    if (command.getParams().size() < 2) {
        sendToClient(client->getFd(), ":server 461 KICK :Not enough parameters");
        return;
    }
    
    std::string channelName = command.getParams()[0];
    std::string targetNick = command.getParams()[1];
    std::string reason = command.getParams().size() > 2 ? command.getParams()[2] : "No reason given";
    
    Channel* channel = getChannel(channelName);
    Client* targetClient = getClientByNickname(targetNick);
    
    if (!channel || !targetClient || !channel->isOperator(client) || !channel->hasClient(targetClient)) {
        if (!channel)
            sendToClient(client->getFd(), ":server 403 " + channelName + " :No such channel");
        else if (!channel->isOperator(client))
            sendToClient(client->getFd(), ":server 482 " + channelName + " :You're not channel operator");
        else if (!targetClient)
            sendToClient(client->getFd(), ":server 401 " + targetNick + " :No such nick/channel");
        else
            sendToClient(client->getFd(), ":server 441 " + targetNick + " " + channelName + 
                       " :They aren't on that channel");
        return;
    }
    
    // Broadcast kick and remove user
    std::string kickMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + 
                        client->getIp() + " KICK " + channelName + " " + targetNick + " :" + reason;
    channel->broadcast(kickMsg, NULL);
    channel->removeClient(targetClient);
}

void Server::handlePart(Client* client, const Command& command) {
    if (command.getParams().empty()) {
        sendToClient(client->getFd(), ":server 461 PART :Not enough parameters");
        return;
    }
    
    std::string channelName = command.getParams()[0];
    std::string reason = command.getParams().size() > 1 ? command.getParams()[1] : "Leaving";
    
    Channel* channel = getChannel(channelName);
    if (!channel || !channel->hasClient(client)) {
        if (!channel)
            sendToClient(client->getFd(), ":server 403 " + channelName + " :No such channel");
        else
            sendToClient(client->getFd(), ":server 442 " + channelName + " :You're not on that channel");
        return;
    }
    
    // Broadcast part and remove user
    std::string partMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + 
                        client->getIp() + " PART " + channelName + " :" + reason;
    channel->broadcast(partMsg, NULL);
    channel->removeClient(client);
    
    // Remove empty channel
    if (channel->getClients().empty())
        removeChannel(channelName);
}

void Server::handleTopic(Client* client, const Command& command) {
    if (command.getParams().empty()) {
        sendToClient(client->getFd(), ":server 461 TOPIC :Not enough parameters");
        return;
    }
    
    std::string channelName = command.getParams()[0];
    Channel* channel = getChannel(channelName);
    
    if (!channel || !channel->hasClient(client)) {
        if (!channel)
            sendToClient(client->getFd(), ":server 403 " + channelName + " :No such channel");
        else
            sendToClient(client->getFd(), ":server 442 " + channelName + " :You're not on that channel");
        return;
    }
    
    if (command.getParams().size() == 1) {
        // Query topic
        if (channel->getTopic().empty())
            sendToClient(client->getFd(), ":server 331 " + client->getNickname() + " " + 
                       channelName + " :No topic is set");
        else
            sendToClient(client->getFd(), ":server 332 " + client->getNickname() + " " + 
                       channelName + " :" + channel->getTopic());
    } else {
        // Set topic
        if (channel->isTopicRestricted() && !channel->isOperator(client)) {
            sendToClient(client->getFd(), ":server 482 " + channelName + " :You're not channel operator");
            return;
        }
        
        std::string newTopic = command.getParams()[1];
        channel->setTopic(newTopic);
        
        std::string topicMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + 
                             client->getIp() + " TOPIC " + channelName + " :" + newTopic;
        channel->broadcast(topicMsg, NULL);
    }
}

void Server::handleMode(Client* client, const Command& command) {
    if (command.getParams().empty()) {
        sendToClient(client->getFd(), ":server 461 MODE :Not enough parameters");
        return;
    }
    
    std::string target = command.getParams()[0];
    
    if (target[0] != '#') return; // Only handle channel modes
    
    Channel* channel = getChannel(target);
    if (!channel) {
        sendToClient(client->getFd(), ":server 403 " + target + " :No such channel");
        return;
    }
    
    if (command.getParams().size() == 1) {
        // Query modes
        std::string modes = "+";
        if (channel->isInviteOnly()) modes += "i";
        if (channel->isTopicRestricted()) modes += "t";
        if (channel->hasPassword()) modes += "k";
        if (channel->hasUserLimit()) modes += "l";
        
        sendToClient(client->getFd(), ":server 324 " + client->getNickname() + " " + target + " " + modes);
        return;
    }
    
    if (!channel->isOperator(client)) {
        sendToClient(client->getFd(), ":server 482 " + target + " :You're not channel operator");
        return;
    }
    
    // Process modes
    std::string modeStr = command.getParams()[1];
    std::string prefix = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIp();
    bool add = true;
    
    // C++98 compliant way to iterate through string
    for (size_t i = 0; i < modeStr.length(); ++i) {
        char c = modeStr[i];
        if (c == '+') add = true;
        else if (c == '-') add = false;
        else if (c == 'i') {
            channel->setInviteOnly(add);
            channel->broadcast(prefix + " MODE " + target + " " + (add ? "+" : "-") + "i", NULL);
        } else if (c == 't') {
            channel->setTopicRestricted(add);
            channel->broadcast(prefix + " MODE " + target + " " + (add ? "+" : "-") + "t", NULL);
        } else if (c == 'k' && ((add && command.getParams().size() > 2) || !add)) {
            if (add) {
                channel->setPassword(command.getParams()[2]);
                channel->broadcast(prefix + " MODE " + target + " +k " + command.getParams()[2], NULL);
            } else {
                channel->setPassword("");
                channel->broadcast(prefix + " MODE " + target + " -k", NULL);
            }
        } else if (c == 'l' && ((add && command.getParams().size() > 2) || !add)) {
            if (add) {
                int limit = std::atoi(command.getParams()[2].c_str());
                channel->setUserLimit(limit);
                channel->broadcast(prefix + " MODE " + target + " +l " + command.getParams()[2], NULL);
            } else {
                channel->setUserLimit(0);
                channel->broadcast(prefix + " MODE " + target + " -l", NULL);
            }
        } else if (c == 'o' && command.getParams().size() > 2) {
            std::string targetNick = command.getParams()[2];
            Client* targetClient = getClientByNickname(targetNick);
            
            if (!targetClient || !channel->hasClient(targetClient)) {
                sendToClient(client->getFd(), ":server 441 " + targetNick + " " + 
                           target + " :They aren't on that channel");
                continue;
            }
            
            if (add) channel->addOperator(targetClient);
            else channel->removeOperator(targetClient);
            
            channel->broadcast(prefix + " MODE " + target + " " + (add ? "+" : "-") + 
                             "o " + targetNick, NULL);
        }
    }
}

void Server::handleInvite(Client* client, const Command& command) {
    if (command.getParams().size() < 2) {
        sendToClient(client->getFd(), ":server 461 INVITE :Not enough parameters");
        return;
    }
    
    std::string targetNick = command.getParams()[0];
    std::string channelName = command.getParams()[1];
    
    Client* targetClient = getClientByNickname(targetNick);
    Channel* channel = getChannel(channelName);
    
    // Validation checks
    if (!targetClient || !channel || !channel->hasClient(client) || 
        (channel->isInviteOnly() && !channel->isOperator(client)) || 
        channel->hasClient(targetClient)) {
        
        if (!targetClient)
            sendToClient(client->getFd(), ":server 401 " + targetNick + " :No such nick/channel");
        else if (!channel)
            sendToClient(client->getFd(), ":server 403 " + channelName + " :No such channel");
        else if (!channel->hasClient(client))
            sendToClient(client->getFd(), ":server 442 " + channelName + " :You're not on that channel");
        else if (channel->isInviteOnly() && !channel->isOperator(client))
            sendToClient(client->getFd(), ":server 482 " + channelName + " :You're not channel operator");
        else
            sendToClient(client->getFd(), ":server 443 " + targetNick + " " + channelName + 
                       " :is already on channel");
        return;
    }
    
    // Add to invited list and send notifications
    channel->addInvited(targetClient);
    sendToClient(client->getFd(), ":server 341 " + client->getNickname() + " " + targetNick + " " + channelName);
    sendToClient(targetClient->getFd(), ":" + client->getNickname() + "!" + client->getUsername() + 
               "@" + client->getIp() + " INVITE " + targetNick + " :" + channelName);
}

void Server::handleQuit(Client* client, const Command& command) {
    std::string reason = command.getParams().empty() ? "Quit" : command.getParams()[0];
    std::string quitMsg = ":" + client->getNickname() + "!" + client->getUsername() + 
                        "@" + client->getIp() + " QUIT :" + reason;
    
    // Notify all channels this client is in - using C++98 iterator
    std::map<std::string, Channel*>::iterator it;
    for (it = channels.begin(); it != channels.end(); ++it) {
        if (it->second->hasClient(client))
            it->second->broadcast(quitMsg, client);
    }
    
    removeClient(client->getFd());
}