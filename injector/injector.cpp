#include <windows.h>
#include <iostream>
#include <tlhelp32.h>

DWORD GetExplorerPid() {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 process;
    process.dwSize = sizeof(process);

    if (Process32First(snapshot, &process)) {
        while (Process32Next(snapshot, &process)) {
            if (_wcsicmp(process.szExeFile, L"explorer.exe") == 0) {
                pid = process.th32ProcessID;
                break;
            }
        }
    }
    CloseHandle(snapshot);
    return pid;
}

using namespace std;

int main() {
    DWORD processId = GetExplorerPid();
    if (processId == 0) {
        cerr << "Explorer.exe process ID not found!" << endl;
        return 1;
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (hProcess == NULL) {
        cerr << "OpenProcess failed! Error code: " << GetLastError() << endl;
        return 1;
    }

    const char* dllPath = R"(C:\Users\uszat\Documents\projekty\WindowsStylesManipulator\DllWindowsTaskbar\x64\Debug\DllWindowsTaskbar.dll)";
    LPVOID pRemoteMem = VirtualAllocEx(hProcess, NULL, strlen(dllPath) + 1, MEM_COMMIT, PAGE_READWRITE);
    if (pRemoteMem == NULL) {
        cerr << "VirtualAllocEx failed! Error code: " << GetLastError() << endl;
        CloseHandle(hProcess);
        return 1;
    }

    if (!WriteProcessMemory(hProcess, pRemoteMem, dllPath, strlen(dllPath) + 1, NULL)) {
        cerr << "WriteProcessMemory failed! Error code: " << GetLastError() << endl;
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }

    PTHREAD_START_ROUTINE pLoadLibrary = (PTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryA");
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, pLoadLibrary, pRemoteMem, 0, NULL);
    if (hThread == NULL) {
        cerr << "CreateRemoteThread failed! Error code: " << GetLastError() << endl;
    }
    else {
        cout << "DLL injected successfully!" << endl;
        CloseHandle(hThread);
    }

    CloseHandle(hProcess);
    return 0;
}