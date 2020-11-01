#pragma once
#include <iostream>
#include <string>
#include <thread>
#include <cstdlib>
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

class Client
{
private:
	constexpr static int PORT = 10240;
	constexpr static int BUFF_SIZE = 1024;
	const char* SERVER_ADDRESS = "10.100.240.12";
	char sendBuff[BUFF_SIZE] = {0};
	char recvBuff[BUFF_SIZE] = { 0 };

	bool clientQuit = false;				// �ͻ����Ƿ��˳�
	SOCKET socketServer = INVALID_SOCKET;	// ������socket

	bool startUp();
	void clear();
	void postRecv();
	void postSend();

public:
	Client() = default;
	~Client();

	void run(int argc, char* argv[]);
};

