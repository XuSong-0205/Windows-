#include "Client.h"

Client::~Client() { clear(); }

// �����ͻ���
bool Client::startUp()
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
		cout << "�汾�Ŵ���" << WSAGetLastError() << endl;
		return false;
	}

	// �����׽���
	socketServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socketServer == INVALID_SOCKET)
	{
		cout << "����socketʧ�ܣ�" << WSAGetLastError() << endl;
		return false;
	}

	// ��ʼ��IP�˿ںź�Э������Ϣ
	SOCKADDR_IN addrServer;
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(PORT);
	inet_pton(AF_INET, SERVER_ADDRESS, &addrServer.sin_addr);

	cout << "�ͻ��������ɹ�" << endl;

	// ���ӷ�����
	if (connect(socketServer, (SOCKADDR*)&addrServer, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		cout << "���ӿͻ���ʧ�ܣ�" << WSAGetLastError() << endl;
		return false;
	}

	cout << "���ӷ������ɹ�" << endl;

	return true;
}

// ������Դ
void Client::clear()
{
	if (socketServer) closesocket(socketServer);
	WSACleanup();
}

// ����recv
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
				cout << "�ͻ������˳���" << endl;
				clientQuit = true;
				return;
			}
		}
		else
		{
			cout << "������ : " << recvBuff << endl;
		}
	}
}

// ����send
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

// �ͻ��������̣��ⲿ���ýӿ�
void Client::run(int argc, char* argv[])
{
	// �������л�ȡ������IP��ַ
	if (argc == 2) SERVER_ADDRESS = argv[1];

	// �����ͻ���
	if (!startUp()) return;

	// ����һ���߳�ȥ���������Ϣ
	thread recvMsg(&Client::postRecv, this);
	recvMsg.detach();

	// ���̴߳�������Ϣ
	postSend();
}