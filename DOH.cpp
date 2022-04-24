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
#endif // __unix__


#include <string>
#include "httplib.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <mutex>
std::map<std::string, std::string> Domains;
std::mutex mapmtx;
std::mutex filemtx;


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
	// Return Cloudflare IP for DNS over HTTPS
	if (HostName == "1.1.1.1")
		return "1.1.1.1";

	std::map<std::string, std::string>::iterator it = Domains.find(HostName);
	if (it != Domains.end())
		return Domains.at(HostName);

	// USE HTTPLIB TO GET CF DOH
	httplib::Client cli("https://cloudflare-dns.com");
	httplib::Headers headers = {
  { "Accept", "application/dns-json" }
};
	//cli.set_proxy("127.0.0.1", 5585); //Set proxy to ourselves, because the Cloudflare may be blocked too
	auto res = cli.Get(("/dns-query?type=A&name=" + HostName).c_str(), headers);

	if (res->status != 200)
		return "";

	std::string IP;

	try
	{
		auto jj = nlohmann::json::parse(res->body);
		bool FoundIP = false;
		for (auto i : jj["Answer"])
		{
			if (i["type"] == 1)
			{
				IP = i["data"];
				FoundIP = true;
			}
		}

		if (!FoundIP)
			return "";
	}
	catch (...)
	{
		return "";
	}


	mapmtx.lock();
	if (Domains.find(HostName) == Domains.end())
	{

		Domains.insert({ HostName, IP });
		SaveIPToFile(HostName, IP);
	}
	mapmtx.unlock();

	return IP;
}

void LoadIPsFromFile()
{
	std::string host, ip;
	std::fstream ins("hosts.txt");
	if (!ins.is_open())
	{
		std::ofstream o("hosts.txt");
		o.close();
		ins.open("hosts.txt");
	}

	while (!ins.eof())
	{
		ins >> host >> ip;
		Domains.insert({ host, ip });
	}
	std::cout << Domains.size() << " Read from file\n";
}
