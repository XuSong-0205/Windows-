#include "Server.h"

Server::Server() : recvBuff{ 0 }, sendBuff{ 0 }{}

Server::~Server() { clear(); }

// 启动服务器
bool Server::startUp()
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
		cout << "网络库版本错误：" << WSAGetLastError() << endl;
		WSACleanup();
		return false;
	}

	// 创建 socket
	SOCKET socketServer = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (socketServer == INVALID_SOCKET)
	{
		cout << "创建socket失败：" << WSAGetLastError() << endl;
		WSACleanup();
		return false;
	}

	// 初始化IP端口号和协议族信息
	SOCKADDR_IN addrServer;
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(PORT);
	inet_pton(AF_INET, SERVER_ADDRESS, &addrServer.sin_addr);

	// 绑定socket
	if (bind(socketServer, (SOCKADDR*)&addrServer, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		cout << "绑定socket失败：" << WSAGetLastError() << endl;
		closesocket(socketServer);
		WSACleanup();
		return false;
	}

	// 创建完成端口
	hPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	if (hPort == 0)
	{
		cout << "创建完成端口失败：" << GetLastError() << endl;
		closesocket(socketServer);
		WSACleanup();
		return false;
	}

	// 绑定完成端口和服务器socket
	if (hPort != CreateIoCompletionPort((HANDLE)socketServer, hPort, 0, 0))
	{
		cout << "绑定完成端口失败：" << GetLastError() << endl;
		CloseHandle(hPort);
		closesocket(socketServer);
		WSACleanup();
		return false;
	}

	// 监听
	if (listen(socketServer, SOMAXCONN) == SOCKET_ERROR)
	{
		cout << "监听失败：" << WSAGetLastError() << endl;
		CloseHandle(hPort);
		closesocket(socketServer);
		WSACleanup();
		return false;
	}

	// 将服务器SocketOverlapped添加到链表中
	sockOver.emplace_back(SocketOverlapped{});
	sockOver.begin()->sock = socketServer;
	sockOver.begin()->overlap.hEvent = WSACreateEvent();
	sockOver.begin()->id = 0;				// 服务器id为0
	soIt[socketServer] = sockOver.begin();	// 记录服务器socket的list迭代器

	cout << "服务器启动成功" << endl;
	cout << "正在等待客户端的连接..." << endl;

	return true;
}

// 清理服务器资源
void Server::clear()
{
	for (auto& sv : sockOver)
	{
		closesocket(sv.sock);
		WSACloseEvent(sv.overlap.hEvent);
	}

	if(hPort != INVALID_HANDLE_VALUE) CloseHandle(hPort);
	WSACleanup();
}

// 处理accept请求
int Server::postAccept()
{
	SOCKET socketClient = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
		nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (socketClient == INVALID_SOCKET)
	{
		int error_code = WSAGetLastError();
		cout << "创建客户端socket失败：" << error_code << endl;
		return error_code;
	}

	sockOver.emplace_back(SocketOverlapped{});
	sockOver.back().sock = socketClient;
	sockOver.back().overlap.hEvent = WSACreateEvent();
	sockOver.back().id = sockOver.size() - 1;
	soIt[socketClient] = --sockOver.end();

	DWORD dwRecvCount = 0;

	bool res = AcceptEx(sockOver.begin()->sock, socketClient,
		recvBuff, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		&dwRecvCount, &soIt.at(socketClient)->overlap);

	int error_code = WSAGetLastError();
	if (error_code == 0 || error_code == ERROR_IO_PENDING)
	{
		// 延迟处理
		return 0;
	}
	else
	{
		// 出错了
		closesocket(socketClient);
		WSACloseEvent(soIt.at(socketClient)->overlap.hEvent);
		sockOver.erase(soIt.at(socketClient));
		soIt.erase(socketClient);
		return error_code;
	}
}

// 处理recv请求
int Server::postRecv(int sock)
{
	WSABUF wsaBuf;
	wsaBuf.buf = recvBuff;
	wsaBuf.len = BUFF_SIZE;

	DWORD dwRecvCount = 0;
	DWORD dwFlag = 0;

	bool res = WSARecv(soIt.at(sock)->sock, &wsaBuf, 1, &dwRecvCount,
		&dwFlag, &soIt.at(sock)->overlap, nullptr);

	int error_code = WSAGetLastError();
	return error_code == 0 || error_code == WSA_IO_PENDING ? 0 : error_code;
}

// 处理send请求
int Server::postSend(int sock)
{
	WSABUF wsaBuf;
	wsaBuf.buf = sendBuff;
	wsaBuf.len = BUFF_SIZE;

	DWORD dwSendCount = 0;
	DWORD dwFlag = 0;

	bool res = WSASend(soIt.at(sock)->sock, &wsaBuf, 1, &dwSendCount,
		dwFlag, &soIt.at(sock)->overlap, nullptr);

	int error_code = WSAGetLastError();
	return error_code == 0 || error_code == WSA_IO_PENDING ? 0 : error_code;
}

// 给客户端发送消息
void Server::sendMsg()
{
	cin.getline(sendBuff, BUFF_SIZE);
	// 解析
	if (string("server quit") == sendBuff)
	{
		serverQuit = true;
		// 等待500毫秒，等待各线程执行结束退出
		Sleep(500);
		return;
	}

	// 给所有客户端发送消息
	for (const auto& so : sockOver)
	{
		postSend(so.sock);
	}
}

// 线程的回调函数
DWORD WINAPI ThreadProc(LPVOID lptr)
{
	Server* server = static_cast<Server*>(lptr);
	HANDLE port = (HANDLE)server->hPort;
	DWORD numberOfBytes = 0;
	ULONG_PTR sock = 0;
	LPOVERLAPPED lpOverlapped;

	while (!server->serverQuit)
	{
		bool res = GetQueuedCompletionStatus(port, &numberOfBytes, &sock, &lpOverlapped, 200);
		int theSocket = static_cast<int>(sock);
		if (!res)
		{
			int error_code = GetLastError();
			// 超时继续
			if (error_code == WSA_WAIT_TIMEOUT) continue;

			if (error_code == 64)
			{
				// 客户端强制退出
				cout << "客户端[" << server->soIt.at(sock)->id
					<< "] 已强制下线！" << endl;

				// 关闭释放资源
				closesocket(server->soIt.at(theSocket)->sock);
				WSACloseEvent(server->soIt.at(theSocket)->overlap.hEvent);
				// 从链表中删除该SocketOverlapped
				server->sockOver.erase(server->soIt.at(theSocket));
				// 删除哈希表中对应的键值
				server->soIt.erase(theSocket);
				continue;
			}
			cout << "通知队列获取失败：" << error_code << endl;
			continue;
		}
		else
		{
			// 是否是服务器socket
			if (sock == 0)
			{
				// accept
				// 绑定到完成端口
				auto socketClient = (--server->sockOver.end())->sock;
				HANDLE tempPort = CreateIoCompletionPort((HANDLE)socketClient,
					server->hPort, socketClient, 0);
				if (tempPort != server->hPort)
				{
					cout << "绑定到完成端口失败：" << GetLastError() << endl;
					closesocket(socketClient);
					server->sockOver.erase(server->soIt.at(socketClient));
					server->soIt.erase(socketClient);
					continue;
				}

				// 设置该客户端id
				server->sockOver.back().id = server->sockOver.size() - 1;
				// 打印客户端登录成功消息
				cout << "客户端[" << server->soIt.at(socketClient)->id
					<< "] 登录成功" << endl;

				// 给客户端发送登录成功信息
				strcpy_s(server->sendBuff, "欢迎登录！");
				server->postSend(server->soIt.at(socketClient)->sock);

				// 投递recv
				server->postRecv(server->soIt.at(socketClient)->sock);

				// 继续投递accept
				server->postAccept();
			}
			else
			{
				if (numberOfBytes == 0)
				{
					// 正常退出
					cout << "客户端[" << server->soIt.at(sock)->id
						<< "] 已下线！" << endl;

					// 关闭资源
					closesocket(server->soIt.at(theSocket)->sock);
					WSACloseEvent(server->soIt.at(theSocket)->overlap.hEvent);
					// 从链表中删除该SocketOverlapped
					server->sockOver.erase(server->soIt.at(theSocket));
					// 删除哈希表中对应的键值
					server->soIt.erase(theSocket);
				}
				else
				{
					if (server->recvBuff[0])
					{
						// recv
						cout << "客户端[" << server->soIt.at(sock)->id << "] : "
							<< server->recvBuff << endl;
						server->recvBuff[0] = 0;
						// 再次投递recv 服务器socket
						server->postRecv(server->soIt.at(theSocket)->sock);
					}
					else
					{
						// send
						// 什么都不做
					}
				}
			}
		}
	}


	return 0;
}


// 返回该类对象的唯一实例
Server& Server::getInstance()
{
	static Server server;
	return server;
}

// 整体运行流程
// 供外部调用的接口
void Server::run()
{
	// 服务器启动失败
	if (!startUp()) return;

	if (postAccept() != 0)
	{
		cout << "accept错误：" << endl;
		return;
	}

	SYSTEM_INFO systemProcessorsCount;
	GetSystemInfo(&systemProcessorsCount);
	// 获取CPU核心数
	int CPUCroeNumber = systemProcessorsCount.dwNumberOfProcessors;

	vector<HANDLE> nThread;		// 线程数组
	for (int i = 0; i < CPUCroeNumber; ++i)
	{
		nThread.push_back(CreateThread(nullptr, 0, ThreadProc, this, 0, nullptr));
	}

	// 主线程阻塞
	while (!serverQuit)
	{
		// 主线程输入要发送的消息
		sendMsg();
	}

	// 释放线程句柄
	for (int i = 0; i < CPUCroeNumber; ++i) CloseHandle(nThread.at(i));
}