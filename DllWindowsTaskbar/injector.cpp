#include "pch.h"
using namespace std;
int main() {
	DWORD processId = 28352;
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    const char* dllPath = R"(C:\Users\uszat\Documents\projekty\WindowsStylesManipulator\DllWindowsTaskbar\x64\Debug\DllWindowsTaskbar.dll)";
    LPVOID pRemoteMem = VirtualAllocEx(hProcess, NULL, strlen(dllPath) + 1, MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(hProcess, pRemoteMem, dllPath, strlen(dllPath) + 1, NULL);
    PTHREAD_START_ROUTINE pLoadLibrary = (PTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryA");
    CreateRemoteThread(hProcess, NULL, 0, pLoadLibrary, pRemoteMem, 0, NULL);

    cout << "DLL injected!" << endl;
    return 0;
}