#include "pch.h"
#include <windows.h>
#include <dwmapi.h>
#include <iostream>
#include <string>
#include <unordered_map>
#pragma comment(lib, "dwmapi.lib")
using namespace std;
enum class CommandType {
    RELOAD,
    UNKNOWN,
    ALPHA,
    COLOR,
    BLUR,
    HIDE    
};
static const unordered_map<string, CommandType> commandMap = {
    {"RELOAD",CommandType::RELOAD},
    {"ALPHA", CommandType::ALPHA},
    {"COLOR", CommandType::COLOR},
    {"BLUR",  CommandType::BLUR},
    {"HIDE",  CommandType::HIDE}
};
extern "C" __declspec(dllexport) void __stdcall ExecuteLogic(const char* command, HWND hwnd) {
    string fullCmd(command);
    size_t colonPos = fullCmd.find(':');

    std::string action = (colonPos != std::string::npos) ? fullCmd.substr(0, colonPos) : fullCmd;
    std::string value = (colonPos != std::string::npos) ? fullCmd.substr(colonPos + 1) : "";

    CommandType type = CommandType::UNKNOWN;
    if (commandMap.find(action) != commandMap.end()) {
        type = commandMap.at(action);
    }

    switch (type) {
    case CommandType::ALPHA:
        WSM_Log(("Logic: Setting Alpha to " + value).c_str());
        
        break;
    case CommandType::COLOR:
        WSM_Log(("Logic: Changing Color to " + value).c_str());
        
        break;
    case CommandType::BLUR:
       
        break;
    default:
        WSM_Log("Logic: Unknown command");

        break;
    }
    
}