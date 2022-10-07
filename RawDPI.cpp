#include <cstdio>
#define CPPHTTPLIB_OPENSSL_SUPPORT

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#endif// _WIN32

#ifdef __unix__
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <signal.h>
#endif // __unix__

#include <thread>
#include <iostream>
#include <queue>
#include <string>
#include <map>
#include "DOH.h"
#include <mutex>
#include "Exceptions.h"
#include "connectionInfo.h"
#include "dns.h"
#define DPI_OFFSET 3
std::queue<std::string> OutputLogQueue;

void ManageRequest(int, sockaddr_in);
void ServerClientTunnel(int, int, std::string, int);
void ClientServerTunnel(int, int, std::string, int);
int InitConnectMethod(int, int, std::string, sockaddr_in);
int InitGetMethod(int, int, std::string, char*, int, sockaddr_in);
void StartOutputStream();
std::string ExtractHostFromRequest(std::string);
std::mutex outmtx;
std::map<int, ConnectionInfo> Connections;
void OutputLogQueuePush(std::string message)
{
	outmtx.lock();
	OutputLogQueue.push(message);
	outmtx.unlock();
}

void socketclose(int socket)
{
#ifdef __unix__
	close(socket);
#endif

#ifdef _WIN32
	closesocket(socket);
#endif
}

int main(int argc, char** argv)
{
#ifdef __unix__
  signal(SIGPIPE, SIG_IGN);
#endif
	InitializeExceptionsList();
	LoadIPsFromFile();
	//std::thread(StartOutputStream).detach();
	#ifdef _WIN32
      	WSADATA WSAData;
        WSAStartup(MAKEWORD(2, 0), &WSAData);
    #endif


	int ListenerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int wow = 1;
#ifdef __unix__
	setsockopt(ListenerSocket, SOL_SOCKET, SO_REUSEADDR, &wow, sizeof(int));
#endif
#ifdef _WIN32
	//setsockopt(ListenerSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&wow, sizeof(int));
#endif
	sockaddr_in ListenerAddress;
	ListenerAddress.sin_family = AF_INET;
	ListenerAddress.sin_addr.s_addr = INADDR_ANY;
	ListenerAddress.sin_port = htons(5585);
	int ListenerAddressSize = sizeof(ListenerAddress);

	int bound = bind(ListenerSocket, (struct sockaddr*)&ListenerAddress, ListenerAddressSize);
	listen(ListenerSocket, 10);
	OutputLogQueuePush("Bound");
	while (true)
	{
		int IncomingSocket = accept(ListenerSocket, (struct sockaddr*)&ListenerAddress, (socklen_t*)&ListenerAddressSize);
		OutputLogQueuePush("Connection Received");
		std::thread(ManageRequest, IncomingSocket, ListenerAddress).detach();
		//ManageRequest(IncomingSocket);
	}
}


void StartOutputStream()
{
	while (true)
	{
		if (!OutputLogQueue.empty())
		{
			std::cout << OutputLogQueue.front() << '\n';
			std::cout.flush();
			OutputLogQueue.pop();
		}
	}
}

int InitRequestResponse(int ClientSocket, sockaddr_in addrinfo)
{
	char RequestBuffer[8192];
	int ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);


	int RequestSize = recv(ClientSocket, RequestBuffer, 8192, 0);

	std::string Host = ExtractHostFromRequest(RequestBuffer);
	if (Host == "")
	{
		send(ClientSocket, "503 Service Unavailable\r\n\r\n", std::string("503 Service Unavailable\r\n\r\n").size(), 0);
		shutdown(ClientSocket, 0);
		return -1;
	}
	std::string ReqBuff = RequestBuffer;

	if (ReqBuff.find("CONNECT") == 0)
	{
		InitConnectMethod(ClientSocket, ServerSocket, Host, addrinfo);
	}
	else
	{
		InitGetMethod(ClientSocket, ServerSocket, Host, (char*)RequestBuffer, RequestSize, addrinfo);
	}
	return -1;
}

int InitGetMethod(int ClientSocket, int ServerSocket, std::string Host, char* RequestBuffer, int RequestSize, sockaddr_in addrinfo)
{
	sockaddr_in ServerAddress;

	std::string ServerIP = ResolveDOHIP(Host);
	const char* SIP = ServerIP.c_str();
	if (ServerIP == "")
		return -1;


	inet_pton(AF_INET, ServerIP.c_str(), &ServerAddress.sin_addr);
	ServerAddress.sin_port = htons(80);
	ServerAddress.sin_family = AF_INET;

	if (0 == connect(ServerSocket, (sockaddr*)&ServerAddress, sizeof(ServerAddress)))
	{
		std::cout << "GET Connection to " + Host + " Established\n";
		std::cout.flush();
		send(ServerSocket, RequestBuffer, RequestSize, 0);

		int ServerResponseSize = 0;
		char* ServerResponse = new char[65535];
		struct timeval nTimeout;

#ifdef __unix__
		nTimeout.tv_sec = 5;
#endif
#ifdef _WIN32
		nTimeout.tv_sec = 5000;
#endif
		setsockopt(ClientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&nTimeout, sizeof(struct timeval));

		do
		{
			ServerResponseSize = recv(ServerSocket, ServerResponse, 65535, 0);
			send(ClientSocket, ServerResponse, ServerResponseSize, 0);
		} while (ServerResponseSize > 0);

		delete[] ServerResponse;
	}
	shutdown(ServerSocket, 0);
	shutdown(ClientSocket, 0);
	return -1;
}

int InitConnectMethod(int ClientSocket, int ServerSocket, std::string Host, sockaddr_in addrinfo)
{
	sockaddr_in ServerAddress;
	//OutputLogQueuePush("Resolving " + Host);
	std::string ServerIP = ResolveDOHIP(Host);
	const char* SIP = ServerIP.c_str();
	if (ServerIP == "")
		return -1;

	inet_pton(AF_INET, SIP, &ServerAddress.sin_addr);
	ServerAddress.sin_port = htons(443);
	ServerAddress.sin_family = AF_INET;

	if (0 == connect(ServerSocket, (sockaddr*)&ServerAddress, sizeof(ServerAddress)))
	{
		std::string SuccessResponse = "HTTP/1.1 200 Connection Established\r\n\r\n";

		std::cout << "Connection to " + Host + " Established\n";

		ConnectionInfo connInfo;
		char temp[17];
		connInfo.SourceIP = inet_ntop(AF_INET, &addrinfo.sin_addr, temp, 17);
		connInfo.DestinationIP = SIP;
		connInfo.SourcePort = addrinfo.sin_port;
		connInfo.DestinationPort = 443;
		connInfo.ClientTunnel = ClientSocket;
		connInfo.ServerTunnel = ServerSocket;

		Connections.insert({ addrinfo.sin_port, connInfo });

		send(ClientSocket, SuccessResponse.c_str(), SuccessResponse.size(), 0);
		struct timeval nTimeout;
		nTimeout.tv_sec = 10;
		//setsockopt(ServerSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&nTimeout, sizeof(struct timeval));
		std::thread(ClientServerTunnel, ClientSocket, ServerSocket, Host, addrinfo.sin_port).detach();
		std::thread(ServerClientTunnel, ClientSocket, ServerSocket,Host, addrinfo.sin_port).detach();
		return ServerSocket;
	}
	return - 1;
}

void ManageRequest(int ClientSocket, sockaddr_in addrinfo)
{
	int ServerSocket = InitRequestResponse(ClientSocket, addrinfo);

	if (ServerSocket == -1)
		return;
}

std::vector<int> FindAllSubStrings(char* str, int strsize, const char* substring, int substrsize)
{
	bool found = true;
	std::vector<int> Occurences;
	for (int i = 0; i < strsize - substrsize; i++)
	{
		found = true;
		for (int j = 0; j < substrsize; j++)
		{
			if (str[i + j] != substring[j])
			{
				found = false;
				break;
			}
		}

		if (found == true)
		{
			Occurences.push_back(i);
			found = false;
		}
	}
	Occurences.push_back(strsize - DPI_OFFSET);
	return Occurences;
}

void ServerClientTunnel(int ClientSocket, int ServerSocket, std::string host, int id)
{
	char* Buffer = new char[65535];
	try
	{
		int ServerReceivedCount;
		do {


			ServerReceivedCount = recv(ServerSocket, Buffer, 65535, 0);
			if (ServerReceivedCount > 0)
                send(ClientSocket, Buffer, ServerReceivedCount, 0);
			//OutputLogQueuePush(std::to_string(ServerReceivedCount) + "Bytes from " + host + " to Client");
		} while (ServerReceivedCount > 0);
		delete[] Buffer;
		Connections.erase(id);
		shutdown(ServerSocket, 0);
		socketclose(ServerSocket);
		shutdown(ClientSocket, 0);
		socketclose(ClientSocket);

		printf("Connection to %s terminated, alive connections: %d\n", host.c_str(), Connections.size());

		//OutputLogQueuePush("Server-Client Tunnel Ended");
	}
	catch (...)
	{
		delete[] Buffer;
		Connections.erase(id);
		shutdown(ServerSocket, 0);
		socketclose(ServerSocket);
		shutdown(ClientSocket, 0);
		socketclose(ClientSocket);

		printf("Connection to %s terminated, alive connections: %d\n", host.c_str(), Connections.size());

		//OutputLogQueuePush("Server-Client Tunnel Failed");
	}
}

void ClientServerTunnel(int ClientSocket, int ServerSocket, std::string Host, int id)
{
	std::vector<int> Hotspots;
	char* Buffer = new char[65535];
	try
	{
		int domain_sep = Host.rfind(".");
		std::string Domain = Host.substr(Host.rfind(".", domain_sep - 1) + 1);
		std::cout << "Domain: " << Domain << '\n';

		int ClientReceivedCount;
		do {
			ClientReceivedCount = recv(ClientSocket, Buffer, 65535, 0);
			if (!IsException(Domain))
			{
				Hotspots = FindAllSubStrings(Buffer, ClientReceivedCount, Domain.c_str(), Domain.size());

					for (int i = 0, hotspot, sent = 0; i < Hotspots.size(); i++)
					{
						hotspot = Hotspots[i];

						sent += send(ServerSocket, Buffer + sent, hotspot - sent + DPI_OFFSET, 0);
					}
			}
			else
			{
				send(ServerSocket, Buffer, ClientReceivedCount, 0);
				std::cout << "Exception found\n";
			}



		} while (ClientReceivedCount > 0);


		Connections.erase(id);
		shutdown(ServerSocket, 0);
		socketclose(ServerSocket);
		shutdown(ClientSocket, 0);
		socketclose(ClientSocket);

		printf("Connection to %s terminated, alive connections: %d\n", Host.c_str(), Connections.size());

		delete[] Buffer;
		//OutputLogQueuePush("Client-Server Tunnel Ended");
	}
	catch (...)
	{
		delete[] Buffer;
		Connections.erase(id);
		shutdown(ServerSocket, 0);
		socketclose(ServerSocket);
		shutdown(ClientSocket, 0);
		socketclose(ClientSocket);

		printf("Connection to %s terminated, alive connections: %d\n", Host.c_str(), Connections.size());
		//OutputLogQueuePush("Client-Server Tunnel Failed");
	}
}

std::string ExtractHostFromRequest(std::string Request)
{
	int host_start = Request.find("Host:") + 6;
	if (host_start == 5)
		return "";
		//OutputLogQueuePush("host was in " + host_start);
	int host_end = Request.find('\r', host_start) < Request.find(':', host_start)? Request.find('\r', host_start) : Request.find(':', host_start);
		std::string host = Request.substr(host_start, host_end - host_start);
		return host;
}
