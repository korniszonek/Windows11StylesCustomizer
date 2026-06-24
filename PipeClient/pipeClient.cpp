#include "pch.h"
#include <iostream>
#include <string>
#include <windows.h>
#include <vector>
using namespace std;

#define PIPE_NAME L"\\\\.\\pipe\\WSM"

int main() {
    cout << "--- Windows 11 Style Manager Pipe Client (Active) ---" << endl;
    cout << "Type a command (e.g., RELOAD, ALPHA:128) or 'EXIT' to quit:" << endl;

    string input;
    while (true) {
        cout << "> ";
        getline(cin, input);

        if (input == "EXIT") break;
        if (input.empty()) continue;

        WSM_Log(("PipeClient: Sending command: " + input).c_str());

        HANDLE hPipe = CreateFile(PIPE_NAME, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

        if (hPipe != INVALID_HANDLE_VALUE) {
            DWORD written;
            if (WriteFile(hPipe, input.c_str(), (DWORD)input.length(), &written, NULL)) {
                cout << "Command sent: " << input << endl;
            }
            else {
                WSM_Log("PipeClient: Failed to write to pipe!");
                cerr << "Failed to write!" << endl;
            }
            CloseHandle(hPipe);
        }
        else {
            WSM_Log("PipeClient: Could not connect to pipe!");
            cerr << "Could not connect to pipe! (Error: " << GetLastError() << ")" << endl;
        }
    }

    return 0;
}

