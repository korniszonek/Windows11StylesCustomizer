#include <windows.h>
#include <iostream>
#include <string>
#include "pch.h" 

using namespace std;

#define PIPE_NAME L"\\\\.\\pipe\\WSM"

int main() {
    WSM_Log("PipeClient: Attempting to connect...");

    HANDLE hPipe = CreateFile(PIPE_NAME, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (hPipe != INVALID_HANDLE_VALUE) {
        const char* msg = "ALPHA:200";
        DWORD written;
        if (WriteFile(hPipe, msg, (DWORD)strlen(msg), &written, NULL)) {
            WSM_Log("PipeClient: Command sent successfully!");
            cout << "Command sent!" << endl;
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

    cout << "Press Enter to exit..." << endl;
    cin.get();
    return 0;
}