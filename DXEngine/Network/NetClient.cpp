
#include "Precomp.h"
#include "NetClient.h"
#include "NetChannel.h"
#include "Package/PackageManager.h"
#include "Package/PackageFlags.h"
#include "Utils/Logger.h"
#include "Utils/StrTools.h"
#include "Utils/MD5.h"
#include "Packages/Engine/UConsole.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/Info/UPlayerReplicationInfo.h"
#include "VM/ScriptCall.h"
#include "Engine.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace
{
	bool EqualsIgnoreCase(const std::string& a, size_t pos, const char* b)
	{
		for (size_t i = 0; b[i]; i++)
		{
			if (pos + i >= a.size() || std::toupper((unsigned char)a[pos + i]) != std::toupper((unsigned char)b[i]))
				return false;
		}
		return true;
	}

	// A rate a second as the time between updates, 0.01 s to 1 s; none for 0.
	float RateInterval(int rate)
	{
		return rate != 0 ? std::clamp(1.0f / (float)rate, 0.01f, 1.0f) : 0.0f;
	}

	// The world stats checksum a login carries to a server that logs world
	// stats, as the original's pending level makes it (Engine.dll,
	// 0x1040a880): the MD5 of the player's name and its world stats password
	// (ngWorldSecret), each as the original's two-byte characters without a
	// terminator, in lowercase hex -- NoChecksum without a password.
	std::string WorldStatsChecksum()
	{
		UPlayerPawn* player = engine->viewport ? engine->viewport->Actor() : nullptr;
		if (!player || player->ngWorldSecret().empty())
			return "Checksum=NoChecksum";

		MD5 md5;
		auto hash = [&](const ScriptString& text)
		{
			for (char16_t c : text)
			{
				uint8_t wide[2] = { (uint8_t)(c & 0xFF), (uint8_t)(c >> 8) };
				md5.Update(wide, 2);
			}
		};
		if (player->PlayerReplicationInfo())
			hash(player->PlayerReplicationInfo()->PlayerName());
		hash(player->ngWorldSecret());
		uint8_t digest[16];
		md5.Final(digest);
		return "Checksum=" + MD5::Hex(digest);
	}

	// A word as the original's appStrtoi reads one in base 16 (Core.dll
	// 0x101235e0, MSVCRT's wcstoul): blanks skipped, a sign, a 0x, then the
	// hex digits up to the first that is not one, a minus negating what was
	// read; eight characters hold no more than 32 bits
	uint32_t HexWord(std::u16string_view text)
	{
		size_t pos = 0;
		while (pos < text.size() && (text[pos] == ' ' || (text[pos] >= '\t' && text[pos] <= '\r')))
			pos++;
		bool negative = false;
		if (pos < text.size() && (text[pos] == '+' || text[pos] == '-'))
			negative = text[pos++] == '-';
		if (pos + 1 < text.size() && text[pos] == '0' && (text[pos + 1] == 'x' || text[pos + 1] == 'X'))
			pos += 2;
		uint32_t value = 0;
		for (; pos < text.size(); pos++)
		{
			char16_t c = text[pos];
			int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
			if (digit < 0)
				break;
			value = value * 16 + (uint32_t)digit;
		}
		return negative ? 0u - value : value;
	}

}

int NetChallengeResponse(int challenge)
{
	uint32_t c = (uint32_t)challenge;
	return (int)((c * 237u) ^ (uint32_t)(challenge >> 16) ^ (c << 16) ^ 0x93fe92ceu);
}

// A line's command as Core's ParseCommand takes it (Core.dll 0x10147a20):
// past any spaces and tabs, the word in any case and no ASCII letter or
// digit straight after it; the rest of the line past the spaces and tabs
// after it.
bool NetParseCommand(const std::string& text, const char* command, std::string* rest)
{
	auto blank = [&](size_t pos) { return pos < text.size() && (text[pos] == ' ' || text[pos] == '\t'); };
	size_t pos = 0;
	while (blank(pos))
		pos++;
	if (!EqualsIgnoreCase(text, pos, command))
		return false;
	pos += strlen(command);
	if (pos < text.size())
	{
		char c = text[pos];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
			return false;
	}
	while (blank(pos))
		pos++;
	if (rest)
		*rest = text.substr(pos);
	return true;
}

// A key's value as Core's Parse finds it (Core.dll 0x10146bc0): the key in
// any case, but none straight after a letter or digit (appStrfind,
// 0x10145ca0), so HELLO's VER= is not the one in MINVER=; a value in double
// quotes up to the next one, any other up to a space, CR, LF, tab or comma
// -- a login's URL=Index.dx?Name=JC(1)?Class=... keeps the name's brackets
// and every option after them --, of the first maxLen - 1 characters at
// most: the original's buffers hold a login's URL= to 1,023 characters, a
// USES's PKG= to 63 and a WELCOME's LEVEL= to 4,095.
bool NetParseValue(const std::string& text, const char* key, std::string& value, size_t maxLen)
{
	return StrTools::ue1_parse(text, key, value, maxLen);
}

// A key's number as Parse's int form reads it (Core.dll 0x10147730):
// appAtoi of all that follows the key, nothing cut, so VER= 1100 is 1100
// and a quoted number 0; true whenever the key is found.
bool NetParseInt(const std::string& text, const char* key, int& value)
{
	size_t at = StrTools::ue1_strfind(text, key);
	if (at == std::string::npos)
		return false;
	value = StrTools::ue1_atoi(std::string_view(text).substr(at + strlen(key)));
	return true;
}

// A DWORD's (0x10147270): appStrtoi of all that follows, in base 10.
bool NetParseInt(const std::string& text, const char* key, uint32_t& value)
{
	size_t at = StrTools::ue1_strfind(text, key);
	if (at == std::string::npos)
		return false;
	value = StrTools::ue1_strtoul(std::string_view(text).substr(at + strlen(key)));
	return true;
}

// A bool as ParseUBOOL reads one (Core.dll 0x101477e0): Parse's text into
// its 16 characters, true for On, True or 1 in any case and false for
// anything else, 2 and Yes among them; true whenever the key is found.
bool NetParseBool(const std::string& text, const char* key, bool& value)
{
	std::string word;
	if (!StrTools::ue1_parse(text, key, word, 16))
		return false;
	value = StrTools::equals_ignore_case(word, "On") || StrTools::equals_ignore_case(word, "True") || word == "1";
	return true;
}

// A GUID as Parse's FGuid form reads one (Core.dll 0x101478e0): Parse's text
// into its 256 characters, four words of 0, and where that text is 32
// characters long each word the base-16 number of its eight, the last eight
// the last word; then written as FGuid's String writes them, 32 hex digits
// in upper case.
bool NetParseGuid(const std::string& text, const char* key, std::string& guid)
{
	std::string value;
	if (!StrTools::ue1_parse(text, key, value, 256))
		return false;
	uint32_t words[4] = {};
	ScriptString wide = UTF8ToScript(value);
	if (wide.size() == 32)
	{
		for (int i = 0; i < 4; i++)
			words[i] = HexWord(std::u16string_view(wide).substr(i * 8, 8));
	}
	char printed[33];
	snprintf(printed, sizeof(printed), "%08X%08X%08X%08X", words[0], words[1], words[2], words[3]);
	guid = printed;
	return true;
}

/////////////////////////////////////////////////////////////////////////////

NetPendingLevel::NetPendingLevel(const UnrealURL& url) : URL(url)
{
	Driver = std::make_unique<NetDriver>();
	if (!Driver->InitConnect(this, URL.Host, URL.Port, Error))
		return;

	NetConnection* connection = Driver->ServerConnection.get();
	if (URL.HasOption("LAN"))
	{
		std::string lanSpeed = engine->packages->GetIniValue("system", "Engine.Player", "ConfiguredLanSpeed");
		if (!lanSpeed.empty())
			connection->CurrentNetSpeed = std::atoi(lanSpeed.c_str());
	}

	connection->SendText("HELLO REVISION=0 MINVER=1100 VER=1100");
	connection->FlushNet();
}

NetPendingLevel::~NetPendingLevel()
{
	// Its connection goes with it, telling it nothing more.
	if (Driver)
		Driver->Notify = nullptr;
	Driver.reset();
}

void NetPendingLevel::Close()
{
	NetConnection* connection = Driver ? Driver->ServerConnection.get() : nullptr;
	if (!connection)
		return;
	if (NetChannel* control = connection->Channels[0])
		control->Close();
	connection->FlushNet();
}

bool NetPendingLevel::VerifyPackages(std::string& error)
{
	for (const UsedPackage& package : Uses)
	{
		std::string guid;
		if (engine->packages->IsPackageLoaded(package.Name))
			guid = engine->packages->GetPackage(package.Name)->GetGuidString();
		else
		{
			// None to be found fails as the original's load does (Core's
			// PackageNotFound, logged as a failed load)
			std::string file = engine->packages->FindPackageFile(package.Name, package.Guid);
			if (file.empty())
			{
				error = LocalizeMessage("Core", "Errors", "PackageNotFound", "Can't find file for package '%s'", { package.Name.ToString() });
				LogMessage("Failed to load '" + package.Name.ToString() + "': " + error);
				return false;
			}
			guid = PackageManager::ReadPackageGuid(file);
		}
		if (!StrTools::equals_ignore_case(guid, package.Guid))
		{
			error = LocalizeMessage("Core", "Errors", "PackageVersion", "Package '%s' version mismatch", { package.Name.ToString() });
			return false;
		}
	}
	return true;
}

void NetPendingLevel::Tick(float deltaTime)
{
	if (!Driver || !Error.empty())
		return;

	// A closed connection fails the join with the localized Connection
	// failed (UNetPendingLevel::Tick, Engine.dll 0x1040c170)
	NetConnection* connection = Driver->ServerConnection.get();
	if (connection->State == ConnectionState::Closed)
	{
		Error = LocalizeMessage("Engine", "Errors", "ConnectionFailed", "Connection failed");
		return;
	}

	Driver->TickDispatch(deltaTime);
	Driver->TickFlush();
}

bool NetPendingLevel::NotifyAcceptingChannel(NetChannel* channel)
{
	return channel->ChType == ChannelType::Control;
}

void NetPendingLevel::NotifyReceivedText(NetConnection* connection, const std::string& text)
{
	LogMessage("Net: pending level received: " + text);

	std::string rest, value;
	if (NetParseCommand(text, "UPGRADE"))
	{
		// A server of another version, shown as the original's pending
		// level shows it (UNetPendingLevel::NotifyReceivedText, Engine.dll
		// 0x1040a880) -- this side too old for a MINVER above its 1100, the
		// player's upgrade menu (-1 s: ShowUpgradeMenu, empty in the stock
		// game), else Connection failed over Server's version is outdated for
		// 6 s --, then the join ended, that shown left as it is, by the
		// owner's choice: the original's stays pending, connecting again,
		// until the player cancels it.
		int minVer = 0;
		NetParseInt(text, "MINVER=", minVer);
		if (minVer > 1100)
			engine->SetProgress("", "", -1.0f);
		else
			engine->SetProgress(LocalizeMessage("Engine", "Errors", "ConnectionFailed", "Connection failed"), LocalizeMessage("Engine", "Errors", "ServerOutdated", "Server's version is outdated"), 6.0f);
		Error = LocalizeMessage("Engine", "Errors", "ServerOutdated", "Server's version is outdated");
		ErrorShown = true;
		connection->State = ConnectionState::Closed;
	}
	else if (NetParseCommand(text, "FAILURE", &rest))
	{
		engine->SetProgress("Rejected By Server", rest, 10.0f);
		Error = "Rejected By Server: " + rest;
		connection->State = ConnectionState::Closed;
	}
	else if (NetParseCommand(text, "FAILCODE", &rest))
	{
		// The console asks again (a password wanted), from the URL without
		// the one tried; nothing else, as the original's pending level only
		// calls the console's ConnectFailure (UNetPendingLevel::
		// NotifyReceivedText, Engine.dll 0x1040a880): the join's error stays
		// the FAILURE's before it, "Rejected By Server: You need to enter a
		// password to join this game.", not the code, and a FAILCODE with
		// no FAILURE ends no join. The stock console's ConnectFailure does
		// nothing.
		UnrealURL retry = URL;
		Array<std::string> options;
		for (const std::string& option : retry.Options)
		{
			if (!EqualsIgnoreCase(option, 0, "PASSWORD="))
				options.push_back(option);
		}
		retry.Options = options;
		if (engine->console)
			CallEvent(engine->console, "ConnectFailure", { ExpressionValue::StringValue(rest), ExpressionValue::StringValue(retry.ToString()) });
	}
	else if (NetParseCommand(text, "USES"))
	{
		// The GUID as the original's four words, 0 for one missing or not
		// of 32 characters (FPackageInfo, Core.dll 0x1012b8b0), and written
		// back as their 32 hex digits: what names the package in the cache
		// is never the server's own text
		UsedPackage package;
		if (NetParseValue(text, "PKG=", value, 64))
			package.Name = value;
		package.Guid = std::string(32, '0');
		NetParseGuid(text, "GUID=", package.Guid);
		NetParseInt(text, "FLAGS=", package.Flags);
		NetParseInt(text, "SIZE=", package.Size);
		NetParseInt(text, "GEN=", package.Generation);
		Uses.push_back(package);
	}
	else if (NetParseCommand(text, "WELCOME"))
	{
		LogMessage("Net: welcomed by server: " + text);
		if (NetParseValue(text, "LEVEL=", value, 4096))
			URL.Map = value;
		NetParseBool(text, "LONE=", LonePlayer);
		NetParseInt(text, "CHALLENGE=", connection->Challenge);

		// Each package is on the paths by its name, or in the cache by its
		// GUID -- then loaded from there under its name --, or has a
		// <name>.dll in the game's System folder, or it is to be
		// downloaded: if this side allows downloads and the server lets
		// that package go (UNetPendingLevel::NotifyReceivedText, Engine.dll
		// 0x1040a880).
		for (UsedPackage& package : Uses)
		{
			bool onPaths = engine->packages->HasPackage(package.Name);
			std::string file = engine->packages->FindPackageFile(package.Name, package.Guid);
			if (!file.empty())
			{
				if (!onPaths)
					engine->packages->UsePackageFile(package.Name, file);
				continue;
			}
			std::error_code ec;
			fs::path dll = engine->packages->GetSystemFolderPath() / (package.Name.ToString() + ".dll");
			if (fs::is_regular_file(dll, ec) && fs::file_size(dll, ec) > 0 && !ec)
				continue;
			FilesNeeded++;
			package.Flags |= (uint32_t)PackageFlags::Need;
			if (!Driver->AllowDownloads || (package.Flags & (uint32_t)PackageFlags::AllowDownload) == 0)
			{
				Error = "Downloading '" + package.Name.ToString() + "' not allowed";
				connection->State = ConnectionState::Closed;
				return;
			}
		}
		ReceiveNextFile(connection);
		Success = true;
	}
	else if (NetParseCommand(text, "CHALLENGE"))
	{
		NetParseInt(text, "VER=", connection->NegotiatedVer);
		NetParseInt(text, "CHALLENGE=", connection->Challenge);
		int stats = 0;
		NetParseInt(text, "STATS=", stats);
		if (stats == 1)
			URL.AddOrReplaceOption(WorldStatsChecksum());

		// The URL the server logs in, as the original's pending level writes
		// it (Engine.dll 0x1040b4dd): the URL's own text (FURL::String,
		// 0x103f9210) with no host and no game type -- a port not the
		// default's still there, ":7791/" before the map.
		engine->SetMultiplayerName(URL);
		UnrealURL login = URL;
		login.Host.clear();
		login.Options.clear();
		for (const std::string& option : URL.Options)
		{
			if (!EqualsIgnoreCase(option, 0, "game="))
				login.Options.push_back(option);
		}

		connection->SendText("NETSPEED " + std::to_string(connection->CurrentNetSpeed));
		connection->SendText("LOGIN RESPONSE=" + std::to_string(NetChallengeResponse(connection->Challenge)) + " URL=" + login.ToString());
		connection->FlushNet();
	}
	else if (NetParseCommand(text, "DYNAMICRATE", &rest))
	{
		connection->DynamicUpdateInterval = RateInterval(StrTools::ue1_atoi(rest));
	}
	else if (NetParseCommand(text, "STATICRATE", &rest))
	{
		connection->StaticUpdateInterval = RateInterval(StrTools::ue1_atoi(rest));
	}
	else if (NetParseCommand(text, "USERFLAG", &rest))
	{
		connection->UserFlags = StrTools::ue1_atoi(rest);
	}
}

void NetPendingLevel::ReceiveNextFile(NetConnection* connection)
{
	for (size_t i = 0; i < Uses.size(); i++)
	{
		const UsedPackage& package = Uses[i];
		if ((package.Flags & (uint32_t)PackageFlags::Need) != 0)
		{
			NetFileChannel::Request(connection, (int)i, package.Name.ToString(), package.Guid, package.Size);
			return;
		}
	}
}

void NetPendingLevel::NotifyReceivedFile(NetConnection* connection, int packageIndex, const std::string& error)
{
	if (packageIndex < 0 || packageIndex >= (int)Uses.size())
		return;
	UsedPackage& package = Uses[packageIndex];
	if (!error.empty())
	{
		if (Error.empty())
			Error = LocalizeMessage("Engine", "Errors", "DownloadFailed", "Downloading package '%s' failed: %s", { package.Name.ToString(), error });
		return;
	}

	// In the cache now, and loaded from there under its name where the
	// search finds it, by CacheExt, as the original's: one written .uxx is
	// not found where CacheExt names another extension.
	package.Flags &= ~(uint32_t)PackageFlags::Need;
	FilesNeeded--;
	std::string cached = engine->packages->FindPackageFile(package.Name, package.Guid);
	if (!cached.empty())
		engine->packages->UsePackageFile(package.Name, cached);
	ReceiveNextFile(connection);
}

void NetPendingLevel::NotifyProgress(const std::string& line1, const std::string& line2, float seconds)
{
	engine->SetProgress(line1, line2, seconds);
}

/////////////////////////////////////////////////////////////////////////////

void NetClientLevel::TickNetClient(NetDriver& driver)
{
	// What the player uses goes this way: Deus Ex's client finds its
	// FrobTarget, which the server's ParseRightClick takes.
	NetConnection* connection = driver.ServerConnection.get();
	if (!connection || connection->State != ConnectionState::Open)
		return;
	for (size_t i = 0; i < connection->OpenChannels.size(); i++)
	{
		NetChannel* channel = connection->OpenChannels[i];
		if (channel->ChType != ChannelType::Actor || channel->Closing)
			continue;
		NetActorChannel* actorChannel = static_cast<NetActorChannel*>(channel);
		UPlayerPawn* pawn = UObject::TryCast<UPlayerPawn>(actorChannel->Actor);
		if (pawn && pawn->Player())
			actorChannel->ReplicateActor();
	}
}

bool NetClientLevel::NotifyAcceptingChannel(NetChannel* channel)
{
	return channel->ChType == ChannelType::Actor;
}

void NetClientLevel::NotifyReceivedText(NetConnection* connection, const std::string& text)
{
	LogMessage("Net: level received: " + text);

	std::string rest;
	if (NetParseCommand(text, "FAILURE", &rest))
		engine->NetFailure = rest.empty() ? "Rejected by the server" : rest;
	else if (NetParseCommand(text, "USERFLAG", &rest))
		connection->UserFlags = StrTools::ue1_atoi(rest);
}

void NetClientLevel::NotifyClientPlayer(NetConnection* connection, UPlayerPawn* pawn)
{
	engine->HandleClientPlayer(connection, pawn);
}
