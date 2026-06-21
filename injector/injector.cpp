#include <windows.h>
#include <iostream>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include "pch.h" 

using namespace std;

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

int main() {
    DWORD processId = GetExplorerPid();
    if (processId == 0) {
        WSM_Log("Explorer.exe process ID not found!");
        return 1;
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (hProcess == NULL) {
        WSM_Log("OpenProcess failed!");
        return 1;
    }

    WCHAR fullPath[MAX_PATH];
    GetFullPathName(L"DllWindowsTaskbar.dll", MAX_PATH, fullPath, NULL);
    wstring finalDllPath = fullPath;

    size_t pathSize = (finalDllPath.length() + 1) * sizeof(WCHAR);
    LPVOID pRemoteMem = VirtualAllocEx(hProcess, NULL, pathSize, MEM_COMMIT, PAGE_READWRITE);

    if (pRemoteMem == NULL) {
        WSM_Log("VirtualAllocEx failed!");
        CloseHandle(hProcess);
        return 1;
    }

    if (!WriteProcessMemory(hProcess, pRemoteMem, finalDllPath.c_str(), pathSize, NULL)) {
        WSM_Log("WriteProcessMemory failed!");
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }

    PTHREAD_START_ROUTINE pLoadLibrary = (PTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryW");
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, pLoadLibrary, pRemoteMem, 0, NULL);

    if (hThread == NULL) {
        WSM_Log("CreateRemoteThread failed!");
    }
    else {
        WaitForSingleObject(hThread, 2000);
        DWORD exitCode = 0;
        GetExitCodeThread(hThread, &exitCode);

        string logMsg = "Thread finished with code: " + to_string(exitCode);
        WSM_Log(logMsg.c_str());

        CloseHandle(hThread);
    }

    CloseHandle(hProcess);
    return 0;
}