#include "Precomp.h"
#include "NetDemo.h"
#include "NetChannel.h"
#include "Package/PackageManager.h"
#include "Utils/File.h"
#include "Utils/Logger.h"
#include "Engine.h"
#include <chrono>
#include <thread>

namespace
{
	// A demo's file where the original's file manager finds the name: the
	// game's System folder, where it runs, unless the name says where.
	fs::path DemoPath(const std::string& filename)
	{
		fs::path path = convert_path_separators(filename);
		if (path.is_relative())
			path = engine->packages->GetSystemFolderPath() / path;
		return Directory::resolveCase(path);
	}
}

std::string NetDemoDriverSection()
{
	std::string section = engine->packages->GetIniValue("system", "Engine.Engine", "DemoRecordingDevice");
	return section.empty() ? std::string("Engine.DemoRecDriver") : section;
}

NetDemoDriver::NetDemoDriver(const std::string& section)
{
	Demo = true;

	// The section's values over UNetDriver's own, which are 0 but
	// MaxClientRate's 25,000 (UNetDriver::StaticConstructor, Engine.dll
	// 0x10409080), not [IpDrv.TcpNetDriver]'s: the stock section gives no
	// DynamicUpdateRate or StaticUpdateRate.
	ConnectionTimeout = 0.0f;
	InitialConnectTimeout = 0.0f;
	KeepAliveTime = 0.0f;
	RelevantTimeout = 0.0f;
	SpawnPrioritySeconds = 0.0f;
	ServerTravelPause = 0.0f;
	MaxClientRate = 25000;
	DynamicUpdateRate = 0;
	StaticUpdateRate = 0;
	NetServerMaxTickRate = 0;
	LanServerMaxTickRate = 0;
	LoadSettings(section);
	DemoSpectatorClass = engine->packages->GetIniValue("system", section, "DemoSpectatorClass");
}

NetDemoDriver::~NetDemoDriver()
{
	// Its connection goes first, its control channel closed, so a
	// recording's last packet closes it; then the file (UNetDriver::Destroy
	// 0x10409840, UNetConnection::Destroy 0x104019e0, LowLevelDestroy
	// 0x10407e50).
	auto close = [](NetConnection* connection)
	{
		if (NetChannel* control = connection->Channels[0])
			control->Close();
		connection->FlushNet();
	};
	for (auto& connection : ClientConnections)
		close(connection.get());
	if (ServerConnection)
		close(ServerConnection.get());
	ClientConnections.clear();
	ServerConnection.reset();

	LogMessage("Closing down demo driver.");
	if (File)
		fclose(File);
	File = nullptr;
}

bool NetDemoDriver::InitRecord(NetNotify* notify, const std::string& filename, std::string& error)
{
	Notify = notify;
	DemoFilename = filename;
	Time = 0.0;
	FrameNum = 0;

	// One connection, open, at 1,000,000 bytes a second: never short of room,
	// all it sends acknowledged by itself (UDemoRecConnection, Engine.dll
	// 0x104068a0, IsNetReady 0x10406c80; InitListen 0x10407870).
	auto connection = std::make_unique<NetConnection>(this, 0, 0, 1000000);
	connection->State = ConnectionState::Open;
	connection->InternalAck = true;
	ClientConnections.push_back(std::move(connection));

	File = fopen(DemoPath(filename).string().c_str(), "wb");
	if (!File)
	{
		error = "Couldn't open demo file " + filename + " for writing";
		return false;
	}
	return true;
}

bool NetDemoDriver::InitPlay(NetNotify* notify, const UnrealURL& url, std::string& error)
{
	Notify = notify;
	DemoFilename = url.Map;
	Time = 0.0;
	FrameNum = 0;

	// The server's connection, pending until the player comes, at the same
	// speed and acknowledging itself as a recording's (InitConnect,
	// Engine.dll 0x10407550).
	ServerConnection = std::make_unique<NetConnection>(this, 0, 0, 1000000);
	ServerConnection->State = ConnectionState::Pending;
	ServerConnection->InternalAck = true;

	File = fopen(DemoPath(DemoFilename).string().c_str(), "rb");
	if (!File)
	{
		error = "Couldn't open demo file " + DemoFilename + " for reading";
		return false;
	}
	fseek(File, 0, SEEK_END);
	FileSize = ftell(File);
	fseek(File, 0, SEEK_SET);

	ClientThirdPerson = url.HasOption("3rdperson");
	TimeBased = url.HasOption("timebased");
	NoFrameCap = url.HasOption("noframecap");
	return true;
}

void NetDemoDriver::TickDispatch(float deltaTime)
{
	NetDriver::TickDispatch(deltaTime);
	FrameNum++;

	NetConnection* connection = ServerConnection.get();
	if (!connection || !File || (connection->State != ConnectionState::Pending && connection->State != ConnectionState::Open))
		return;

	// Each packet whose frame has come, or with timebased whose time, one a
	// tick while the connection waits for its player; the file's end, or a
	// packet not there whole, closes the connection (TickDispatch,
	// Engine.dll 0x10407f60). A packet longer than any the protocol sends is
	// the file's end too, where the original reads it into a 520-byte
	// buffer whatever its size.
	using Clock = std::chrono::steady_clock;
	Clock::time_point waitStart;
	bool waited = false;
	for (;;)
	{
		long at = ftell(File);
		if (at < 0 || at >= FileSize)
			break;

		int32_t frame = 0;
		double time = 0.0;
		int32_t size = 0;
		uint8_t data[576];
		bool header = fread(&frame, 4, 1, File) == 1 && fread(&time, 8, 1, File) == 1;
		if (header)
		{
			if (TimeBased ? time > Time : frame > FrameNum)
			{
				fseek(File, at, SEEK_SET);
				return;
			}

			// Played by frame, no faster than it was made: a packet ahead of
			// the driver's time waits out the difference, counted from this
			// tick's first wait, where the original's waits it out again
			// for each packet, from the end of the last wait (0x10408052-
			// 0x1040811c), so a frame of several packets plays slower than
			// it was made (bugs.md D.<n>).
			if (!NoFrameCap && !TimeBased && time > Time)
			{
				if (!waited)
				{
					waitStart = Clock::now();
					waited = true;
				}
				std::this_thread::sleep_until(waitStart + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(time - Time)));
			}
		}

		bool read = header && fread(&size, 4, 1, File) == 1 && size >= 0 && size <= (int32_t)sizeof(data) &&
			fread(data, 1, (size_t)size, File) == (size_t)size;
		if (!read)
		{
			LogMessage("Failed to read demo file packet");
			break;
		}

		connection->ReceivedRawPacket(data, size);
		if (connection->State == ConnectionState::Pending)
			return;
	}
	connection->State = ConnectionState::Closed;
}

void NetDemoDriver::LowLevelSend(NetConnection* connection, const uint8_t* data, int count)
{
	if (ServerConnection || !File)
		return;
	int32_t frame = FrameNum;
	double time = Time;
	int32_t size = count;
	fwrite(&frame, 4, 1, File);
	fwrite(&time, 8, 1, File);
	fwrite(&size, 4, 1, File);
	fwrite(data, 1, (size_t)count, File);
}

/////////////////////////////////////////////////////////////////////////////

NetDemoPendingLevel::NetDemoPendingLevel(const UnrealURL& url) : NetPendingLevel(url, NoConnect())
{
	// Its driver opens the file (UDemoPlayPendingLevel, Engine.dll
	// 0x104058f0); one that cannot is let go, its error the playback's.
	auto driver = std::make_unique<NetDemoDriver>(NetDemoDriverSection());
	if (driver->InitPlay(this, URL, Error))
		Driver = std::move(driver);
}

void NetDemoPendingLevel::Tick(float deltaTime)
{
	// The file's packets in, the connection ticked (0x104065b0): its end, or
	// a package not to be had, leaves the playback waiting, as the
	// original's, until another join or playback takes its place.
	if (!Driver)
		return;
	Driver->TickDispatch(deltaTime);
	Driver->TickFlush();
}

void NetDemoPendingLevel::NotifyReceivedText(NetConnection* connection, const std::string& text)
{
	// The packages the level used and its WELCOME, nothing else
	// (UDemoPlayPendingLevel::NotifyReceivedText, Engine.dll 0x10406080).
	LogMessage("DemoPlayPendingLevel received: " + text);

	std::string value;
	if (NetParseCommand(text, "USES"))
	{
		UsedPackage package;
		package.Guid = std::string(32, '0');
		NetParseGuid(text, "GUID=", package.Guid);
		NetParseInt(text, "GEN=", package.Generation);
		NetParseInt(text, "SIZE=", package.Size);
		NetParseInt(text, "FLAGS=", package.Flags);
		if (NetParseValue(text, "PKG=", value, 64))
			package.Name = value;
		Uses.push_back(package);
	}
	else if (NetParseCommand(text, "WELCOME"))
	{
		std::string level;
		NetParseValue(text, "LEVEL=", level, 4096);

		// Each package is had, on the paths by its name or in the cache by
		// its GUID, or the playback stops here.
		for (const UsedPackage& package : Uses)
		{
			bool onPaths = engine->packages->HasPackage(package.Name);
			std::string file = engine->packages->FindPackageFile(package.Name, package.Guid);
			if (file.empty())
			{
				LogMessage("Don't have package for demo: " + package.Name.ToString());
				return;
			}
			if (!onPaths)
				engine->packages->UsePackageFile(package.Name, file);
		}

		// A server's demo has no player of its own: it is watched from a
		// spectator's place.
		if (NetParseValue(text, "SERVERDEMO", value))
			DemoDriver()->ClientThirdPerson = true;
		Driver->Time = 0.0;
		LevelName = !Uses.empty() ? Uses[0].Name.ToString() : level;
		Success = true;
	}
}

/////////////////////////////////////////////////////////////////////////////

void NetDemoClientLevel::NotifyClientPlayer(NetConnection* connection, UPlayerPawn* pawn)
{
	engine->HandleDemoClientPlayer(connection, pawn);
}
