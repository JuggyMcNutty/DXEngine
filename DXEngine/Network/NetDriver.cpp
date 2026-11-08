
#include "Precomp.h"
#include "NetDriver.h"
#include "NetChannel.h"
#include "Package/PackageManager.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Utils/Logger.h"
#include "Utils/StrTools.h"
#include "Engine.h"
#include "OriginalCommandLine.h"
#include <algorithm>
#include <thread>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace
{
#ifdef WIN32
	constexpr intptr_t NoSocket = (intptr_t)INVALID_SOCKET;
	bool WouldBlock() { return WSAGetLastError() == WSAEWOULDBLOCK; }
	bool ConnectionReset() { return WSAGetLastError() == WSAECONNRESET; }
	void CloseSocket(intptr_t s) { closesocket((SOCKET)s); }
	void SetNonBlocking(intptr_t s) { u_long nonblocking = 1; ioctlsocket((SOCKET)s, FIONBIO, &nonblocking); }
#else
	constexpr intptr_t NoSocket = -1;
	bool WouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK; }
	bool ConnectionReset() { return errno == ECONNREFUSED || errno == ECONNRESET; }
	void CloseSocket(intptr_t s) { close((int)s); }
	void SetNonBlocking(intptr_t s) { fcntl((int)s, F_SETFL, fcntl((int)s, F_GETFL, 0) | O_NONBLOCK); }
#endif

	float IniFloat(const char* key, float defaultValue)
	{
		std::string value = engine->packages->GetIniValue("system", "IpDrv.TcpNetDriver", key);
		return value.empty() ? defaultValue : (float)std::atof(value.c_str());
	}

	int IniInt(const char* section, const char* key, int defaultValue)
	{
		std::string value = engine->packages->GetIniValue("system", section, key);
		return value.empty() ? defaultValue : std::atoi(value.c_str());
	}
}

std::string LocalizeMessage(const char* package, const char* section, const char* key, const char* fallback, std::initializer_list<LocalizeArg> args)
{
	std::string format = engine->packages->Localize(package, section, key);
	if (format.empty())
		format = fallback;

	std::string result;
	auto arg = args.begin();
	for (size_t i = 0; i < format.size(); i++)
	{
		if (format[i] != '%')
		{
			result += format[i];
			continue;
		}
		size_t end = format.find_first_not_of("0123456789.-+ #", i + 1);
		if (end == std::string::npos)
			break;
		char conversion = format[end];
		std::string spec = format.substr(i, end - i + 1);
		i = end;
		if (conversion == '%')
		{
			result += '%';
			continue;
		}
		if (arg == args.end())
			continue;
		char buffer[256] = {};
		if (conversion == 's')
			result += arg->IsText ? arg->Text : std::string();
		else if (conversion == 'f' || conversion == 'g' || conversion == 'e')
			snprintf(buffer, sizeof(buffer), spec.c_str(), arg->Number);
		else if (conversion == 'x' || conversion == 'X' || conversion == 'u')
			snprintf(buffer, sizeof(buffer), spec.c_str(), (unsigned int)(int64_t)arg->Number);
		else
			snprintf(buffer, sizeof(buffer), (spec.substr(0, spec.size() - 1) + "d").c_str(), (int)arg->Number);
		result += buffer;
		++arg;
	}
	return result;
}

NetDriver::NetDriver()
{
	LoadSettings();
}

NetDriver::~NetDriver()
{
	ServerConnection.reset();
	ClientConnections.clear();
	if (Socket != NoSocket)
		CloseSocket(Socket);
}

void NetDriver::LoadSettings()
{
	ConnectionTimeout = IniFloat("ConnectionTimeout", ConnectionTimeout);
	InitialConnectTimeout = IniFloat("InitialConnectTimeout", InitialConnectTimeout);
	AckTimeout = IniFloat("AckTimeout", AckTimeout);
	KeepAliveTime = IniFloat("KeepAliveTime", KeepAliveTime);
	RelevantTimeout = IniFloat("RelevantTimeout", RelevantTimeout);
	SpawnPrioritySeconds = IniFloat("SpawnPrioritySeconds", SpawnPrioritySeconds);
	ServerTravelPause = IniFloat("ServerTravelPause", ServerTravelPause);
	MaxClientRate = IniInt("IpDrv.TcpNetDriver", "MaxClientRate", MaxClientRate);
	DynamicUpdateRate = IniInt("IpDrv.TcpNetDriver", "DynamicUpdateRate", DynamicUpdateRate);
	StaticUpdateRate = IniInt("IpDrv.TcpNetDriver", "StaticUpdateRate", StaticUpdateRate);
	NetServerMaxTickRate = IniInt("IpDrv.TcpNetDriver", "NetServerMaxTickRate", NetServerMaxTickRate);
	LanServerMaxTickRate = IniInt("IpDrv.TcpNetDriver", "LanServerMaxTickRate", LanServerMaxTickRate);
	std::string allowDownloads = engine->packages->GetIniValue("system", "IpDrv.TcpNetDriver", "AllowDownloads");
	if (!allowDownloads.empty())
		AllowDownloads = StrTools::equals_ignore_case(allowDownloads, "True") || allowDownloads == "1";
}

bool NetDriver::InitConnect(NetNotify* notify, const std::string& host, int port, std::string& error)
{
	Notify = notify;

	// The server's address: a dotted number as it is, a name looked up on a
	// thread of its own (the original's connection does: IpDrv.dll
	// UTcpipConnection 0x107092d0, "Resolving"), the game going on; a lookup
	// that failed ends the join (TickDispatch), and nothing goes out before
	// it answers (LowLevelSend). The thread is let go of: a slow resolver
	// holds no one up, and the job outlives the driver.
	uint32_t addr = 0;
	in_addr dotted = {};
	if (inet_pton(AF_INET, host.c_str(), &dotted) == 1)
	{
		addr = ntohl(dotted.s_addr);
	}
	else
	{
		Resolve = std::make_shared<ResolveJob>();
		Resolve->Host = host;
		LogMessage("Net: resolving " + host);
		std::shared_ptr<ResolveJob> job = Resolve;
		std::thread([job]()
		{
			addrinfo hints = {};
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_DGRAM;
			addrinfo* info = nullptr;
			if (getaddrinfo(job->Host.c_str(), nullptr, &hints, &info) == 0 && info)
			{
				job->Addr = ntohl(((sockaddr_in*)info->ai_addr)->sin_addr.s_addr);
				freeaddrinfo(info);
				job->State = 1;
			}
			else
			{
				job->State = 2;
			}
		}).detach();
	}

	if (!OpenSocket(0, error))
		return false;

	// The speed the client asks for: [Engine.Player]'s for the internet,
	// its LAN one with ?LAN in the URL (the caller's to choose).
	int netSpeed = IniInt("Engine.Player", "ConfiguredInternetSpeed", 2600);
	ServerConnection = std::make_unique<NetConnection>(this, addr, port, netSpeed);
	ServerConnection->CreateChannel(ChannelType::Control, true, 0);
	return true;
}

bool NetDriver::InitListen(NetNotify* notify, int port, int tries, std::string& error)
{
	Notify = notify;
	// The port, or the next free of the tries from it, as the original's
	// server binds (IpDrv.dll UTcpNetDriver::InitBase 0x1070abb0): a second
	// server on the machine takes the next
	for (int i = 0; i < tries; i++)
	{
		if (OpenSocket(port + i, error))
		{
			BoundPort = port + i;
			return true;
		}
	}
	return false;
}

// The address a socket binds, in host order, as the original's
// getlocalbindaddr chooses it (IpDrv.dll 0x10708e40, getlocalhostaddr
// 0x10703eb0): MULTIHOME='s a.b.c.d, read into 256 characters (0x10703fb1),
// when it has four parts, each part's number, and every address when it has
// not, logged; with -PRIMARYNET the first address of the machine's own name;
// else every one of the machine's (0.0.0.0).
uint32_t NetLocalBindAddress()
{
	const OriginalCommandLine& commandLine = OriginalCommandLine::Get();
	std::string multihome;
	if (commandLine.Value("MULTIHOME", multihome, 256))
	{
		size_t first = multihome.find('.');
		size_t second = first != std::string::npos ? multihome.find('.', first + 1) : std::string::npos;
		size_t third = second != std::string::npos ? multihome.find('.', second + 1) : std::string::npos;
		if (third == std::string::npos)
		{
			LogMessage("Invalid multihome IP address " + multihome);
			return 0;
		}
		uint32_t a = (uint32_t)std::atoi(multihome.c_str()) & 0xff;
		uint32_t b = (uint32_t)std::atoi(multihome.c_str() + first + 1) & 0xff;
		uint32_t c = (uint32_t)std::atoi(multihome.c_str() + second + 1) & 0xff;
		uint32_t d = (uint32_t)std::atoi(multihome.c_str() + third + 1) & 0xff;
		return (a << 24) | (b << 16) | (c << 8) | d;
	}
	if (commandLine.Param("PRIMARYNET"))
	{
		char name[256] = {};
		addrinfo hints = {};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		addrinfo* info = nullptr;
		if (gethostname(name, sizeof(name) - 1) == 0 && getaddrinfo(name, nullptr, &hints, &info) == 0 && info)
		{
			uint32_t addr = ntohl(((sockaddr_in*)info->ai_addr)->sin_addr.s_addr);
			freeaddrinfo(info);
			return addr;
		}
		LogMessage("gethostbyname failed for " + std::string(name));
	}
	return 0;
}

bool NetDriver::OpenSocket(int port, std::string& error)
{
	if (Socket != NoSocket)
	{
		CloseSocket(Socket);
		Socket = NoSocket;
	}
	Socket = (intptr_t)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (Socket == NoSocket)
	{
		error = "Could not create a socket";
		return false;
	}
	SetNonBlocking(Socket);

	// As the original's socket is set up (InitBase): broadcast and address
	// reuse on, the buffers 32 KB for a client's and 128 KB for a server's
	int on = 1;
	setsockopt((decltype(socket(0, 0, 0)))Socket, SOL_SOCKET, SO_BROADCAST, (const char*)&on, sizeof(on));
	setsockopt((decltype(socket(0, 0, 0)))Socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
	int buffer = port != 0 ? 0x20000 : 0x8000;
	setsockopt((decltype(socket(0, 0, 0)))Socket, SOL_SOCKET, SO_SNDBUF, (const char*)&buffer, sizeof(buffer));
	setsockopt((decltype(socket(0, 0, 0)))Socket, SOL_SOCKET, SO_RCVBUF, (const char*)&buffer, sizeof(buffer));

	sockaddr_in local = {};
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = htonl(NetLocalBindAddress());
	local.sin_port = htons((uint16_t)port);
	if (bind((decltype(socket(0, 0, 0)))Socket, (sockaddr*)&local, sizeof(local)) != 0)
	{
		error = "Could not bind port " + std::to_string(port);
		return false;
	}
	LocalAddr = ntohl(local.sin_addr.s_addr);
	return true;
}

void NetDriver::TickDispatch(float deltaTime)
{
	Time += deltaTime;

	// The host name's answer: the server's address, or the join's end
	if (Resolve && ServerConnection)
	{
		int state = Resolve->State;
		if (state == 1)
		{
			ServerConnection->RemoteAddr = Resolve->Addr;
			LogMessage("Net: resolved " + Resolve->Host + " to " + NetAddressString(Resolve->Addr));
			Resolve.reset();
		}
		else if (state == 2)
		{
			LogMessage("Net: could not resolve " + Resolve->Host);
			ServerConnection->State = ConnectionState::Closed;
			Resolve.reset();
		}
	}

	// A server lets go of the connections that closed, and their players.
	for (size_t i = ClientConnections.size(); i-- > 0;)
	{
		if (ClientConnections[i]->State == ConnectionState::Closed)
		{
			std::unique_ptr<NetConnection> connection = std::move(ClientConnections[i]);
			ClientConnections.erase(ClientConnections.begin() + i);
			if (Notify)
				Notify->NotifyConnectionClosed(connection.get());
		}
	}

	if (Socket == NoSocket)
		return;

	uint8_t data[4096];
	for (;;)
	{
		sockaddr_in from = {};
		socklen_t fromSize = sizeof(from);
		int size = (int)recvfrom((decltype(socket(0, 0, 0)))Socket, (char*)data, sizeof(data), 0, (sockaddr*)&from, &fromSize);
		if (size < 0)
		{
			if (WouldBlock())
				break;
			if (ConnectionReset())
				continue;
			LogMessage("Net: receive failed");
			break;
		}

		uint32_t addr = ntohl(from.sin_addr.s_addr);
		int port = ntohs(from.sin_port);
		NetConnection* connection = nullptr;
		if (ServerConnection && addr == ServerConnection->RemoteAddr && port == ServerConnection->RemotePort)
			connection = ServerConnection.get();
		for (size_t i = 0; !connection && i < ClientConnections.size(); i++)
		{
			if (addr == ClientConnections[i]->RemoteAddr && port == ClientConnections[i]->RemotePort)
				connection = ClientConnections[i].get();
		}

		// A new client, when the level takes it; its connection is open
		// from the first packet.
		if (!connection && !ServerConnection && Notify && Notify->NotifyAcceptingConnection())
		{
			ClientConnections.push_back(std::make_unique<NetConnection>(this, addr, port, IniInt("Engine.Player", "ConfiguredInternetSpeed", 2600)));
			connection = ClientConnections.back().get();
			connection->State = ConnectionState::Open;
			LogMessage("Net: open " + std::to_string(addr >> 24) + "." + std::to_string((addr >> 16) & 255) + "." + std::to_string((addr >> 8) & 255) + "." + std::to_string(addr & 255) + ":" + std::to_string(port));
		}

		if (connection)
			connection->ReceivedRawPacket(data, size);
	}
}

void NetDriver::TickFlush()
{
	if (ServerConnection)
		ServerConnection->Tick();
	for (auto& connection : ClientConnections)
		connection->Tick();
}

void NetDriver::NotifyActorDestroyed(UActor* actor)
{
	for (size_t i = ClientConnections.size(); i-- > 0;)
	{
		NetConnection* connection = ClientConnections[i].get();
		if (actor->bNetTemporary())
		{
			auto& sent = connection->SentTemporaries;
			sent.erase(std::remove(sent.begin(), sent.end(), actor), sent.end());
		}
		if (NetActorChannel* channel = connection->FindActorChannel(actor))
			channel->Close();
	}
}

void NetDriver::LowLevelSend(NetConnection* connection, const uint8_t* data, int count)
{
	// The server's name still being looked up: nothing to send to yet, as
	// the original's connection sends nothing while resolving
	if (Resolve && connection == ServerConnection.get())
		return;
	if (Socket == NoSocket)
		return;
	sockaddr_in to = {};
	to.sin_family = AF_INET;
	to.sin_addr.s_addr = htonl(connection->RemoteAddr);
	to.sin_port = htons((uint16_t)connection->RemotePort);
	sendto((decltype(socket(0, 0, 0)))Socket, (const char*)data, count, 0, (sockaddr*)&to, sizeof(to));
}

void NetDriver::Mark(GCMarker& marker)
{
	if (ServerConnection)
		ServerConnection->Mark(marker);
	for (auto& connection : ClientConnections)
		connection->Mark(marker);
}
