#include "../include/utils.hpp"
#include <algorithm>
#include <cctype>

std::string toUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string trim(const std::string& str) {
    std::string result = str;
    
    // Trim leading whitespace
    result.erase(result.begin(), std::find_if(result.begin(), result.end(), 
        std::not1(std::ptr_fun<int, int>(std::isspace))));
    
    // Trim trailing whitespace
    result.erase(std::find_if(result.rbegin(), result.rend(), 
        std::not1(std::ptr_fun<int, int>(std::isspace))).base(), result.end());
    
    return result;
}