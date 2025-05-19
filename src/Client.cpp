#include "../include/Client.hpp"

Client::Client(int fd, const std::string& ip)
    : fd(fd), ip(ip), authenticated(false), passOk(false) {
}

Client::~Client() {
}

int Client::getFd() const {
    return fd;
}

const std::string& Client::getIp() const {
    return ip;
}

const std::string& Client::getNickname() const {
    return nickname;
}

const std::string& Client::getUsername() const {
    return username;
}

const std::string& Client::getRealname() const {
    return realname;
}

bool Client::isAuthenticated() const {
    return authenticated;
}

bool Client::isPassOk() const {
    return passOk;
}

void Client::setNickname(const std::string& nickname) {
    this->nickname = nickname;
}

void Client::setUsername(const std::string& username) {
    this->username = username;
}

void Client::setRealname(const std::string& realname) {
    this->realname = realname;
}

void Client::setAuthenticated(bool authenticated) {
    this->authenticated = authenticated;
}

void Client::setPassOk(bool passOk) {
    this->passOk = passOk;
}

void Client::appendBuffer(const std::string& data) {
    buffer += data;
}

std::vector<std::string> Client::getCompletedCommands() {
    std::vector<std::string> commands;
    
    size_t pos = 0;
    while ((pos = buffer.find("\r\n", pos)) != std::string::npos) {
        std::string cmd = buffer.substr(0, pos);
        commands.push_back(cmd);
        buffer.erase(0, pos + 2);
        pos = 0;
    }
    
    // Alternative for just \n (some clients may not send \r\n)
    pos = 0;
    while ((pos = buffer.find('\n', pos)) != std::string::npos) {
        if (pos > 0 && buffer[pos - 1] == '\r') {
            // Already handled above
            pos++;
            continue;
        }
        
        std::string cmd = buffer.substr(0, pos);
        commands.push_back(cmd);
        buffer.erase(0, pos + 1);
        pos = 0;
    }
    
    return commands;
}