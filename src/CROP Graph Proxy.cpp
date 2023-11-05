// CROP Graph Proxy.cpp: 定义应用程序的入口点。
//

#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <winhttp.h>

#include "zlib/zlib.h"

#pragma comment(lib, "winhttp.lib")

#include "CROP Graph Proxy.h"

using namespace std;

static const string base64_chars =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";

static inline bool is_base64(BYTE c)
{
	return (isalnum(c) || (c == '+') || (c == '/'));
}

vector<BYTE> base64_decode(string const &encoded_string)
{
	int in_len = encoded_string.size();
	int i = 0;
	int j = 0;
	int in_ = 0;
	BYTE char_array_4[4], char_array_3[3];
	std::vector<BYTE> ret;

	while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
	{
		char_array_4[i++] = encoded_string[in_];
		in_++;
		if (i == 4)
		{
			for (i = 0; i < 4; i++)
				char_array_4[i] = base64_chars.find(char_array_4[i]);

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (i = 0; (i < 3); i++)
				ret.push_back(char_array_3[i]);
			i = 0;
		}
	}

	if (i)
	{
		for (j = i; j < 4; j++)
			char_array_4[j] = 0;

		for (j = 0; j < 4; j++)
			char_array_4[j] = base64_chars.find(char_array_4[j]);

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
		char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

		for (j = 0; (j < i - 1); j++)
			ret.push_back(char_array_3[j]);
	}

	return ret;
}

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
				char buffer[1024]{};
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

int httpRequest(string url, string port, string path, char *pBuffer, int *pSize)
{
	HINTERNET hSession{};
	HINTERNET hConnect{};
	HINTERNET hRequest{};
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
				stoi(port),															 // port
				0																					 // reserved
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
		BOOL bHeaderRequestSent = WinHttpAddRequestHeaders(
				hRequest,																 // request handle
				L"CROP-PATH: graph/simple\r\n", // headers to add
				(ULONG)-1L,																				 // string length
				WINHTTP_ADDREQ_FLAG_ADD						 // add if not exist
		);
		if (!bHeaderRequestSent)
		{
			cout << "Failed to add request headers." << endl;
			break;
		}
		BOOL bRequestSent = WinHttpSendRequest(
				hRequest, // request handle
				WINHTTP_NO_ADDITIONAL_HEADERS, // no headers
				0,														 // headers length
				WINHTTP_NO_REQUEST_DATA,			 // no body
				0,														 // body length
				0,														 // total length
				0														 // context
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
		char* head = pBuffer;
		*pSize = 0;
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
			// do something with data in buffer
			memcpy(head, pszOutBuffer, dwDownloaded);
			head += dwDownloaded;
			*pSize += dwDownloaded;

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

int resolveData(int deviceCount, int *timestamp, int decompressSize, unsigned char *decompressBuffer, char *pBuffer, int *pSize)
{
	string data = string(pBuffer, *pSize);
	string timeStr = data.substr(0, data.find_first_of("\n"));
	string base64Str = data.substr(data.find_first_of("\n") + 1);

	int originalTimestamp = *timestamp;
	*timestamp = stoi(timeStr);

	// if timestamp is not changed, then return
	if (*timestamp == originalTimestamp)
	{
		return -1;
	}

	// convert base64 to binary
	int deviceSize = 0;
	vector<BYTE> decodedData = base64_decode(base64Str);

	int inputSize = (decodedData[6] << 8) + decodedData[5];
	unsigned char *inputData = new unsigned char[inputSize];
	memcpy(inputData, &decodedData[7], inputSize);

	// decompress data
	memset(decompressBuffer, 0, decompressSize);
	int result = uncompress((unsigned char *)decompressBuffer, (unsigned long *)&decompressSize, (unsigned char *)inputData, inputSize);

	delete[] inputData;

	if (result != Z_OK)
	{
		cout << "Failed to decompress data." << endl;
		return -1;
	}

	return 0;
}

int readDeviceCount(string filename)
{
	FILE *fp = fopen(filename.c_str(), "rb");
	if (fp == NULL)
	{
		cout << "Failed to open file." << endl;
		return -1;
	}
	// read the 5th line
	char buffer[1024];
	for (int i = 0; i < 5; i++)
	{
		fgets(buffer, sizeof(buffer), fp);
	}
	// get data before ;
	string data = string(buffer);
	data = data.substr(0, data.find_first_of(";"));
	// get device count
	int deviceCount = stoi(data);
	fclose(fp);
	return deviceCount;
}

void requestThread(HANDLE hPipe)
{
	string url = getenv("REQUEST_HOST");
	string port = getenv("REQUEST_PORT");
	string prefix = getenv("REQUEST_PATH");
	string station = getenv("STATION");
	string path = prefix + "?station=" + station;

	int deviceSize = 120;

	int deviceCount = readDeviceCount("graph\\graph.dat");
	if (deviceCount == -1)
	{
		cout << "Failed to read device count." << endl;
		deviceCount = 120;
	}
	cout << "Device Count:" << deviceCount << endl;

	char *buffer = new char[102400];
	int decompressSize = 102400;
	unsigned char *decompressBuffer = new unsigned char[decompressSize];

	unsigned char *writeBuffer = new unsigned char[5120];
	((DWORD *)writeBuffer)[0] = 0x00000000;
	((DWORD *)writeBuffer)[1] = (DWORD)128;
	((WORD *)writeBuffer)[4] = 0x0001;
	((WORD *)writeBuffer)[5] = 0x0001;
	((WORD *)writeBuffer)[6] = 0x0000;
	((WORD *)writeBuffer)[7] = 0x0000;

	int timestamp = 0;

	while (true)
	{
		int size = 0;
		int result = -1;

		result = httpRequest(url, port, path, buffer, &size);
		if (result == 0)
		{
			cout << "Response size:" << size << endl;
		}

		if (result == 0 && size > 0)
		{
			result = resolveData(deviceCount, &timestamp, decompressSize, decompressBuffer, buffer, &size);
		}

		if (result == 0 && hPipe != NULL && hPipe != INVALID_HANDLE_VALUE && clientConnected)
		{
			DWORD dwWritten;
			bool flag = false;

			for (int i = 0; i < deviceCount; i++)
			{
				((DWORD *)writeBuffer)[1] = 0x00000000;
				memcpy(&writeBuffer[8], &decompressBuffer[i * deviceSize], deviceSize);
				if (!WriteFile(hPipe, writeBuffer, 5120, &dwWritten, NULL))
				{
					flag = true;
				}
			}

			if (flag)
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
		}

		Sleep(1000);
	}

	delete[] buffer;
	delete[] decompressBuffer;
	delete[] writeBuffer;
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
