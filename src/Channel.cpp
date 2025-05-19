#include "../include/Channel.hpp"
#include <algorithm>
#include <sys/socket.h>
#include <cerrno>
Channel::Channel(const std::string& name, Client* creator)
    : name(name), inviteOnly(false), topicRestricted(true), userLimit(0) {
    addClient(creator);
    addOperator(creator);
}

Channel::~Channel() {
}

const std::string& Channel::getName() const {
    return name;
}

const std::string& Channel::getTopic() const {
    return topic;
}

const std::string& Channel::getPassword() const {
    return password;
}

const std::vector<Client*>& Channel::getClients() const {
    return clients;
}

int Channel::getUserLimit() const {
    return userLimit;
}

void Channel::setTopic(const std::string& topic) {
    this->topic = topic;
}

void Channel::setPassword(const std::string& password) {
    this->password = password;
}

void Channel::setInviteOnly(bool inviteOnly) {
    this->inviteOnly = inviteOnly;
}

void Channel::setTopicRestricted(bool restricted) {
    this->topicRestricted = restricted;
}

void Channel::setUserLimit(int limit) {
    this->userLimit = limit;
}

void Channel::addClient(Client* client) {
    if (!hasClient(client)) {
        clients.push_back(client);
    }
}

void Channel::removeClient(Client* client) {
    clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
    removeOperator(client);
    removeInvited(client);
}

bool Channel::hasClient(Client* client) const {
    return std::find(clients.begin(), clients.end(), client) != clients.end();
}

void Channel::addOperator(Client* client) {
    operators.insert(client);
}

void Channel::removeOperator(Client* client) {
    operators.erase(client);
}

bool Channel::isOperator(Client* client) const {
    return operators.find(client) != operators.end();
}

void Channel::addInvited(Client* client) {
    invited.insert(client);
}

void Channel::removeInvited(Client* client) {
    invited.erase(client);
}

bool Channel::isInvited(Client* client) const {
    return invited.find(client) != invited.end();
}

bool Channel::isInviteOnly() const {
    return inviteOnly;
}

bool Channel::isTopicRestricted() const {
    return topicRestricted;
}

bool Channel::hasPassword() const {
    return !password.empty();
}

bool Channel::hasUserLimit() const {
    return userLimit > 0;
}

void Channel::broadcast(const std::string& message, Client* exclude) {
    for (size_t i = 0; i < clients.size(); ++i) {
        if (clients[i] != exclude) {
            send(clients[i]->getFd(), message.c_str(), message.size(), 0);
            
            // Add newline if not already present
            if (message.find("\r\n") == std::string::npos) {
                const char* newline = "\r\n";
                send(clients[i]->getFd(), newline, 2, 0);
            }
        }
    }
}