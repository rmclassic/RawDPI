#include <string>
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
#endif // __unix__


#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
std::map<std::string, std::string> Domains;
std::mutex mapmtx;
std::mutex filemtx;
std::string ExtractIPFromAnswer(std::string ansstr)
{
	int iploc = ansstr.find("IP:") + 3;
	if (-1 < iploc)
	{
		std::string host = ansstr.substr(iploc);
		return host;
	}
	return "";
}

void SaveIPToFile(std::string host, std::string ip)
{
	filemtx.lock();
	std::ofstream ostr("hosts.txt", std::ios::app);
	ostr << host << " " << ip << '\n';
	ostr.close();
	filemtx.unlock();
}

std::string ResolveDOHIP(std::string HostName)
{

	std::map<std::string, std::string>::iterator it = Domains.find(HostName);
	if (it != Domains.end())
		return Domains.at(HostName);
 std::cout << "SHOULD RESOLVE HOsT\n";

	char* recvbuff = new char[2000];
	int Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in ServerAddress;
	ServerAddress.sin_family = AF_INET;
	inet_pton(AF_INET, "185.176.43.62", &ServerAddress.sin_addr);
	ServerAddress.sin_port = htons(80);
	int ListenerAddressSize = sizeof(ServerAddress);

	if (0 > connect(Socket, (sockaddr*)& ServerAddress, ListenerAddressSize))
		return "";
		std::to_string(HostName.size() + 5);

	std::string reqstr = "POST /DNS.php HTTP/1.1\r\nHost: rmclassic.royalwebhosting.net\r\nContent-Length: " + std::to_string(HostName.size() + 5) + "\r\nContent-Type: application/x-www-form-urlencoded\r\nCookie: __test=5d600a5638740d94f2bfbfede8b2b18f\r\n\r\nhost=" + HostName;
	send(Socket, reqstr.c_str(), reqstr.length(), 0);

	std::string ansstr;
	//timeval tv;
	//tv.tv_sec = 1000;
	//setsockopt(Socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));
  	usleep(1500000);
	  int recvd = recv(Socket, recvbuff, 2000, 0);
		for (int i = 0; i < recvd; i++)
		{

			ansstr += recvbuff[i];
			recvbuff[i] = -52;
		}



	std::string IP = ExtractIPFromAnswer(ansstr);

	if (IP == "")
		return "";
	mapmtx.lock();
	Domains.insert({ HostName, IP });
	mapmtx.unlock();
	SaveIPToFile(HostName, IP);
	return IP;
}

void LoadIPsFromFile()
{
	std::string host, ip;
	std::ifstream ins("hosts.txt");
	while (!ins.eof())
	{
		ins >> host >> ip;
		Domains.insert({ host, ip });
	}
	std::cout << Domains.size() << " Read from file\n";
}
