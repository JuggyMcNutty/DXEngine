#pragma once

class GCMarker;

#include "NetConnection.h"
#include <initializer_list>
#include <memory>
#include <atomic>
#include <string>
#include <unordered_map>

class NetChannel;
class UPlayerPawn;

// A message from a package's .int file ([section] key, else the fallback
// given), its %s, %i and %f filled in turn (Core's Localize and appSprintf).
struct LocalizeArg
{
	LocalizeArg(const std::string& text) : Text(text), IsText(true) {}
	LocalizeArg(const char* text) : Text(text), IsText(true) {}
	LocalizeArg(int number) : Number(number) {}
	LocalizeArg(double number) : Number(number) {}
	std::string Text;
	double Number = 0.0;
	bool IsText = false;
};
std::string LocalizeMessage(const char* package, const char* section, const char* key, const char* fallback, std::initializer_list<LocalizeArg> args = {});

// The address the game's socket binds, and the script's links (IpDrv's
// TcpLink and UdpLink), in host order: every one of the machine's but with
// MULTIHOME= or -PRIMARYNET.
uint32_t NetLocalBindAddress();

// What a driver tells its owner: the pending level while joining, the level
// once in (Engine's FNetworkNotify).
class NetNotify
{
public:
	virtual ~NetNotify() = default;
	// A server's: whether a new connection is taken.
	virtual bool NotifyAcceptingConnection() { return false; }
	virtual bool NotifyAcceptingChannel(NetChannel* channel) = 0;
	virtual void NotifyReceivedText(NetConnection* connection, const std::string& text) = 0;
	// The server's pawn for this client has arrived (the original's
	// HandleClientPlayer on the connection).
	virtual void NotifyClientPlayer(NetConnection* connection, UPlayerPawn* pawn) {}
	// A server's connection closed: its player leaves.
	virtual void NotifyConnectionClosed(NetConnection* connection) {}
	// Whether the other side may have the package it asks for by its GUID:
	// a server's clients may, a server may not.
	virtual bool NotifySendingFile(NetConnection* connection, const std::string& guid) { return false; }
	// A client's: a package's download ended, with its error or none.
	virtual void NotifyReceivedFile(NetConnection* connection, int packageIndex, const std::string& error) {}
	// Two lines for the player, shown for the seconds given (the engine's
	// SetProgress).
	virtual void NotifyProgress(const std::string& line1, const std::string& line2, float seconds) {}
};

// The UDP socket and its connections (IpDrv's TcpNetDriver over Engine's
// UNetDriver): packets in at the start of a tick, out at the end. As a
// client, one connection: the server's; as a server, one for each client
// that sends, taken as the level says.
class NetDriver
{
public:
	NetDriver();
	virtual ~NetDriver();

	bool InitConnect(NetNotify* notify, const std::string& host, int port, std::string& error);
	// A server's socket, on the port or the next free of the tries from it;
	// the one bound is in BoundPort
	bool InitListen(NetNotify* notify, int port, int tries, std::string& error);
	int BoundPort = 0;
	// A join's host name is looked up on a thread of its own, the game
	// going on, as the original's connection looks it up (IpDrv.dll
	// UTcpipConnection 0x107092d0); until it is, nothing is sent.
	struct ResolveJob
	{
		std::string Host;
		std::atomic<int> State{ 0 }; // 0 looking up, 1 found, 2 failed
		uint32_t Addr = 0;
	};
	std::shared_ptr<ResolveJob> Resolve;
	virtual void TickDispatch(float deltaTime);
	void TickFlush();

	virtual void LowLevelSend(NetConnection* connection, const uint8_t* data, int count);

	// A server's actor is gone: each client's channel for it closes.
	void NotifyActorDestroyed(UActor* actor);

	// What its connections hold (NetConnection::Mark).
	void Mark(GCMarker& marker);

	NetNotify* Notify = nullptr;
	// The address the socket is bound to, in host order: every one of the
	// machine's (0.0.0.0) but with MULTIHOME= or -PRIMARYNET, as IpDrv's is.
	uint32_t LocalAddr = 0;
	std::unique_ptr<NetConnection> ServerConnection;
	Array<std::unique_ptr<NetConnection>> ClientConnections;
	double Time = 0.0;

	// [IpDrv.TcpNetDriver]
	float ConnectionTimeout = 15.0f;
	float InitialConnectTimeout = 500.0f;
	float AckTimeout = 1.0f;
	float KeepAliveTime = 1.0f;
	float RelevantTimeout = 5.0f;
	float SpawnPrioritySeconds = 1.0f;
	float ServerTravelPause = 4.0f;
	int MaxClientRate = 20000;
	int DynamicUpdateRate = 40;
	int StaticUpdateRate = 12;
	// A dedicated server's ticks a second, on the internet and on a LAN.
	int NetServerMaxTickRate = 20;
	int LanServerMaxTickRate = 35;
	// A client's: whether it fetches a package it lacks from the server.
	bool AllowDownloads = true;
	// A demo's driver (NetDemoDriver): its connection writes to a file, or
	// reads from one.
	bool Demo = false;

	// A server's guard of ours against one host's, or many hosts', sources
	// holding its memory, where the original's makes a connection for every
	// new source, without limit (bugs.md NET.40): a connection whose client
	// has not answered the challenge goes after this many seconds with
	// nothing from it; a new source is let go while this many connections
	// have no player yet, of all and of its address. An honest join's
	// connection has no player for seconds, or for its downloads; a LAN
	// party of 16 behind one address joins at once.
	static constexpr float HandshakeTimeout = 30.0f;
	static constexpr int MaxWaitingConnections = 256;
	static constexpr int MaxWaitingConnectionsPerAddress = 32;

protected:
	// The settings of an ini section, each as it is where the section has
	// none.
	void LoadSettings(const std::string& section);

private:
	bool OpenSocket(int port, std::string& error);
	// Whether a new source's connection is made (bugs.md NET.40).
	bool RoomForConnection(uint32_t addr, int port);

	intptr_t Socket = -1;
	// A server's connections by address and port, each packet's found
	// without a scan of them all (bugs.md NET.40).
	std::unordered_map<uint64_t, NetConnection*> ConnectionsByAddress;
	bool RefusingConnections = false;
};
