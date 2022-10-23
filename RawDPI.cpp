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
#include <regex>
#include <inttypes.h>
#include "Exceptions.h"
#include "connectionInfo.h"
#include "dns.h"
#define DPI_OFFSET 3
std::queue<std::string> OutputLogQueue;

void ManageRequest(int, sockaddr_in);
void ServerClientTunnel(int, int, std::string, int);
void ClientServerTunnel(int, int, std::string, int);
int InitConnectMethod(int, int, std::string, char*, int, sockaddr_in);
int InitGetMethod(int, int, std::string, char*, int, sockaddr_in);
void StartOutputStream();
std::string ExtractHostFromRequest(std::string);
unsigned short ExtractPortFromRequest(std::string, unsigned short);
std::mutex conmtx;
std::map<int, ConnectionInfo> Connections;

void socketclose(int socket, int id = 0)
{
#ifdef __unix__
	close(socket);
	shutdown(socket, 0);
#endif

#ifdef _WIN32
	closesocket(socket);
	shutdown(socket, 0);
#endif

	if (id == 0)
	{
		return;
	}

	conmtx.lock();
	printf("alive connections: %" PRId64 "\n", Connections.size());
	Connections.erase(id);
	conmtx.unlock();
}

int main(int argc, char** argv)
{
#ifdef __unix__
  signal(SIGPIPE, SIG_IGN);
#endif
	InitializeExceptionsList();
	LoadIPsFromFile();
	InitCache();
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
	while (true)
	{
		int IncomingSocket = accept(ListenerSocket, (struct sockaddr*)&ListenerAddress, (socklen_t*)&ListenerAddressSize);
		std::thread(ManageRequest, IncomingSocket, ListenerAddress).detach();
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
		InitConnectMethod(ClientSocket, ServerSocket, Host, (char*)RequestBuffer, RequestSize, addrinfo);
	}
	else
	{
		InitGetMethod(ClientSocket, ServerSocket, Host, (char*)RequestBuffer, RequestSize, addrinfo);
	}
	return -1;
}

size_t ScanContentLength(const char* data, size_t dataSize)
{
	std::string dataStr = std::string(data, dataSize);
	auto r = std::regex("[c|C]ontent-[L|l]ength: [0-9]{0,5}\\r\\n");
	std::smatch match;
	std::regex_search(dataStr, match, r);

	if (match.size() > 1)
		return atof(match[2].str().c_str());

	return -1;
}

int InitGetMethod(int ClientSocket, int ServerSocket, std::string Host, char* RequestBuffer, int RequestSize, sockaddr_in addrinfo)
{
	sockaddr_in ServerAddress;

	std::string ServerIP = ResolveDOHIP(Host);
	const char* SIP = ServerIP.c_str();
	unsigned short port = ExtractPortFromRequest(std::string(RequestBuffer, RequestSize), 80);

	if (ServerIP == "")
		return -1;


	inet_pton(AF_INET, ServerIP.c_str(), &ServerAddress.sin_addr);
	ServerAddress.sin_port = htons(port);
	ServerAddress.sin_family = AF_INET;

	if (0 == connect(ServerSocket, (sockaddr*)&ServerAddress, sizeof(ServerAddress)))
	{
		std::cout << "GET Connection to " + Host + " Established\n";
		std::cout.flush();

		strstr(RequestBuffer, " ")[1] = '\t';
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
		setsockopt(ServerSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&nTimeout, sizeof(struct timeval));

		size_t ContentLength = -1;
		bool headersRead = false;
		do
		{
			ServerResponseSize = recv(ServerSocket, ServerResponse, 65535, 0);
			if (!headersRead)
			{
				ContentLength = ScanContentLength(ServerResponse, ServerResponseSize);
				headersRead = true;
			}
			

			send(ClientSocket, ServerResponse, ServerResponseSize, 0);
		} while ((ServerResponseSize < ContentLength && ContentLength > 0) || (ServerResponseSize > 0 && (ContentLength <= 0)));

		std::cout << "Done GET " << Host << '\n';
		delete[] ServerResponse;
	}
	socketclose(ClientSocket);
	socketclose(ServerSocket);
	return -1;
}

int InitConnectMethod(int ClientSocket, int ServerSocket, std::string Host, char* request, int request_size, sockaddr_in addrinfo)
{
	sockaddr_in ServerAddress;
	std::string ServerIP = ResolveDOHIP(Host);
	const char* SIP = ServerIP.c_str();
	unsigned short port = ExtractPortFromRequest(std::string(request, request_size), 443);
	if (ServerIP == "")
		return -1;

	inet_pton(AF_INET, SIP, &ServerAddress.sin_addr);
	ServerAddress.sin_port = htons(port);
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
		connInfo.DestinationPort = port;
		connInfo.ClientTunnel = ClientSocket;
		connInfo.ServerTunnel = ServerSocket;

		conmtx.lock();
		Connections.insert({ addrinfo.sin_port, connInfo });
		conmtx.unlock();

		send(ClientSocket, SuccessResponse.c_str(), SuccessResponse.size(), 0);
		struct timeval nTimeout;
		nTimeout.tv_sec = 10;
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
	int ServerReceivedCount;
	do 
	{
		ServerReceivedCount = recv(ServerSocket, Buffer, 65535, 0);
		if (ServerReceivedCount > 0)
            send(ClientSocket, Buffer, ServerReceivedCount, 0);
	} while (ServerReceivedCount > 0);
	delete[] Buffer;
	socketclose(ClientSocket);
	socketclose(ServerSocket);
}

void ClientServerTunnel(int ClientSocket, int ServerSocket, std::string Host, int id)
{
	std::vector<int> Hotspots;
	char* Buffer = new char[65535];

		int domain_sep = Host.rfind(".");
		std::string Domain = Host.substr(Host.rfind(".", domain_sep - 1) + 1);
		std::cout << "Domain: " << Domain << '\n';

		int ClientReceivedCount;
		do {
			ClientReceivedCount = recv(ClientSocket, Buffer, 65535, 0);
			if (!IsException(Domain))
			{
				Hotspots = FindAllSubStrings(Buffer, ClientReceivedCount, Domain.c_str(), Domain.size());
				Hotspots = { 1, ClientReceivedCount };
					for (int i = 0, hotspot, sent = 0; i < Hotspots.size(); i++)
					{
						hotspot = Hotspots[i];

						sent += send(ServerSocket, Buffer + sent, hotspot - sent /* + DPI_OFFSET*/, 0);
					}
			}
			else
			{
				send(ServerSocket, Buffer, ClientReceivedCount, 0);
				std::cout << "Exception found\n";
			}
		} while (ClientReceivedCount > 0);
		delete[] Buffer;
		socketclose(ClientSocket);
		socketclose(ServerSocket);
}

std::string ExtractHostFromRequest(std::string Request)
{
	std::regex r("Host: ([^:]*)\r\n");
	std::smatch match;
	std::regex_search(Request, match, r);

	if (match.size() > 1)
		return match[1];
	else
	{
		r = std::regex("CONNECT ([^:]*).* HTTP\\/[0-9]\\.[0-9]\r\n.*");
		std::regex_search(Request, match, r);
	}

	if (match.size() > 1)
		return match[2];

	return "";
}

unsigned short ExtractPortFromRequest(std::string Request, unsigned short default_val)
{
	auto r = std::regex("(CONNECT|GET) .*:([0-9]{0,5}) HTTP\\/[0-9]\\.[0-9]");
	std::smatch match;
	std::regex_search(Request, match, r);

	if (match.size() > 1)
		return atoi(match[2].str().c_str());

	return default_val;
}
