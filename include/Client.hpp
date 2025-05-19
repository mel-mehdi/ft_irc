#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>

class Client {
private:
    int fd;
    std::string ip;
    std::string nickname;
    std::string username;
    std::string realname;
    bool authenticated;
    bool passOk;
    std::string buffer;
    
public:
    Client(int fd, const std::string& ip);
    ~Client();
    
    // Getters
    int getFd() const;
    const std::string& getIp() const;
    const std::string& getNickname() const;
    const std::string& getUsername() const;
    const std::string& getRealname() const;
    bool isAuthenticated() const;
    bool isPassOk() const;
    
    // Setters
    void setNickname(const std::string& nickname);
    void setUsername(const std::string& username);
    void setRealname(const std::string& realname);
    void setAuthenticated(bool authenticated);
    void setPassOk(bool passOk);
    
    // Buffer management
    void appendBuffer(const std::string& data);
    std::vector<std::string> getCompletedCommands();
};

#endif