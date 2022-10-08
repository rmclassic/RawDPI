#include "dns.h"
#include "base64url.h"
#include <openssl/ossl_typ.h>
#include <openssl/ssl.h>
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
	if (HostName == "adfree.usableprivacy.net")
		return "78.47.163.141";

	std::map<std::string, std::string>::iterator it = Domains.find(HostName);
	if (it != Domains.end())
		return Domains.at(HostName);

	// USE HTTPLIB TO GET CF DOH
	httplib::SSLClient cli("adfree.usableprivacy.net");
	cli.set_connection_timeout(4, 0);
	SSL_CTX_set_options(cli.ssl_context(), SSL_OP_NO_TLSv1_3);

	httplib::Headers headers = {
  		{ "Accept", "application/dns-json" }
	};

	char request_buffer[10240];
	size_t request_buff_len = DNSQuery::Question(HostName).Serialize(request_buffer);
	auto req_b64 = base64_encode(std::string(request_buffer, request_buff_len));

	//cli.set_proxy("127.0.0.1", 5585); //Set proxy to ourselves, because the Cloudflare may be blocked too
	auto res = cli.Get(("/query?dns=" + req_b64).c_str(), headers);
	if (res == nullptr || res->status != 200)
		return "";

	std::string IP;

	try
	{
		auto dnsRes = DNSQuery::Parse((const unsigned char*)res->body.c_str());
		for (auto answer : dnsRes.Answers)
		{
			if (answer.Class == 1 && answer.Type == 1)
				IP = answer.IPAddress();
		}
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
