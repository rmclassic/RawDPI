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

#define DOH_HOST "adfree.usableprivacy.net"
#define DOH_IP "78.47.163.141"

std::map<std::string, std::string> Domains;
std::mutex mapmtx;
std::mutex filemtx;

std::ofstream ostr;

void InitCache()
{
	if (!ostr.is_open())
		ostr.open("hosts.txt", std::ios::app);
}

bool IsIpAddress(std::string host)
{
	std::regex r("^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)\\.?\\b){4}$");
	return std::regex_match(host, r);
}

void SaveIPToFile(std::string host, std::string ip)
{
	filemtx.lock();
	if (ostr.is_open())
	{
		ostr << host + " " + ip + '\n';
		std::cout << "RESOLVED " << ip << " FOR " << host << '\n';
	}
	filemtx.unlock();
}

std::string CacheGetDomain(std::string HostName)
{
	mapmtx.lock();
	std::map<std::string, std::string>::iterator it = Domains.find(HostName);
	if (it != Domains.end())
	{
		auto ip = Domains.at(HostName);
		mapmtx.unlock();
		return ip;
	}

	mapmtx.unlock();
	return "";
}

void CacheDomain(std::string HostName, std::string ip)
{
	mapmtx.lock();
	Domains[HostName] = ip;
	mapmtx.unlock();
}

std::string ResolveDOHIP(std::string HostName)
{
	if (IsIpAddress(HostName))
		return HostName;

	std::cout << "IP IS ? " << IsIpAddress(HostName);
	// Return Cloudflare IP for DNS over HTTPS
	if (HostName == DOH_HOST)
		return DOH_IP;



	// USE HTTPLIB TO GET CF DOH
	httplib::SSLClient cli(DOH_HOST);
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

	SaveIPToFile(HostName, IP);
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
	ins.close();
}
