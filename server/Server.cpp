#include "Server.h"

Server::Server() : recvBuff{ 0 }, sendBuff{ 0 }{}

Server::~Server() { clear(); }

// ����������
bool Server::startUp()
{
	// �������
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "�������ʧ�ܣ�" << WSAGetLastError() << endl;
		return false;
	}

	// У��汾��
	if (HIBYTE(wsaData.wVersion) != 2 || LOBYTE(wsaData.wVersion) != 2)
	{
		cout << "�����汾����" << WSAGetLastError() << endl;
		WSACleanup();
		return false;
	}

	// ���� socket
	SOCKET socketServer = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (socketServer == INVALID_SOCKET)
	{
		cout << "����socketʧ�ܣ�" << WSAGetLastError() << endl;
		WSACleanup();
		return false;
	}

	// ��ʼ��IP�˿ںź�Э������Ϣ
	SOCKADDR_IN addrServer;
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(PORT);
	inet_pton(AF_INET, SERVER_ADDRESS, &addrServer.sin_addr);

	// ��socket
	if (bind(socketServer, (SOCKADDR*)&addrServer, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		cout << "��socketʧ�ܣ�" << WSAGetLastError() << endl;
		closesocket(socketServer);
		WSACleanup();
		return false;
	}

	// ������ɶ˿�
	hPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	if (hPort == 0)
	{
		cout << "������ɶ˿�ʧ�ܣ�" << GetLastError() << endl;
		closesocket(socketServer);
		WSACleanup();
		return false;
	}

	// ����ɶ˿ںͷ�����socket
	if (hPort != CreateIoCompletionPort((HANDLE)socketServer, hPort, 0, 0))
	{
		cout << "����ɶ˿�ʧ�ܣ�" << GetLastError() << endl;
		CloseHandle(hPort);
		closesocket(socketServer);
		WSACleanup();
		return false;
	}

	// ����
	if (listen(socketServer, SOMAXCONN) == SOCKET_ERROR)
	{
		cout << "����ʧ�ܣ�" << WSAGetLastError() << endl;
		CloseHandle(hPort);
		closesocket(socketServer);
		WSACleanup();
		return false;
	}

	// ��������SocketOverlapped��ӵ�������
	sockOver.emplace_back(SocketOverlapped{});
	sockOver.begin()->sock = socketServer;
	sockOver.begin()->overlap.hEvent = WSACreateEvent();
	sockOver.begin()->id = 0;				// ������idΪ0
	soIt[socketServer] = sockOver.begin();	// ��¼������socket��list������

	cout << "�����������ɹ�" << endl;
	cout << "���ڵȴ��ͻ��˵�����..." << endl;

	return true;
}

// �����������Դ
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

// ����accept����
int Server::postAccept()
{
	SOCKET socketClient = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
		nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (socketClient == INVALID_SOCKET)
	{
		int error_code = WSAGetLastError();
		cout << "�����ͻ���socketʧ�ܣ�" << error_code << endl;
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
		// �ӳٴ���
		return 0;
	}
	else
	{
		// ������
		closesocket(socketClient);
		WSACloseEvent(soIt.at(socketClient)->overlap.hEvent);
		sockOver.erase(soIt.at(socketClient));
		soIt.erase(socketClient);
		return error_code;
	}
}

// ����recv����
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

// ����send����
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

// ���ͻ��˷�����Ϣ
void Server::sendMsg()
{
	cin.getline(sendBuff, BUFF_SIZE);
	// ����
	if (string("server quit") == sendBuff)
	{
		serverQuit = true;
		// �ȴ�500���룬�ȴ����߳�ִ�н����˳�
		Sleep(500);
		return;
	}

	// �����пͻ��˷�����Ϣ
	for (const auto& so : sockOver)
	{
		postSend(so.sock);
	}
}

// �̵߳Ļص�����
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
			// ��ʱ����
			if (error_code == WSA_WAIT_TIMEOUT) continue;

			if (error_code == 64)
			{
				// �ͻ���ǿ���˳�
				cout << "�ͻ���[" << server->soIt.at(sock)->id
					<< "] ��ǿ�����ߣ�" << endl;

				// �ر��ͷ���Դ
				closesocket(server->soIt.at(theSocket)->sock);
				WSACloseEvent(server->soIt.at(theSocket)->overlap.hEvent);
				// ��������ɾ����SocketOverlapped
				server->sockOver.erase(server->soIt.at(theSocket));
				// ɾ����ϣ���ж�Ӧ�ļ�ֵ
				server->soIt.erase(theSocket);
				continue;
			}
			cout << "֪ͨ���л�ȡʧ�ܣ�" << error_code << endl;
			continue;
		}
		else
		{
			// �Ƿ��Ƿ�����socket
			if (sock == 0)
			{
				// accept
				// �󶨵���ɶ˿�
				auto socketClient = (--server->sockOver.end())->sock;
				HANDLE tempPort = CreateIoCompletionPort((HANDLE)socketClient,
					server->hPort, socketClient, 0);
				if (tempPort != server->hPort)
				{
					cout << "�󶨵���ɶ˿�ʧ�ܣ�" << GetLastError() << endl;
					closesocket(socketClient);
					server->sockOver.erase(server->soIt.at(socketClient));
					server->soIt.erase(socketClient);
					continue;
				}

				// ���øÿͻ���id
				server->sockOver.back().id = server->sockOver.size() - 1;
				// ��ӡ�ͻ��˵�¼�ɹ���Ϣ
				cout << "�ͻ���[" << server->soIt.at(socketClient)->id
					<< "] ��¼�ɹ�" << endl;

				// ���ͻ��˷��͵�¼�ɹ���Ϣ
				strcpy_s(server->sendBuff, "��ӭ��¼��");
				server->postSend(server->soIt.at(socketClient)->sock);

				// Ͷ��recv
				server->postRecv(server->soIt.at(socketClient)->sock);

				// ����Ͷ��accept
				server->postAccept();
			}
			else
			{
				if (numberOfBytes == 0)
				{
					// �����˳�
					cout << "�ͻ���[" << server->soIt.at(sock)->id
						<< "] �����ߣ�" << endl;

					// �ر���Դ
					closesocket(server->soIt.at(theSocket)->sock);
					WSACloseEvent(server->soIt.at(theSocket)->overlap.hEvent);
					// ��������ɾ����SocketOverlapped
					server->sockOver.erase(server->soIt.at(theSocket));
					// ɾ����ϣ���ж�Ӧ�ļ�ֵ
					server->soIt.erase(theSocket);
				}
				else
				{
					if (server->recvBuff[0])
					{
						// recv
						cout << "�ͻ���[" << server->soIt.at(sock)->id << "] : "
							<< server->recvBuff << endl;
						server->recvBuff[0] = 0;
						// �ٴ�Ͷ��recv ������socket
						server->postRecv(server->soIt.at(theSocket)->sock);
					}
					else
					{
						// send
						// ʲô������
					}
				}
			}
		}
	}


	return 0;
}


// ���ظ�������Ψһʵ��
Server& Server::getInstance()
{
	static Server server;
	return server;
}

// ������������
// ���ⲿ���õĽӿ�
void Server::run()
{
	// ����������ʧ��
	if (!startUp()) return;

	if (postAccept() != 0)
	{
		cout << "accept����" << endl;
		return;
	}

	SYSTEM_INFO systemProcessorsCount;
	GetSystemInfo(&systemProcessorsCount);
	// ��ȡCPU������
	int CPUCroeNumber = systemProcessorsCount.dwNumberOfProcessors;

	vector<HANDLE> nThread;		// �߳�����
	for (int i = 0; i < CPUCroeNumber; ++i)
	{
		nThread.push_back(CreateThread(nullptr, 0, ThreadProc, this, 0, nullptr));
	}

	// ���߳�����
	while (!serverQuit)
	{
		// ���߳�����Ҫ���͵���Ϣ
		sendMsg();
	}

	// �ͷ��߳̾��
	for (int i = 0; i < CPUCroeNumber; ++i) CloseHandle(nThread.at(i));
}