
#include "Precomp.h"
#include "UnrealURL.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include "Utils/StrTools.h"

// A value of the game's [URL] section, as the original's FURL::StaticInit
// reads each (Engine.dll 0x103f6c50).
static std::string DefaultValue(const char* key, const char* fallback)
{
	return engine && engine->packages ? engine->packages->GetIniValue("system", "URL", key, fallback) : std::string(fallback);
}

// The game's [URL] Port: any URL's port unless it names another, as the
// original's FURL defaults it.
static int DefaultPort()
{
	return std::atoi(DefaultValue("Port", "7777").c_str());
}

// The game's [URL] Protocol, deusex, the same way: the one a map file's
// level holds, which its URL writes without.
static std::string DefaultProtocol()
{
	return DefaultValue("Protocol", "unreal");
}

// The game's [URL] Map, Index.dx, Host and Portal, both empty: a URL's
// where its text names none.
static std::string DefaultMap()
{
	return DefaultValue("Map", "Index.unr");
}

static std::string DefaultHost()
{
	return DefaultValue("Host", "");
}

static std::string DefaultPortal()
{
	return DefaultValue("Portal", "");
}

UnrealURL::UnrealURL() : Protocol(DefaultProtocol()), Map(DefaultMap()), Host(DefaultHost()), Portal(DefaultPortal()), Port(DefaultPort())
{
}

UnrealURL::UnrealURL(std::string urlString) : UnrealURL(nullptr, std::move(urlString), ETravelType::TRAVEL_Absolute)
{
}

// The text as the original's parse copies it into its buffer of 1,024
// characters (appStrncpy, Engine.dll 0x103f7d00-0x103f7d16): its first
// 1,023, one past U+FFFF counting two, as its UTF-16 has it -- such a
// character at the end dropped whole --, before anything is read of it.
static void CutToParseBuffer(std::string& text)
{
	const size_t bufferLen = 1024;
	size_t units = 0;
	for (size_t pos = 0; pos < text.size(); pos++)
	{
		// A character's length in UTF-16, at its UTF-8 lead byte
		unsigned char c = (unsigned char)text[pos];
		if ((c & 0xc0) == 0x80)
			continue;
		units += c >= 0xf0 ? 2 : 1;
		if (units >= bufferLen)
		{
			text.resize(pos);
			return;
		}
	}
}

// The original's parse, step by step: a URL is
// [protocol:][//][host[:port]/][map[/portal]][?option...][#portal]
UnrealURL::UnrealURL(const UnrealURL* baseURL, std::string urlString, ETravelType travelType) : UnrealURL()
{
	// A relative travel stays on the base's server, map and portal; a
	// relative or partial one keeps its options but PUSH, POP, PEER and LOAD
	if (baseURL && travelType == ETravelType::TRAVEL_Relative)
	{
		Protocol = baseURL->Protocol;
		Host = baseURL->Host;
		Map = baseURL->Map;
		Portal = baseURL->Portal;
		Port = baseURL->Port;
	}
	if (baseURL && travelType != ETravelType::TRAVEL_Absolute)
	{
		for (const std::string& option : baseURL->Options)
		{
			if (!StrTools::equals_ignore_case(option, "push") && !StrTools::equals_ignore_case(option, "pop") &&
				!StrTools::equals_ignore_case(option, "peer") && !StrTools::equals_ignore_case(option, "load"))
				Options.push_back(option);
		}
	}

	// Its first 1,023 characters (CutToParseBuffer); its leading spaces
	// passed over, its trailing ones kept
	CutToParseBuffer(urlString);
	urlString.erase(0, urlString.find_first_not_of(' '));

	// The options and a '#' portal first, each up to the next '?' or '#',
	// in any order: a blank in one makes no URL
	size_t mark = urlString.find_first_of("?#");
	for (size_t pos = mark; pos != std::string::npos;)
	{
		size_t next = urlString.find_first_of("?#", pos + 1);
		std::string part = urlString.substr(pos + 1, next == std::string::npos ? std::string::npos : next - pos - 1);
		if (part.find(' ') != std::string::npos)
		{
			Refuse();
			return;
		}
		if (urlString[pos] == '?')
			AddOrReplaceOption(part);
		else
			Portal = part;
		pos = next;
	}
	if (mark != std::string::npos)
		urlString.resize(mark);

	bool hasHost = false, hasMap = false;
	// On Linux an absolute path is what a drive letter is on Windows, by
	// choice: the original's parse refuses a '/' not followed by another
	bool absolutePath = false;
#ifndef _WIN32
	absolutePath = urlString.size() > 1 && urlString[0] == '/' && urlString[1] != '/';
#endif
	if ((urlString.size() > 2 && urlString[1] == ':') || absolutePath)
	{
		// A file by its drive letter: all of it the map, at the default
		// protocol and portal, with no host
		Protocol = DefaultProtocol();
		Host.clear();
		Map = urlString;
		Portal = DefaultPortal();
		urlString.clear();
		hasHost = hasMap = true;
	}
	else
	{
		// A protocol: the text before a ':' past the second character and
		// before any '.'
		size_t colon = urlString.find(':');
		size_t dot = urlString.find('.');
		if (colon != std::string::npos && colon > 1 && (dot == std::string::npos || colon < dot))
		{
			Protocol = urlString.substr(0, colon);
			urlString.erase(0, colon + 1);
		}

		// "//" before a host; a '/' alone makes no URL
		if (!urlString.empty() && urlString[0] == '/')
		{
			if (urlString.size() < 2 || urlString[1] != '/')
			{
				Refuse();
				return;
			}
			urlString.erase(0, 2);
			Host.clear();
			hasHost = true;
		}

		// A host: the text up to a '/', when it has a '.' past its first
		// character whose next letters are not the map's or a save's
		// extension; its port after a ':'. Its map the default one, none for
		// another protocol, unless the text after the '/' names one.
		dot = urlString.find('.');
		auto isExtension = [&](const std::string& ext) {
			if (ext.empty() || !StrTools::equals_ignore_case(urlString.substr(dot + 1, ext.size()), ext))
				return false;
			size_t after = dot + 1 + ext.size();
			return after >= urlString.size() || !std::isalnum((unsigned char)urlString[after]);
		};
		std::string mapExt = engine ? engine->packages->GetMapExtension() : MapExt;
		std::string saveExt = engine ? engine->packages->GetSaveExtension() : SaveExt;
		if (dot != std::string::npos && dot > 0 && !isExtension(mapExt) && !isExtension(saveExt))
		{
			size_t slash = urlString.find('/');
			std::string address = urlString.substr(0, slash);
			urlString = slash != std::string::npos ? urlString.substr(slash + 1) : std::string();
			size_t portPos = address.find(':');
			if (portPos != std::string::npos)
			{
				Port = std::atoi(address.substr(portPos + 1).c_str());
				address.resize(portPos);
			}
			Host = address;
			Map = IsInternal() ? DefaultMap() : std::string();
			hasHost = true;
		}
	}

	// An absolute travel of the game's protocol takes the base's player
	// options its text lacks or has empty, after its own, each logged: the
	// console's start keeps the player's name and class
	if (baseURL && travelType == ETravelType::TRAVEL_Absolute && IsInternal())
	{
		const char* playerOptions[] = { "Name=", "Team=", "Class=", "Skin=", "Face=", "Voice=", "OverrideClass=" };
		for (const std::string& option : baseURL->Options)
		{
			for (const char* name : playerOptions)
			{
				std::string key = name;
				if (StrTools::startswith(option, key, true))
				{
					if (GetOption(key.substr(0, key.size() - 1)).empty())
					{
						LogMessage("URL: Adding default option " + option);
						Options.push_back(option);
					}
					break;
				}
			}
		}
	}

	// The map; for the game's protocol a '/' in it marks a portal, up to a
	// second '/', past which anything makes no URL
	if (!urlString.empty())
	{
		if (IsInternal())
		{
			hasMap = true;
			size_t slash = urlString.find('/');
			if (slash != std::string::npos)
			{
				std::string portal = urlString.substr(slash + 1);
				urlString.resize(slash);
				size_t second = portal.find('/');
				if (second != std::string::npos)
				{
					if (second + 1 < portal.size())
					{
						Refuse();
						return;
					}
					portal.resize(second);
				}
				Portal = portal;
			}
		}
		Map = urlString;
	}

	// No blank in the protocol, host or portal, and a host, a map or an
	// option at least
	if (Protocol.find(' ') != std::string::npos || Host.find(' ') != std::string::npos || Portal.find(' ') != std::string::npos ||
		(!hasHost && !hasMap && Options.empty()))
		Refuse();
}

// A text the original's refuses: the URL the default one, marked so.
void UnrealURL::Refuse()
{
	*this = UnrealURL();
	Valid = false;
}

void UnrealURL::AddOrReplaceOption(const std::string& newvalue)
{
	size_t pos = newvalue.find('=');
	if (pos != std::string::npos)
	{
		std::string name = newvalue.substr(0, pos);
		for (char& c : name) c = std::tolower(c);
		for (std::string& option : Options)
		{
			if (option.size() >= name.size() + 1 && option[name.size()] == '=')
			{
				std::string key = option.substr(0, name.size());
				for (char& c : key) c = std::tolower(c);
				if (key == name)
				{
					option = newvalue;
					return;
				}
			}
		}
		Options.push_back(newvalue);
	}
	else
	{
		std::string name = newvalue;
		for (char& c : name) c = std::tolower(c);
		for (std::string& option : Options)
		{
			if (option.size() == name.size())
			{
				std::string key = option;
				for (char& c : key) c = std::tolower(c);
				if (key == name)
				{
					option = newvalue;
					return;
				}
			}
		}
		Options.push_back(newvalue);
	}
}

// An option whole, in any case, as the original's (Engine.dll 0x103f9df0):
// ?listen, not ?listen=1.
bool UnrealURL::HasOption(std::string name) const
{
	for (const std::string& option : Options)
	{
		if (StrTools::equals_ignore_case(option, name))
			return true;
	}
	return false;
}

std::string UnrealURL::GetOption(std::string name) const
{
	for (char& c : name) c = std::tolower(c);

	for (const std::string& option : Options)
	{
		if (option.size() >= name.size() + 1 && option[name.size()] == '=')
		{
			std::string key = option.substr(0, name.size());
			for (char& c : key) c = std::tolower(c);
			if (key == name)
				return option.substr(name.size() + 1);
		}
	}
	return {};
}

std::string UnrealURL::GetAddressURL() const
{
	return Host + ":" + std::to_string(Port);
}

std::string UnrealURL::GetOptions() const
{
	std::string result;
	for (const std::string& option : Options)
	{
		result += "?";
		result += option;
	}
	return result;
}

std::string UnrealURL::GetPortal() const
{
	return Portal;
}

// The game's own protocol, in any case (Engine.dll 0x103f97b0).
bool UnrealURL::IsInternal() const
{
	return StrTools::equals_ignore_case(Protocol, DefaultProtocol());
}

// As the original's FURL::String writes it (dx-reverse-info/network.md,
// addresses): the protocol when it is not the default, in any case, the host
// when it or the port is not, the port only when it is not, '#' before a
// portal.
std::string UnrealURL::ToString() const
{
	std::string result;

	bool defaultHost = StrTools::equals_ignore_case(Host, DefaultHost());
	if (!IsInternal())
	{
		result += Protocol;
		result += ":";
		if (!defaultHost)
			result += "//";
	}

	int defaultPort = DefaultPort();
	if (!defaultHost || Port != defaultPort)
	{
		result += Host;
		if (Port != defaultPort)
		{
			result += ":";
			result += std::to_string(Port);
		}
		result += "/";
	}

	result += Map;
	result += GetOptions();
	if (!Portal.empty())
	{
		result += "#";
		result += Portal;
	}

	return result;
}

bool UnrealURL::Empty() const
{
	return Map.empty() && Portal.empty() && Options.empty();
}

// Emptied whole, the port and protocol the defaults again: a relative
// travel merged onto a cleared URL kept a joined server's port. No map
// either, not the default Index.dx: a cleared travel URL is no travel
// (Engine::Tick takes a travel from a map or a host).
void UnrealURL::Clear()
{
	*this = UnrealURL();
	Map.clear();
}
