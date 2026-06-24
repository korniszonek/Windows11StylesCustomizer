#pragma once
#include <windows.h>

struct DynamicConfig {
    int radius = 12;
    int padding = 16;
    int iconSize = 32;
    int margins = 44;
    
    BYTE bgA = 220, bgR = 13, bgG = 16, bgB = 25;
    BYTE borderA = 255, borderR = 0, borderG = 240, borderB = 255;
    bool isDirty = true; 
};