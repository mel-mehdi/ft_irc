#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <string>
#include <vector>

class Command {
private:
    std::string command;
    std::vector<std::string> params;
    std::string prefix;
    
public:
    Command(const std::string& raw);
    ~Command();
    
    const std::string& getCommand() const;
    const std::vector<std::string>& getParams() const;
    const std::string& getPrefix() const;
};

#endif