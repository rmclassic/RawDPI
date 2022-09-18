#include <string>

struct ConnectionInfo
{
public:
	std::string SourceIP, DestinationIP;
	int SourcePort, DestinationPort;
	int ClientTunnel, ServerTunnel;
};