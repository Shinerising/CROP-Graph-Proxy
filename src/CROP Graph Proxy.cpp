// CROP Graph Proxy.cpp: 定义应用程序的入口点。
//

#include <iostream>
#include <string>
#include <windows.h>
#include <winhttp.h>

#include "CROP Graph Proxy.h"

using namespace std;

int startExecuteProcess(string path)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));						// Start the child process.
	if (!CreateProcess(NULL,								// No module name (use command line)
										 (LPSTR)path.c_str(), // Command line
										 NULL,								// Process handle not inheritable
										 NULL,								// Thread handle not inheritable
										 FALSE,								// Set handle inheritance to FALSE
										 0,										// No creation flags
										 NULL,								// Use parent's environment block
										 NULL,								// Use parent's starting directory
										 &si,									// Pointer to STARTUPINFO structure
										 &pi)									// Pointer to PROCESS_INFORMATION structure
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
			name.c_str(),																		 // name of the pipe
			PIPE_ACCESS_DUPLEX,															 // 1-way pipe -- send only
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, // send data as a byte stream
			1,																							 // only allow 1 instance of this pipe
			0,																							 // no outbound buffer
			0,																							 // no inbound buffer
			0,																							 // use default wait time
			NULL																						 // use default security attributes
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

bool clientConnected = false;
void messageThread(HANDLE hPipe)
{
	while (true)
	{
		if (ConnectNamedPipe(hPipe, NULL) != FALSE) // wait for someone to connect to the pipe
		{
			clientConnected = true;
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
		clientConnected = false;
	}
}

int httpRequest(string url, string path, char *pBuffer, int *pSize)
{
	HINTERNET hSession;
	HINTERNET hConnect;
	HINTERNET hRequest;
	int flag = -1;
	while (true)
	{
		hSession = WinHttpOpen(
				L"curl",													 // user agent
				WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, // default access type
				WINHTTP_NO_PROXY_NAME,						 // default proxy name
				WINHTTP_NO_PROXY_BYPASS,					 // default proxy bypass
				0																	 // reserved
		);
		if (hSession == NULL)
		{
			cout << "Failed to open session." << endl;
			break;
		}
		hConnect = WinHttpConnect(
				hSession,																 // session handle
				wstring(url.begin(), url.end()).c_str(), // host name,
				INTERNET_DEFAULT_HTTP_PORT,							 // default port
				0																				 // reserved
		);
		if (hConnect == NULL)
		{
			cout << "Failed to connect." << endl;
			break;
		}
		hRequest = WinHttpOpenRequest(
				hConnect,																	 // connection handle
				L"GET",																		 // request method
				wstring(path.begin(), path.end()).c_str(), // request URI
				NULL,																			 // version
				WINHTTP_NO_REFERER,												 // referrer
				WINHTTP_DEFAULT_ACCEPT_TYPES,							 // accept types
				0																					 // reserved
		);
		if (hRequest == NULL)
		{
			cout << "Failed to open request." << endl;
			break;
		}
		BOOL bRequestSent = WinHttpSendRequest(
				hRequest,											 // request handle
				WINHTTP_NO_ADDITIONAL_HEADERS, // no headers
				0,														 // headers length
				WINHTTP_NO_REQUEST_DATA,			 // no body
				0,														 // body length
				0,														 // total length
				0															 // context
		);
		if (!bRequestSent)
		{
			cout << "Failed to send request." << endl;
			break;
		}
		BOOL bResponseReceived = WinHttpReceiveResponse(
				hRequest, // request handle
				NULL			// reserved
		);
		if (!bResponseReceived)
		{
			cout << "Failed to receive response." << endl;
			break;
		}
		DWORD dwSize = 0;
		BOOL bResult = WinHttpQueryDataAvailable(
				hRequest, // request handle
				&dwSize		// size available
		);
		if (!bResult)
		{
			cout << "Failed to query data." << endl;
			break;
		}
		// Report any errors.
		DWORD dwStatusCode = 0;
		DWORD dwSizeStatusCode = sizeof(dwStatusCode);
		bResult = WinHttpQueryHeaders(
				hRequest,																							 // request handle
				WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, // get the status code
				NULL,																									 // name
				&dwStatusCode,																				 // value
				&dwSizeStatusCode,																		 // size
				NULL																									 // reserved
		);
		if (!bResult)
		{
			cout << "Failed to query status code." << endl;
			break;
		}
		if (dwStatusCode != HTTP_STATUS_OK)
		{
			cout << "Status code is not OK:" << dwStatusCode << endl;
			break;
		}

		DWORD dwDownloaded = 0;
		while (dwSize > 0)
		{
			// Allocate space for the buffer.
			char *pszOutBuffer = new char[dwSize + 1];
			if (!pszOutBuffer)
			{
				cout << "Out of memory." << endl;
				break;
			}
			// Read the data.
			ZeroMemory(pszOutBuffer, dwSize);
			bResult = WinHttpReadData(
					hRequest,							// request handle
					(LPVOID)pszOutBuffer, // the data
					dwSize,								// number of bytes to read
					&dwDownloaded					// number of bytes read
			);
			if (!bResult)
			{
				cout << "Failed to read data." << endl;
				break;
			}
			// copy data to buffer
			memcpy(pBuffer, pszOutBuffer, dwSize);
			*pSize = dwSize;
			// Free the memory allocated to the buffer.
			delete[] pszOutBuffer;
			dwSize = 0;
			bResult = WinHttpQueryDataAvailable(
					hRequest, // request handle
					&dwSize		// size available
			);
			if (!bResult)
			{
				cout << "Failed to query data." << endl;
				break;
			}
		}
		flag = 0;
		break;
	}
	// Close any open handles.
	if (hRequest)
		WinHttpCloseHandle(hRequest);
	if (hConnect)
		WinHttpCloseHandle(hConnect);
	if (hSession)
		WinHttpCloseHandle(hSession);
	return 0;
}

void requestThread(HANDLE hPipe)
{
	string value = getenv("REQUEST_URI");
	int pos = value.find_first_of('/');
	string url = pos == string::npos ? value : value.substr(0, pos);
	string path = pos == string::npos ? "/" : value.substr(pos);

	while (true)
	{
		char buffer[102400];
		int size = 0;
		int result = -1;
		memset(buffer, 0, sizeof(buffer));

		result = httpRequest(url, path, buffer, &size);

		if (result == 0)
		{
			cout << "Response size:" << size << endl;
		}

		if (result == 0 && size > 0 && hPipe != NULL && hPipe != INVALID_HANDLE_VALUE && clientConnected)
		{
			DWORD dwWritten;
			if (!WriteFile(hPipe, buffer, size, &dwWritten, NULL))
			{
				cout << "Failed to write to pipe." << endl;
			}
		}

		Sleep(1000);
	}
}

int startMessageThread(HANDLE hPipe)
{
	HANDLE hThread = CreateThread(
			NULL,																	 // no security attribute
			0,																		 // default stack size
			(LPTHREAD_START_ROUTINE)messageThread, // thread proc
			hPipe,																 // thread parameter
			0,																		 // not suspended
			NULL																	 // returns thread ID
	);
	return 0;
}

int startRequestThread(HANDLE hPipe)
{
	HANDLE hThread = CreateThread(
			NULL,																	 // no security attribute
			0,																		 // default stack size
			(LPTHREAD_START_ROUTINE)requestThread, // thread proc
			hPipe,																 // thread parameter
			0,																		 // not suspended
			NULL																	 // returns thread ID
	);
	return 0;
}

int main()
{
	cout << "GraphProxy started!" << endl;

	HANDLE hPipe;
	hPipe = createNamedPipeline("\\\\.\\pipe\\PIPE");
	if (hPipe == NULL)
	{
		return -1;
	}
	cout << "Pipe created!" << endl;

	startMessageThread(hPipe);
	// startExecuteProcess("graph\\Graph.exe");
	startRequestThread(hPipe);

	// system("pause");

	// If esc pressed then stop
	while (true)
	{
		if (GetAsyncKeyState(VK_ESCAPE))
		{
			break;
		}
	}

	// Close the pipe (automatically disconnects client too)
	CloseHandle(hPipe);
	return 0;
}
