// CROP Graph Proxy.cpp: 定义应用程序的入口点。
//

#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <winhttp.h>

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
	PSECURITY_DESCRIPTOR pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
	if (pSD == NULL)
	{
		cout << "Failed to LocalAlloc." << endl;
		return NULL;
	}
	if (!InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION))
	{
		cout << "Failed to InitializeSecurityDescriptor." << endl;
		LocalFree((HLOCAL)pSD);
		return NULL;
	}
	if (!SetSecurityDescriptorDacl(pSD, TRUE, (PACL)NULL, FALSE))
	{
		cout << "Failed to SetSecurityDescriptorDacl." << endl;
		LocalFree((HLOCAL)pSD);
		return NULL;
	}
	SECURITY_ATTRIBUTES     sa;
	ZeroMemory(&sa, sizeof(SECURITY_ATTRIBUTES));
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = pSD;

	HANDLE hPipe;
	hPipe = CreateNamedPipe(
		name.c_str(),																		 // name of the pipe
		PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,											 // 1-way pipe -- send only
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, // send data as a byte stream
		1,																							 // only allow 1 instance of this pipe
		11240,																							 // no outbound buffer
		11240,																							 // no inbound buffer
		0,																							 // use default wait time
		&sa																						 // use default security attributes
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
				char buffer[10240]{};
				DWORD dwRead;
				if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &dwRead, NULL) != FALSE)
				{
					/* add terminating zero */
					buffer[dwRead] = '\0';
					/* do something with data in buffer */
					//cout << buffer << endl;
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

int httpRequest(string url, string port, string path, string headers, char *pBuffer, int *pSize)
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
				wstring(headers.begin(), headers.end()).c_str(), // headers to add
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

int resolveData(int *timestamp, int graphSize, unsigned char *graphBuffer, char *pBuffer, int *pSize)
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

	// copy to graph buffer
	if (decodedData.size() > graphSize)
	{
		cout << "Graph buffer is not enough." << endl;
		return -1;
	}

	memcpy(graphBuffer, decodedData.data(), decodedData.size());

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

string getenvwithdefault(const char* name, const string defaultValue)
{
	string string_variable;
	char const* temp = getenv(name);
	if (temp != NULL)
	{
		string_variable = std::string(temp);
	}
	else
	{
		string_variable = defaultValue;
	}
	return string_variable;
}

void requestThread(HANDLE hPipe)
{
	string url = getenvwithdefault("REQUEST_HOST", "localhost");
	string port = getenvwithdefault("REQUEST_PORT", "5182");
	string prefix = getenvwithdefault("REQUEST_PATH", "api/graph/simple");
	string station = getenvwithdefault("STATION", "test");
	bool test = getenvwithdefault("TEST", "false") == "true";
	test = true;
	string path = prefix + "?station=" + station;
	string header = test ? "CROP-PATH: graph/simple\r\nCROP-TEST: true" : "CROP-PATH: graph/simple\r\n";
	string interval = getenvwithdefault("INTERVAL", "1000");
	int intervalValue = stoi(interval);

	int deviceSize = 105;

	int deviceCount = readDeviceCount("graph\\graph.dat");
	if (deviceCount == -1)
	{
		cout << "Failed to read device count." << endl;
		deviceCount = 120;
	}
	cout << "Device Count:" << deviceCount << endl;

	char *buffer = new char[102400];
	int graphSize = 102400;
	unsigned char* graphBuffer = new unsigned char[graphSize];
	unsigned char* graphCache = new unsigned char[graphSize];
	memset(graphBuffer, 0, graphSize);
	memset(graphCache, 0, graphSize);

	int writeSize = 5120;
	unsigned char* writeBuffer = new unsigned char[writeSize];
	memset(writeBuffer, 0, writeSize);
	((DWORD *)writeBuffer)[0] = 0x00000000;
	((DWORD *)writeBuffer)[1] = (DWORD)128;
	((WORD *)writeBuffer)[4] = 0x0001;
	((WORD *)writeBuffer)[5] = 0x0001;
	((WORD *)writeBuffer)[6] = 0x0000;
	((WORD *)writeBuffer)[7] = 0x0000;

	int timestamp = 0;

	HANDLE hEventWrt = CreateEvent(NULL, TRUE, FALSE, NULL);
	OVERLAPPED OverLapWrt;
	memset(&OverLapWrt, 0, sizeof(OVERLAPPED));
	OverLapWrt.hEvent = hEventWrt;

	while (true)
	{
		int size = 0;
		int result = -1;

		result = httpRequest(url, port, path, header, buffer, &size);
		if (result == 0)
		{
			cout << "Response size:" << size << endl;
		}

		if (result == 0 && size > 0)
		{
			result = resolveData(&timestamp, graphSize, graphBuffer, buffer, &size);
		}

		if (result == 0 && size > 0 && hPipe != NULL && hPipe != INVALID_HANDLE_VALUE && clientConnected)
		{
			DWORD dwWritten;
			bool flag = false;

			for (int i = 0; i < deviceCount; i++)
			{
				if (memcmp(&graphBuffer[i * deviceSize], &graphCache[i * deviceSize], deviceSize) == 0)
				{
					continue;
				}
				memcpy(&graphCache[i * deviceSize], &graphBuffer[i * deviceSize], deviceSize);

				((DWORD*)writeBuffer)[0] = 0x00000000;
				((DWORD*)writeBuffer)[1] = (DWORD)(writeSize + 8);
				((WORD*)writeBuffer)[4] = 0x0001;
				((WORD*)writeBuffer)[5] = 0x0001;
				((WORD*)writeBuffer)[6] = (WORD)(i + 1);
				((WORD*)writeBuffer)[7] = (WORD)(writeSize);
				memcpy(&writeBuffer[16], &graphBuffer[i * deviceSize], deviceSize);
				if (!WriteFile(hPipe, writeBuffer, writeSize, &dwWritten, &OverLapWrt))
				{
					int error = GetLastError();
					if (error == ERROR_IO_PENDING)
					{
						WaitForSingleObject(hEventWrt, (DWORD)-1);
						i--;
						continue;
					}
					cout << "Failed to write to pipe:" << error << endl;
					flag = true;
					break;
				}
			}
		}

		Sleep(intervalValue);
	}

	delete[] buffer;
	delete[] graphBuffer;
	delete[] graphCache;
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
