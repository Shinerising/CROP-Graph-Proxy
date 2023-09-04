// CROP Graph Proxy.cpp: 定义应用程序的入口点。
//

#include <iostream>
#include <Windows.h>
#include <string>

#include "CROP Graph Proxy.h"

using namespace std;

int startExecuteProcess(string path)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi)); // Start the child process. 
	if (!CreateProcess(NULL, // No module name (use command line)
		(LPSTR)path.c_str(), // Command line
		NULL, // Process handle not inheritable
		NULL, // Thread handle not inheritable
		FALSE, // Set handle inheritance to FALSE
		0, // No creation flags
		NULL, // Use parent's environment block
		NULL, // Use parent's starting directory
		&si, // Pointer to STARTUPINFO structure
		&pi) // Pointer to PROCESS_INFORMATION structure
		)
	{
		printf("CreateProcess failed (%d).\n", GetLastError());
		return -1;
	}
	// Wait until child process exits.
	WaitForSingleObject(pi.hProcess, INFINITE);
	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return 0;
}

HANDLE createNamedPipeline(string name)
{
	HANDLE hPipe;
	hPipe = CreateNamedPipe(
		name.c_str(), // name of the pipe
		PIPE_ACCESS_DUPLEX, // 1-way pipe -- send only
		PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT, // send data as a byte stream
		1, // only allow 1 instance of this pipe
		0, // no outbound buffer
		0, // no inbound buffer
		0, // use default wait time
		NULL // use default security attributes
	);
	if (hPipe == NULL || hPipe == INVALID_HANDLE_VALUE)
	{
		cout << "Failed to create outbound pipe instance.";
		// look up error code here using GetLastError()
		system("pause");
		return NULL;
	}
	return hPipe;
}

void messageThread()
{
	HANDLE hPipe;
	hPipe = createNamedPipeline("\\\\.\\pipe\\PIPE");
	while (true)
	{
		if (ConnectNamedPipe(hPipe, NULL) != FALSE) // wait for someone to connect to the pipe
		{
			cout << "connect success" << endl;
			while (true)
			{
				char buffer[1024];
				DWORD dwRead;
				if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &dwRead, NULL) != FALSE)
				{
					/* add terminating zero */
					buffer[dwRead] = '\0';
					/* do something with data in buffer */
					cout << buffer << endl;
				}
				if (dwRead == 0)
				{
					break;
				}
			}
		}
		DisconnectNamedPipe(hPipe);
	}
}

int startMessageThread()
{
	HANDLE hThread = CreateThread(
		NULL, // no security attribute
		0, // default stack size
		(LPTHREAD_START_ROUTINE)messageThread, // thread proc
		NULL, // thread parameter
		0, // not suspended
		NULL // returns thread ID
	);
	return 0;
}

int main()
{
	cout << "Hello world!" << endl;
	
	startMessageThread();
	startExecuteProcess("graph\\Graph.exe");

	system("pause");
	return 0;
}
