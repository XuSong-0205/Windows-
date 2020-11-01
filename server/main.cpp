#include "Server.h"

int main()
{
	auto& server = Server::getInstance();
	server.run();

	return 0;
}