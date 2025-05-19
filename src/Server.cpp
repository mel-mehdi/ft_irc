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
#include <sys/socket.h>
#include <cstdlib>
Server::Server(int port, const std::string& password)
    : port(port), password(password), serverSocket(-1) {
}

Server::~Server() {
    // Close server socket
    if (serverSocket != -1)
        close(serverSocket);
    
    // Delete all clients
    for (std::map<int, Client*>::iterator it = clients.begin(); it != clients.end(); ++it)
        delete it->second;
    
    // Delete all channels
    for (std::map<std::string, Channel*>::iterator it = channels.begin(); it != channels.end(); ++it)
        delete it->second;
}

void Server::setupSocket() {
    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
        throw std::runtime_error("Failed to create socket");
    
    // Set socket options
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        throw std::runtime_error("Failed to set socket options");
    
    // Set non-blocking mode
    if (fcntl(serverSocket, F_SETFL, O_NONBLOCK) == -1)
        throw std::runtime_error("Failed to set non-blocking mode");
    
    // Bind socket
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1)
        throw std::runtime_error("Failed to bind socket");
    
    // Listen for connections
    if (listen(serverSocket, 10) == -1)
        throw std::runtime_error("Failed to listen on socket");
    
    std::cout << "Server listening on port " << port << std::endl;
    
    // Add server socket to poll array
    pollfd pfd;
    pfd.fd = serverSocket;
    pfd.events = POLLIN;
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
    
    // Set non-blocking mode for client socket
    if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1) {
        std::cerr << "Failed to set client socket to non-blocking mode" << std::endl;
        close(clientFd);
        return;
    }
    
    // Add client to poll array
    pollfd pfd;
    pfd.fd = clientFd;
    pfd.events = POLLIN;
    pollFds.push_back(pfd);
    
    // Create client object
    std::string clientIP = inet_ntoa(clientAddr.sin_addr);
    Client* client = new Client(clientFd, clientIP);
    clients[clientFd] = client;
    
    std::cout << "New client connected from " << clientIP << " (fd: " << clientFd << ")" << std::endl;
}

void Server::removeClient(int clientFd) {
    if (clients.find(clientFd) != clients.end()) {
        Client* client = clients[clientFd];
        
        // Leave all channels
        for (std::map<std::string, Channel*>::iterator it = channels.begin(); it != channels.end(); ++it) {
            it->second->removeClient(client);
        }
        
        // Remove empty channels
        std::vector<std::string> emptyChannels;
        for (std::map<std::string, Channel*>::iterator it = channels.begin(); it != channels.end(); ++it) {
            if (it->second->getClients().empty()) {
                emptyChannels.push_back(it->first);
            }
        }
        
        for (size_t i = 0; i < emptyChannels.size(); ++i) {
            removeChannel(emptyChannels[i]);
        }
        
        // Delete client
        std::cout << "Client disconnected (fd: " << clientFd << ")" << std::endl;
        delete client;
        clients.erase(clientFd);
    }
    
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
        if (bytesRead == 0) {
            // Client disconnected
            removeClient(clientFd);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // Error occurred
            std::cerr << "Error receiving data from client: " << strerror(errno) << std::endl;
            removeClient(clientFd);
        }
        return;
    }
    
    buffer[bytesRead] = '\0';
    client->appendBuffer(buffer);
    
    // Process completed commands
    std::vector<std::string> commands = client->getCompletedCommands();
    for (size_t i = 0; i < commands.size(); ++i) {
        executeCommand(client, commands[i]);
    }
}

void Server::start() {
    try {
        setupSocket();
        
        std::cout << "IRC Server started successfully!" << std::endl;
        
        while (true) {
            // Wait for events on the sockets
            int ready = poll(&pollFds[0], pollFds.size(), -1);
            
            if (ready == -1) {
                if (errno == EINTR)
                    continue;
                throw std::runtime_error("Poll failed");
            }
            
            // Check for events on server socket (new client connecting)
            if (pollFds[0].revents & POLLIN) {
                acceptClient();
            }
            
            // Check for events on client sockets
            for (size_t i = 1; i < pollFds.size(); ++i) {
                if (pollFds[i].revents & POLLIN) {
                    handleClientData(pollFds[i].fd);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
}

void Server::broadcast(const std::string& message, int excludeFd) {
    for (std::map<int, Client*>::iterator it = clients.begin(); it != clients.end(); ++it) {
        if (it->first != excludeFd && it->second->isAuthenticated()) {
            sendToClient(it->first, message);
        }
    }
}

void Server::sendToClient(int clientFd, const std::string& message) {
    std::string fullMessage = message;
    if (fullMessage.find("\r\n") == std::string::npos) {
        fullMessage += "\r\n";
    }
    
    send(clientFd, fullMessage.c_str(), fullMessage.size(), 0);
}

Channel* Server::getChannel(const std::string& name) {
    if (channels.find(name) != channels.end()) {
        return channels[name];
    }
    return NULL;
}

Channel* Server::createChannel(const std::string& name, Client* creator) {
    Channel* channel = new Channel(name, creator);
    channels[name] = channel;
    return channel;
}

void Server::removeChannel(const std::string& name) {
    if (channels.find(name) != channels.end()) {
        delete channels[name];
        channels.erase(name);
        std::cout << "Channel " << name << " removed" << std::endl;
    }
}

Client* Server::getClientByNickname(const std::string& nickname) {
    for (std::map<int, Client*>::iterator it = clients.begin(); it != clients.end(); ++it) {
        if (it->second->getNickname() == nickname) {
            return it->second;
        }
    }
    return NULL;
}

const std::string& Server::getPassword() const {
    return password;
}

void Server::executeCommand(Client* client, const std::string& commandStr) {
    Command command(commandStr);
    std::string cmd = toUpper(command.getCommand());
    
    std::cout << "Client " << client->getFd() << " sent command: " << cmd << std::endl;
    
    if (!client->isAuthenticated()) {
        // Only allow PASS, NICK, USER commands before authentication
        if (cmd == "PASS") {
            // Handle PASS command
            if (command.getParams().size() < 1) {
                sendToClient(client->getFd(), ":server 461 PASS :Not enough parameters");
                return;
            }
            
            std::string pass = command.getParams()[0];
            if (pass == password) {
                client->setPassOk(true);
                std::cout << "Client " << client->getFd() << " password accepted" << std::endl;
            } else {
                sendToClient(client->getFd(), ":server 464 :Password incorrect");
            }
        } else if (cmd == "NICK") {
            // Handle NICK command
            if (command.getParams().size() < 1) {
                sendToClient(client->getFd(), ":server 431 :No nickname given");
                return;
            }
            
            std::string nickname = command.getParams()[0];
            
            // Check if nickname is already in use
            if (getClientByNickname(nickname)) {
                sendToClient(client->getFd(), ":server 433 " + nickname + " :Nickname is already in use");
                return;
            }
            
            client->setNickname(nickname);
            std::cout << "Client " << client->getFd() << " set nickname to " << nickname << std::endl;
            
            // Check if client is now authenticated
            if (client->isPassOk() && !client->getUsername().empty()) {
                client->setAuthenticated(true);
                // Send welcome messages
                sendToClient(client->getFd(), ":server 001 " + nickname + " :Welcome to the IRC server " + nickname + "!");
            }
        } else if (cmd == "USER") {
            // Handle USER command
            if (command.getParams().size() < 4) {
                sendToClient(client->getFd(), ":server 461 USER :Not enough parameters");
                return;
            }
            
            std::string username = command.getParams()[0];
            std::string realname = command.getParams()[3];
            
            client->setUsername(username);
            client->setRealname(realname);
            std::cout << "Client " << client->getFd() << " set username to " << username << std::endl;
            
            // Check if client is now authenticated
            if (client->isPassOk() && !client->getNickname().empty()) {
                client->setAuthenticated(true);
                // Send welcome messages
                sendToClient(client->getFd(), ":server 001 " + client->getNickname() + " :Welcome to the IRC server " + client->getNickname() + "!");
            }
        } else {
            sendToClient(client->getFd(), ":server 451 :You have not registered");
        }
    } else {
        // Client is authenticated, handle all commands
        if (cmd == "JOIN") {
            // Handle JOIN command
            if (command.getParams().size() < 1) {
                sendToClient(client->getFd(), ":server 461 JOIN :Not enough parameters");
                return;
            }
            
            std::string channelName = command.getParams()[0];
            if (channelName[0] != '#') {
                channelName = "#" + channelName;
            }
            
            Channel* channel = getChannel(channelName);
            if (!channel) {
                // Create new channel
                channel = createChannel(channelName, client);
                std::cout << "Channel " << channelName << " created by " << client->getNickname() << std::endl;
            }
            
            // Check channel password if provided
            if (command.getParams().size() > 1 && channel->hasPassword()) {
                std::string password = command.getParams()[1];
                if (password != channel->getPassword()) {
                    sendToClient(client->getFd(), ":server 475 " + channelName + " :Cannot join channel (+k) - wrong key");
                    return;
                }
            }
            
            // Check if channel is invite-only
            if (channel->isInviteOnly() && !channel->isInvited(client)) {
                sendToClient(client->getFd(), ":server 473 " + channelName + " :Cannot join channel (+i) - you must be invited");
                return;
            }

			// check user has limit
            if (channel->hasUserLimit() && static_cast<int>(channel->getClients().size()) >= channel->getUserLimit()) {
				sendToClient(client->getFd(), ":server 471 " + channelName + " :Cannot join channel (+l) - channel is full");
				return;
			}
            
            // Add client to channel
            channel->addClient(client);
            
            // Send JOIN message to all clients in the channel
            std::string joinMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIp() + " JOIN " + channelName;
            channel->broadcast(joinMsg, NULL);
            
            // Send channel topic
            if (!channel->getTopic().empty()) {
                sendToClient(client->getFd(), ":server 332 " + client->getNickname() + " " + channelName + " :" + channel->getTopic());
            }
            
            // Send list of users in channel
            std::string names = ":server 353 " + client->getNickname() + " = " + channelName + " :";
            std::vector<Client*> channelClients = channel->getClients();
            for (size_t i = 0; i < channelClients.size(); ++i) {
                if (channel->isOperator(channelClients[i])) {
                    names += "@";
                }
                names += channelClients[i]->getNickname() + " ";
            }
            sendToClient(client->getFd(), names);
            sendToClient(client->getFd(), ":server 366 " + client->getNickname() + " " + channelName + " :End of NAMES list");
            
        } else if (cmd == "PRIVMSG") {
            // Handle PRIVMSG command
            if (command.getParams().size() < 2) {
                sendToClient(client->getFd(), ":server 461 PRIVMSG :Not enough parameters");
                return;
            }
            
            std::string target = command.getParams()[0];
            std::string message = command.getParams()[1];
            
            if (target[0] == '#') {
                // Message to channel
                Channel* channel = getChannel(target);
                if (!channel) {
                    sendToClient(client->getFd(), ":server 403 " + target + " :No such channel");
                    return;
                }
                
                // Check if client is in channel
                if (!channel->hasClient(client)) {
                    sendToClient(client->getFd(), ":server 404 " + target + " :Cannot send to channel");
                    return;
                }
                
                // Send message to all clients in channel except the sender
                std::string msg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIp() + " PRIVMSG " + target + " :" + message;
                channel->broadcast(msg, client);
            } else {
                // Private message to user
                Client* targetClient = getClientByNickname(target);
                if (!targetClient) {
                    sendToClient(client->getFd(), ":server 401 " + target + " :No such nick/channel");
                    return;
                }
                
                std::string msg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIp() + " PRIVMSG " + target + " :" + message;
                sendToClient(targetClient->getFd(), msg);
            }
        } else if (cmd == "KICK") {
            // Handle KICK command
            if (command.getParams().size() < 2) {
                sendToClient(client->getFd(), ":server 461 KICK :Not enough parameters");
                return;
            }
            
            std::string channelName = command.getParams()[0];
            std::string targetNick = command.getParams()[1];
            std::string reason = command.getParams().size() > 2 ? command.getParams()[2] : "No reason given";
            
            Channel* channel = getChannel(channelName);
            if (!channel) {
                sendToClient(client->getFd(), ":server 403 " + channelName + " :No such channel");
                return;
            }
            
            // Check if client is operator in channel
            if (!channel->isOperator(client)) {
                sendToClient(client->getFd(), ":server 482 " + channelName + " :You're not channel operator");
                return;
            }
            
            Client* targetClient = getClientByNickname(targetNick);
            if (!targetClient) {
                sendToClient(client->getFd(), ":server 401 " + targetNick + " :No such nick/channel");
                return;
            }
            
            if (!channel->hasClient(targetClient)) {
                sendToClient(client->getFd(), ":server 441 " + targetNick + " " + channelName + " :They aren't on that channel");
                return;
            }
            
            // Send KICK message to all clients in channel
            std::string kickMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIp() + " KICK " + channelName + " " + targetNick + " :" + reason;
            channel->broadcast(kickMsg, NULL);
            
            // Remove target client from channel
            channel->removeClient(targetClient);
            
        } else if (cmd == "PART") {
            // Handle PART command
            if (command.getParams().size() < 1) {
                sendToClient(client->getFd(), ":server 461 PART :Not enough parameters");
                return;
            }
            
            std::string channelName = command.getParams()[0];
            std::string reason = command.getParams().size() > 1 ? command.getParams()[1] : "Leaving";
            
            Channel* channel = getChannel(channelName);
            if (!channel) {
                sendToClient(client->getFd(), ":server 403 " + channelName + " :No such channel");
                return;
            }
            
            if (!channel->hasClient(client)) {
                sendToClient(client->getFd(), ":server 442 " + channelName + " :You're not on that channel");
                return;
            }
            
            // Send PART message to all clients in channel
            std::string partMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIp() + " PART " + channelName + " :" + reason;
            channel->broadcast(partMsg, NULL);
            
            // Remove client from channel
            channel->removeClient(client);
            
            // Remove channel if empty
            if (channel->getClients().empty()) {
                removeChannel(channelName);
            }
            
        } else if (cmd == "TOPIC") {
            // Handle TOPIC command
            if (command.getParams().size() < 1) {
                sendToClient(client->getFd(), ":server 461 TOPIC :Not enough parameters");
                return;
            }
            
            std::string channelName = command.getParams()[0];
            
            Channel* channel = getChannel(channelName);
            if (!channel) {
                sendToClient(client->getFd(), ":server 403 " + channelName + " :No such channel");
                return;
            }
            
            if (!channel->hasClient(client)) {
                sendToClient(client->getFd(), ":server 442 " + channelName + " :You're not on that channel");
                return;
            }
            
            if (command.getParams().size() == 1) {
                // User is querying the topic
                if (channel->getTopic().empty()) {
                    sendToClient(client->getFd(), ":server 331 " + client->getNickname() + " " + channelName + " :No topic is set");
                } else {
                    sendToClient(client->getFd(), ":server 332 " + client->getNickname() + " " + channelName + " :" + channel->getTopic());
                }
            } else {
                // User is trying to set the topic
                if (channel->isTopicRestricted() && !channel->isOperator(client)) {
                    sendToClient(client->getFd(), ":server 482 " + channelName + " :You're not channel operator");
                    return;
                }
                
                std::string newTopic = command.getParams()[1];
                channel->setTopic(newTopic);
                
                // Notify all clients in channel
                std::string topicMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIp() + " TOPIC " + channelName + " :" + newTopic;
                channel->broadcast(topicMsg, NULL);
            }
            
        } else if (cmd == "MODE") {
            // Handle MODE command
            if (command.getParams().size() < 1) {
                sendToClient(client->getFd(), ":server 461 MODE :Not enough parameters");
                return;
            }
            
            std::string target = command.getParams()[0];
            
            if (target[0] == '#') {
                // Channel mode
                Channel* channel = getChannel(target);
                if (!channel) {
                    sendToClient(client->getFd(), ":server 403 " + target + " :No such channel");
                    return;
                }
                
                if (command.getParams().size() == 1) {
                    // Query channel modes
                    std::string modes = "+";
                    if (channel->isInviteOnly()) modes += "i";
                    if (channel->isTopicRestricted()) modes += "t";
                    if (channel->hasPassword()) modes += "k";
                    if (channel->hasUserLimit()) modes += "l";
                    
                    sendToClient(client->getFd(), ":server 324 " + client->getNickname() + " " + target + " " + modes);
                    return;
                }
                
                // Check if client is operator in channel
                if (!channel->isOperator(client)) {
                    sendToClient(client->getFd(), ":server 482 " + target + " :You're not channel operator");
                    return;
                }
                
                std::string modeStr = command.getParams()[1];
                bool add = true;
                
                for (size_t i = 0; i < modeStr.size(); ++i) {
                    char c = modeStr[i];
                    
                    if (c == '+') {
                        add = true;
                    } else if (c == '-') {
                        add = false;
                    } else if (c == 'i') {
                        channel->setInviteOnly(add);
                        std::string modeMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIp() + " MODE " + target + " " + (add ? "+" : "-") + "i";
                        channel->broadcast(modeMsg, NULL);
                    } else if (c == 't') {
                        channel->setTopicRestricted(add);
                        std::string modeMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIp() + " MODE " + target + " " + (add ? "+" : "-") + "t";
                        channel->broadcast(modeMsg, NULL);
                    } else if (c == 'k') {
                        if (add && command.getParams().size() > 2) {
                            channel->setPassword(command.getParams()[2]);
                            std::string modeMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIp() + " MODE " + target + " +k " + command.getParams()[2];
                            channel->broadcast(modeMsg, NULL);
                        } else if (!add) {
                            channel->setPassword("");
                            std::string modeMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIp() + " MODE " + target + " -k";
                            channel->broadcast(modeMsg, NULL);
                        }
                    } else if (c == 'l') {
                        if (add && command.getParams().size() > 2) {
                            int limit = std::atoi(command.getParams()[2].c_str());
                            channel->setUserLimit(limit);
                            std::string modeMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIp() + " MODE " + target + " +l " + command.getParams()[2];
                            channel->broadcast(modeMsg, NULL);
                        } else if (!add) {
                            channel->setUserLimit(0);
                            std::string modeMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIp() + " MODE " + target + " -l";
                            channel->broadcast(modeMsg, NULL);
                        }
                    } else if (c == 'o') {
                        if (command.getParams().size() > 2) {
                            std::string targetNick = command.getParams()[2];
                            Client* targetClient = getClientByNickname(targetNick);
                            
                            if (!targetClient || !channel->hasClient(targetClient)) {
                                sendToClient(client->getFd(), ":server 441 " + targetNick + " " + target + " :They aren't on that channel");
                                continue;
                            }
                            
                            if (add) {
                                channel->addOperator(targetClient);
                            } else {
                                channel->removeOperator(targetClient);
                            }
                            
                            std::string modeMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIp() + " MODE " + target + " " + (add ? "+" : "-") + "o " + targetNick;
                            channel->broadcast(modeMsg, NULL);
                        }
                    }
                }
            }
            
        } else if (cmd == "INVITE") {
            // Handle INVITE command
            if (command.getParams().size() < 2) {
                sendToClient(client->getFd(), ":server 461 INVITE :Not enough parameters");
                return;
            }
            
            std::string targetNick = command.getParams()[0];
            std::string channelName = command.getParams()[1];
            
            Client* targetClient = getClientByNickname(targetNick);
            if (!targetClient) {
                sendToClient(client->getFd(), ":server 401 " + targetNick + " :No such nick/channel");
                return;
            }
            
            Channel* channel = getChannel(channelName);
            if (!channel) {
                sendToClient(client->getFd(), ":server 403 " + channelName + " :No such channel");
                return;
            }
            
            if (!channel->hasClient(client)) {
                sendToClient(client->getFd(), ":server 442 " + channelName + " :You're not on that channel");
                return;
            }
            
            if (channel->isInviteOnly() && !channel->isOperator(client)) {
                sendToClient(client->getFd(), ":server 482 " + channelName + " :You're not channel operator");
                return;
            }
            
            if (channel->hasClient(targetClient)) {
                sendToClient(client->getFd(), ":server 443 " + targetNick + " " + channelName + " :is already on channel");
                return;
            }
            
            // Add target to invited list
            channel->addInvited(targetClient);
            
            // Notify the inviter
            sendToClient(client->getFd(), ":server 341 " + client->getNickname() + " " + targetNick + " " + channelName);
            
            // Notify the invitee
            std::string inviteMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIp() + " INVITE " + targetNick + " :" + channelName;
            sendToClient(targetClient->getFd(), inviteMsg);
            
        } else if (cmd == "QUIT") {
            // Handle QUIT command
            std::string reason = command.getParams().size() > 0 ? command.getParams()[0] : "Quit";
            
            // Notify all channels this client is in
            for (std::map<std::string, Channel*>::iterator it = channels.begin(); it != channels.end(); ++it) {
                Channel* channel = it->second;
                if (channel->hasClient(client)) {
                    std::string quitMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIp() + " QUIT :" + reason;
                    channel->broadcast(quitMsg, client);
                }
            }
            
            // Disconnect client
            removeClient(client->getFd());
        } else if (cmd == "PING") {
            // Handle PING command
            std::string token = command.getParams().size() > 0 ? command.getParams()[0] : "";
            sendToClient(client->getFd(), "PONG server " + token);
        } else {
            // Unknown command
            sendToClient(client->getFd(), ":server 421 " + cmd + " :Unknown command");
        }
    }
}