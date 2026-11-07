#pragma once

#include <cstdint>
#include <string>

enum class ETravelType : uint8_t;

class UnrealURL
{
public:
	// The default URL, the game's [URL] section's, as the original's FURL()
	// makes it (Engine.dll 0x103f7850): its map Index.dx.
	UnrealURL();
	// A URL's text read on no base, an absolute travel's.
	UnrealURL(std::string urlString);
	// A URL's text read on a base, as the original's FURL(Base, URL, Type)
	// reads it (Engine.dll 0x103f7b60): one it refuses is the default URL,
	// Valid false.
	UnrealURL(const UnrealURL* baseURL, std::string urlString, ETravelType travelType);

	void AddOrReplaceOption(const std::string& newvalue);
	bool HasOption(std::string name) const;
	std::string GetOption(std::string name) const;
	std::string GetAddressURL() const;
	std::string GetOptions() const;
	std::string GetPortal() const;
	std::string ToString() const;
	bool IsInternal() const;
	bool Empty() const;
	void Clear();

	// A server's address, when the URL has one: its host, and its port (the
	// game's [URL] Port when not given).
	std::string Protocol = "unreal";
	std::string ProtocolDescription = "Unreal Protocol";
	std::string Name = "Player";
	std::string Map = "Index.unr";
	std::string Host;
	std::string Portal;
	std::string MapExt = "unr";
	std::string SaveExt = "usa";
	int Port = 7777;
	Array<std::string> Options;
	bool Valid = true;

private:
	void Refuse();
};
