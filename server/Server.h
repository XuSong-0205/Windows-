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
	constexpr static int PORT = 10240;				// 端口号
	constexpr static int BUFF_SIZE = 1024;			// 缓冲区大小
	const char* SERVER_ADDRESS = "10.100.240.12";	// 服务器IP地址
	char recvBuff[1024];							// 接收的信息
	char sendBuff[1024];							// 发送的信息

	struct SocketOverlapped
	{
		SOCKET sock;			// socket
		OVERLAPPED overlap;		// Overlapped
		int id = 0;				// 该socket的id
	};
	bool serverQuit = false;								// 服务器是否退出
	HANDLE hPort = INVALID_HANDLE_VALUE;					// 完成端口对象
	list<SocketOverlapped>	sockOver;						// 存放所有的SocketOverlapped信息
	unordered_map<int, decltype(sockOver.begin())> soIt;	// key socket(int)	value list<SocketOverlapped>::iterator

private:
	Server();										// 私有构造函数
	bool startUp();									// 启动服务器
	void clear();									// 清理服务器资源

	int postAccept();								// 接收客户端的连接
	int postRecv(int);								// 发送信息
	int postSend(int);								// 接收信息
	void sendMsg();									// 给客户端发送消息

	friend DWORD WINAPI ThreadProc(LPVOID);			// 线程的回调函数
public:
	Server(const Server&) = delete;
	Server(Server&&) = default;
	Server& operator=(const Server&) = delete;
	Server& operator=(Server&&) = default;
	~Server();

	static Server& getInstance();					// 获取该类的唯一实例
	void run();										// 外部调用接口
};