#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include <set>
#include "Client.hpp"

class Channel {
private:
    std::string name;
    std::string topic;
    std::string password;
    std::vector<Client*> clients;
    std::set<Client*> operators;
    std::set<Client*> invited;
    bool inviteOnly;
    bool topicRestricted;
    int userLimit;
    
public:
    Channel(const std::string& name, Client* creator);
    ~Channel();
    
    // Getters
    const std::string& getName() const;
    const std::string& getTopic() const;
    const std::string& getPassword() const;
    const std::vector<Client*>& getClients() const;
    int getUserLimit() const;
    
    // Setters
    void setTopic(const std::string& topic);
    void setPassword(const std::string& password);
    void setInviteOnly(bool inviteOnly);
    void setTopicRestricted(bool restricted);
    void setUserLimit(int limit);
    
    // Client management
    void addClient(Client* client);
    void removeClient(Client* client);
    bool hasClient(Client* client) const;
    
    // Operator management
    void addOperator(Client* client);
    void removeOperator(Client* client);
    bool isOperator(Client* client) const;
    
    // Invite management
    void addInvited(Client* client);
    void removeInvited(Client* client);
    bool isInvited(Client* client) const;
    
    // Mode flags
    bool isInviteOnly() const;
    bool isTopicRestricted() const;
    bool hasPassword() const;
    bool hasUserLimit() const;
    
    // Messaging
    void broadcast(const std::string& message, Client* exclude);
};

#endif