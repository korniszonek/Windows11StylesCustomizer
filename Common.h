#pragma once
#define WSM_LOG_PREFIX "korcuuW11SM: "
inline void WSM_Log(const char* msg) {
    std::string fullMsg = WSM_LOG_PREFIX + std::string(msg);
    OutputDebugStringA(fullMsg.c_str());
}