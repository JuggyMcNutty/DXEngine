#pragma once

class GCMarker;

#include "NetDriver.h"
#include "UnrealURL.h"
#include <map>
#include <string>

class Package;
class UObject;
class UPlayerPawn;

// A server's level (ULevel's side of the network, dx-reverse-info/network.md):
// which connections and channels it takes, the handshake's server side, the
// player each client is given, and the packages a client needs.
class NetServerLevel : public NetNotify
{
public:
	explicit NetServerLevel(Package* level);
	~NetServerLevel();

	// The packages the clients are told of: the level's, the ServerPackages
	// the game engine's section lists, and the game's (Engine's
	// UGameEngine::BuildServerMasterMap), each with what it imports.
	void BuildMasterMap(UObject* game);

	// The level and the packages it offers.
	void Mark(GCMarker& marker);

	bool NotifyAcceptingConnection() override;
	bool NotifyAcceptingChannel(NetChannel* channel) override;
	void NotifyReceivedText(NetConnection* connection, const std::string& text) override;
	void NotifyConnectionClosed(NetConnection* connection) override;
	bool NotifySendingFile(NetConnection* connection, const std::string& guid) override;

	// Each tick, before the packets go: what each client is sent of the
	// level (ULevel::TickNetServer).
	void TickNetServer(float deltaSeconds);

	// The packages the clients are told of, in the order both sides number
	// them: each package, then what it imports, depth first, less what is
	// only the server's.
	Array<NetPackageMap::PackageInfo> Packages;

private:
	void Welcome(NetConnection* connection);
	void Join(NetConnection* connection);
	int ServerTickClient(NetConnection* connection);

	Package* Level = nullptr;
	std::map<NetConnection*, UnrealURL> RequestURLs;
	// Marks the actors one client's tick has looked at.
	int NetTag = 0;
};

// A client's player on a server is its connection, a Player object for the
// scripts (Engine.NetConnection); which connection each one is.
NetConnection* NetConnectionOfPlayer(UObject* player);

// The packages a level's clients are told of, a server's and a demo's
// (UGameEngine::BuildServerMasterMap, Engine.dll 0x10388b10): the level's,
// the ServerPackages the game engine's section lists, and the game's, each
// with what it imports, depth first, less what is only the server's.
Array<NetPackageMap::PackageInfo> NetBuildMasterMap(Package* level, UObject* game);

// The level's welcome to a connection (ULevel::WelcomePlayer, Engine.dll
// 0x1039f500): the packages given its map, a USES line for each, WELCOME
// with the level, LONE= and what the caller adds -- a demo's CLIENTDEMO or
// SERVERDEMO --, then the driver's DYNAMICRATE and STATICRATE.
void NetWelcome(NetConnection* connection, const Array<NetPackageMap::PackageInfo>& packages, Package* level, const std::string& optional);
