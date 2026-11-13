#pragma once

#include "NetDriver.h"
#include "NetClient.h"
#include "UnrealURL.h"
#include <cstdint>
#include <cstdio>
#include <string>

class UPlayerPawn;

// A demo (Engine's UDemoRecDriver, dx-reverse-info/network.md, demos): what a
// level sends a client, written to a file a packet at a time with the frame
// and the time it went in, and played back from the file as a client plays a
// server's level. A recording has one connection, which takes what a server
// sends a client, in single player too, and acknowledges all it sends
// itself; a playback's is the server's, its packets read as their frames
// come. The file is each packet's frame (4 bytes), its time (an 8-byte
// double), its size (4 bytes) and its bytes, little-endian.
class NetDemoDriver : public NetDriver
{
public:
	// Its settings from its own section, the class [Engine.Engine]
	// DemoRecordingDevice names ([Engine.DemoRecDriver]).
	explicit NetDemoDriver(const std::string& section);
	~NetDemoDriver() override;

	// A recording into the file (UDemoRecDriver::InitListen, Engine.dll
	// 0x10407870): the connection open, the file made; false, and why, where
	// it cannot be written.
	bool InitRecord(NetNotify* notify, const std::string& filename, std::string& error);
	// A playback of the file the URL's map names (InitConnect 0x10407550),
	// with the URL's options 3rdperson, timebased and noframecap; false, and
	// why, where it cannot be read.
	bool InitPlay(NetNotify* notify, const UnrealURL& url, std::string& error);

	// Each tick the driver's time and frame go on; a playback's packets are
	// read up to this frame, or this time (TickDispatch 0x10407f60).
	void TickDispatch(float deltaTime) override;
	// A recording's packet into the file after its frame, time and size
	// (UDemoRecConnection::LowLevelSend 0x10406a70); a playback sends
	// nothing.
	void LowLevelSend(NetConnection* connection, const uint8_t* data, int count) override;

	// A recording's connection; none once it has closed.
	NetConnection* RecordConnection() const { return ClientConnections.empty() ? nullptr : ClientConnections[0].get(); }

	std::string DemoFilename;
	// A third person's view of a playback, Engine.Spectator where it names
	// none (UDemoRecConnection::HandleClientPlayer 0x10406cc0).
	std::string DemoSpectatorClass;
	bool ClientThirdPerson = false;
	bool TimeBased = false;
	bool NoFrameCap = false;
	int FrameNum = 0;

private:
	FILE* File = nullptr;
	int64_t FileSize = 0;
};

// A playback before its map loads (Engine's UDemoPlayPendingLevel): the
// file's first packets, the packages the level used, then its WELCOME.
class NetDemoPendingLevel : public NetPendingLevel
{
public:
	explicit NetDemoPendingLevel(const UnrealURL& url);

	bool IsDemo() const override { return true; }
	void Tick(float deltaTime) override;

	bool NotifyAcceptingChannel(NetChannel* channel) override { return true; }
	void NotifyReceivedText(NetConnection* connection, const std::string& text) override;
	void NotifyReceivedFile(NetConnection* connection, int packageIndex, const std::string& error) override {}

	NetDemoDriver* DemoDriver() const { return static_cast<NetDemoDriver*>(Driver.get()); }

	// The level to load: the first package the demo uses, its level, as the
	// original's map load takes it (UGameEngine::LoadMap, Engine.dll
	// 0x1038c607-0x1038c6ab), or its WELCOME's LEVEL= where it uses none.
	std::string LevelName;
};

// The demo driver's section, the class [Engine.Engine] DemoRecordingDevice
// names: Engine.DemoRecDriver, the game's one, where it names none.
std::string NetDemoDriverSection();

// A recording's notify: its connection hears nothing.
class NetDemoRecordLevel : public NetNotify
{
public:
	bool NotifyAcceptingChannel(NetChannel* channel) override { return false; }
	void NotifyReceivedText(NetConnection* connection, const std::string& text) override {}
};

// A playback's level once its map is in: a client's, but for its player,
// who is the demo connection's (UDemoRecConnection::HandleClientPlayer,
// Engine.dll 0x10406cc0).
class NetDemoClientLevel : public NetClientLevel
{
public:
	void NotifyClientPlayer(NetConnection* connection, UPlayerPawn* pawn) override;
};
