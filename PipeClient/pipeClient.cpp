#include <windows.h>
#include <iostream>

using namespace std;

#define PIPE_NAME L"\\\\.\\pipe\\WSM"

int main() {
	HANDLE hPipe = CreateFile(PIPE_NAME, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hPipe != INVALID_HANDLE_VALUE) {
		const char* msg = "ALPHA:200";
		DWORD written;
		WriteFile(hPipe, msg, strlen(msg), &written, NULL);
		CloseHandle(hPipe);
		cout << "Command sent!" << endl;
	}
	else {
		cerr << "Could not connet to pipe!" << endl;
	}
	cout << "Press Enter to exit..." << endl;
	cin.get();

	return 0;
}