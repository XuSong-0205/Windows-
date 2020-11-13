#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <WinSock2.h>
#include <mswsock.h>
#include <WS2tcpip.h>
#include <vector>
#include <list>
#include <unordered_map>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")
using namespace std;

class Server
{
private:
	constexpr static int PORT = 10240;				// �˿ں�
	constexpr static int BUFF_SIZE = 1024;			// ��������С
	const char* SERVER_ADDRESS = "10.100.240.12";	// ������IP��ַ
	char recvBuff[1024];							// ���յ���Ϣ
	char sendBuff[1024];							// ���͵���Ϣ

	struct SocketOverlapped
	{
		SOCKET sock;			// socket
		OVERLAPPED overlap;		// Overlapped
		int id = 0;				// ��socket��id
	};
	bool serverQuit = false;								// �������Ƿ��˳�
	HANDLE hPort = INVALID_HANDLE_VALUE;					// ��ɶ˿ڶ���
	list<SocketOverlapped>	sockOver;						// ������е�SocketOverlapped��Ϣ
	unordered_map<int, decltype(sockOver.begin())> soIt;	// key socket(int)	value list<SocketOverlapped>::iterator

private:
	Server();										// ˽�й��캯��
	bool startUp();									// ����������
	void clear();									// �����������Դ

	int postAccept();								// ���տͻ��˵�����
	int postRecv(int);								// ������Ϣ
	int postSend(int);								// ������Ϣ
	void sendMsg();									// ���ͻ��˷�����Ϣ

	friend DWORD WINAPI ThreadProc(LPVOID);			// �̵߳Ļص�����
public:
	Server(const Server&) = delete;
	Server(Server&&) = default;
	Server& operator=(const Server&) = delete;
	Server& operator=(Server&&) = default;
	~Server();

	static Server& getInstance();					// ��ȡ�����Ψһʵ��
	void run();										// �ⲿ���ýӿ�
};