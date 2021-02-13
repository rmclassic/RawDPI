#define CPPHTTPLIB_OPENSSL_SUPPORT

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#define WINDOWS 1
#endif// _WIN32

#ifdef __unix__
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <signal.h>
#define WINDOWS 0
#endif // __unix__

#include <thread>
#include <iostream>
#include <queue>
#include <string>

#include "DOH.h"
#include <mutex>
#include "Exceptions.h"
#define DPI_OFFSET 10
std::queue<std::string> OutputLogQueue;

void ManageRequest(int);
void ServerClientTunnel(int, int, std::string);
void ClientServerTunnel(int, int, std::string);
int InitConnectMethod(int, int, std::string);
int InitGetMethod(int, int, std::string, char*, int);
void StartOutputStream();
std::string ExtractHostFromRequest(std::string);
std::mutex outmtx;
void OutputLogQueuePush(std::string message)
{
	outmtx.lock();
	OutputLogQueue.push(message);
	outmtx.unlock();
}

int main(int argc, char** argv)
{

  signal(SIGPIPE, SIG_IGN);
	InitializeExceptionsList();
	LoadIPsFromFile();
	//std::thread(StartOutputStream).detach();
	#ifdef _WIN32
      	WSADATA WSAData;
        WSAStartup(MAKEWORD(2, 0), &WSAData);
    #endif


	int ListenerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int wow = 1;
	setsockopt(ListenerSocket, SOL_SOCKET, SO_REUSEADDR, &wow, sizeof(int));
	sockaddr_in ListenerAddress;
	ListenerAddress.sin_family = AF_INET;
	ListenerAddress.sin_addr.s_addr = INADDR_ANY;
	ListenerAddress.sin_port = htons(5585);
	int ListenerAddressSize = sizeof(ListenerAddress);

	int bound = bind(ListenerSocket, (struct sockaddr*)& ListenerAddress, ListenerAddressSize);
	listen(ListenerSocket, 10);
	OutputLogQueuePush("Bound");
	while (true)
	{
		int IncomingSocket = accept(ListenerSocket, (struct sockaddr*)&ListenerAddress, (socklen_t*)&ListenerAddressSize);
		OutputLogQueuePush("Connection Received");
		std::thread(ManageRequest, IncomingSocket).detach();
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

int InitRequestResponse(int ClientSocket)
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
		InitConnectMethod(ClientSocket, ServerSocket, Host);
	}
	else
	{
		InitGetMethod(ClientSocket, ServerSocket, Host, (char*)RequestBuffer, RequestSize);
	}
	return -1;
}

int InitGetMethod(int ClientSocket, int ServerSocket, std::string Host, char* RequestBuffer, int RequestSize)
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
		char ServerResponse[16000];
		do
		{
			ServerResponseSize = recv(ServerSocket, ServerResponse, 16000, 0);
			send(ClientSocket, ServerResponse, ServerResponseSize, 0);

		} while (ServerResponseSize > 0);
	}
	shutdown(ServerSocket, 0);
	shutdown(ClientSocket, 0);
	return -1;
}

int InitConnectMethod(int ClientSocket, int ServerSocket, std::string Host)
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

		send(ClientSocket, SuccessResponse.c_str(), SuccessResponse.size(), 0);
		struct timeval nTimeout;
		nTimeout.tv_sec = 20000;
		setsockopt(ServerSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&nTimeout, sizeof(struct timeval));
		std::thread(ClientServerTunnel, ClientSocket, ServerSocket, Host).detach();
		std::thread(ServerClientTunnel, ClientSocket, ServerSocket,Host).detach();
		return ServerSocket;
	}
	return - 1;
}

void ManageRequest(int ClientSocket)
{
	int ServerSocket = InitRequestResponse(ClientSocket);

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

void ServerClientTunnel(int ClientSocket, int ServerSocket, std::string host)
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
		shutdown(ServerSocket, 0);
		shutdown(ClientSocket, 0);

			//OutputLogQueuePush("Server-Client Tunnel Ended");
	}
	catch (...)
	{
		delete[] Buffer;
		shutdown(ServerSocket, 0);
		shutdown(ClientSocket, 0);

		//OutputLogQueuePush("Server-Client Tunnel Failed");
	}
}

void ClientServerTunnel(int ClientSocket, int ServerSocket, std::string Host)
{
	std::vector<int> Hotspots;
	char* Buffer = new char[65535];
	try
	{
		int ClientReceivedCount;
		do {
			ClientReceivedCount = recv(ClientSocket, Buffer, 65535, 0);
			if (!IsException(Host))
			{
				Hotspots = FindAllSubStrings(Buffer, ClientReceivedCount, Host.c_str(), Host.size());

					for (int i = 0, hotspot, sent = 0; i < Hotspots.size(); i++)
					{
						hotspot = Hotspots[i];


						send(ServerSocket, Buffer + sent, hotspot - sent + DPI_OFFSET, 0);

						sent += hotspot + DPI_OFFSET;
					}
			}
			else
			{
				send(ServerSocket, Buffer, ClientReceivedCount, 0);
				std::cout << "Exception found\n";
			}



		} while (ClientReceivedCount > 0);

		shutdown(ServerSocket, 0);
		shutdown(ClientSocket, 0);
		delete[] Buffer;
		//OutputLogQueuePush("Client-Server Tunnel Ended");
	}
	catch (...)
	{
		delete[] Buffer;
		shutdown(ServerSocket, 0);
		shutdown(ClientSocket, 0);
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
