#include "Client.h"

Client::~Client() { clear(); }

// 开启客户端
bool Client::startUp()
{
	// 打开网络库
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "打开网络库失败：" << WSAGetLastError() << endl;
		return false;
	}

	// 校验版本号
	if (HIBYTE(wsaData.wVersion) != 2 || LOBYTE(wsaData.wVersion) != 2)
	{
		cout << "版本号错误：" << WSAGetLastError() << endl;
		return false;
	}

	// 创建套接字
	socketServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socketServer == INVALID_SOCKET)
	{
		cout << "创建socket失败：" << WSAGetLastError() << endl;
		return false;
	}

	// 初始化IP端口号和协议族信息
	SOCKADDR_IN addrServer;
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(PORT);
	inet_pton(AF_INET, SERVER_ADDRESS, &addrServer.sin_addr);

	cout << "客户端启动成功" << endl;

	// 连接服务器
	if (connect(socketServer, (SOCKADDR*)&addrServer, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		cout << "连接客户端失败：" << WSAGetLastError() << endl;
		return false;
	}

	cout << "连接服务器成功" << endl;

	return true;
}

// 清理资源
void Client::clear()
{
	if (socketServer) closesocket(socketServer);
	WSACleanup();
}

// 处理recv
void Client::postRecv()
{
	while (!clientQuit)
	{
		if(recv(socketServer, recvBuff, BUFF_SIZE, 0) == SOCKET_ERROR)
		{
			int error_code = WSAGetLastError();
			switch (error_code)
			{
			case 10053:
			case 10054:
				cout << "客户端已退出！" << endl;
				clientQuit = true;
				return;
			}
		}
		else
		{
			cout << "服务器 : " << recvBuff << endl;
		}
	}
}

// 处理send
void Client::postSend()
{
	while (!clientQuit)
	{
		cin.getline(sendBuff, BUFF_SIZE);
		if (sendBuff[0] == 0) continue;

		if (string("quit") == sendBuff)
		{
			clientQuit = true;
			return;
		}

		send(socketServer, sendBuff, BUFF_SIZE, 0);
	}
}

// 客户端主流程，外部调用接口
void Client::run(int argc, char* argv[])
{
	// 从命令行获取服务器IP地址
	if (argc == 2) SERVER_ADDRESS = argv[1];

	// 开启客户端
	if (!startUp()) return;

	// 开启一个线程去处理接收消息
	thread recvMsg(&Client::postRecv, this);
	recvMsg.detach();

	// 主线程处理发送消息
	postSend();
}