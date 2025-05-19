#include "../include/Command.hpp"
#include "../include/utils.hpp"

Command::Command(const std::string& raw) {
    std::string commandStr = raw;
    
    // Check for prefix
    if (!commandStr.empty() && commandStr[0] == ':') {
        size_t spacePos = commandStr.find(' ');
        if (spacePos != std::string::npos) {
            prefix = commandStr.substr(1, spacePos - 1);
            commandStr = commandStr.substr(spacePos + 1);
        }
    }
    
    // Extract command
    size_t spacePos = commandStr.find(' ');
    if (spacePos != std::string::npos) {
        command = commandStr.substr(0, spacePos);
        commandStr = commandStr.substr(spacePos + 1);
    } else {
        command = commandStr;
        commandStr = "";
    }
    
    // Extract parameters
    while (!commandStr.empty()) {
        if (commandStr[0] == ':') {
            // Last parameter (all the rest of the string)
            params.push_back(commandStr.substr(1));
            break;
        }
        
        spacePos = commandStr.find(' ');
        if (spacePos != std::string::npos) {
            std::string param = commandStr.substr(0, spacePos);
            if (!param.empty()) {
                params.push_back(param);
            }
            commandStr = commandStr.substr(spacePos + 1);
        } else {
            if (!commandStr.empty()) {
                params.push_back(commandStr);
            }
            break;
        }
    }
}

Command::~Command() {
}

const std::string& Command::getCommand() const {
    return command;
}

const std::vector<std::string>& Command::getParams() const {
    return params;
}

const std::string& Command::getPrefix() const {
    return prefix;
}