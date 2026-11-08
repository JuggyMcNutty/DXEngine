
#include "Precomp.h"
#include "UUdpLink.h"
#include "VM/ScriptCall.h"
#include "VM/Frame.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UFunction.h"
#include "Engine.h"

#ifdef WIN32
#include <winsock2.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

UUdpLink::~UUdpLink()
{
	ActorDestroyed();
}

// The socket closes with the actor, as the original's AInternetLink::Destroy
// closes it: a destroyed link held its port until a collection.
void UUdpLink::ActorDestroyed()
{
	if (handle != invalid_socket_value)
		CloseSocket(handle);
	handle = invalid_socket_value;
}

void UUdpLink::Tick(float elapsed)
{
	UInternetLink::Tick(elapsed);

	if (handle == invalid_socket_value)
		return;

	if (ReceiveMode() == RMODE_Event)
	{
		// One datagram a tick, as the original's, each its own event.
		char data[4096];
		IpAddr from = { 0, 0 };
		int count = Receive(from, data, sizeof(data) - 1);
		if (count < 0)
			return;
		data[count] = 0;

		UStructProperty prop({}, nullptr, ObjectFlags::NoFlags);
		if (LinkMode() == MODE_Text)
		{
			CallEvent(this, "ReceivedText", { IpAddrArg("ReceivedText", from, prop), ExpressionValue::StringValue(std::string(data)) });
		}
		else if (LinkMode() == MODE_Line)
		{
			// The whole datagram is the line.
			CallEvent(this, "ReceivedLine", { IpAddrArg("ReceivedLine", from, prop), ExpressionValue::StringValue(std::string(data)) });
		}
		else if (LinkMode() == MODE_Binary)
		{
			UFunction* func = FindEventFunction(this, "ReceivedBinary");
			if (func && func->Properties.size() >= 3)
			{
				uint8_t bytes[255] = {};
				memcpy(bytes, data, std::min(count, 255));
				CallEvent(this, "ReceivedBinary", { IpAddrArg("ReceivedBinary", from, prop), ExpressionValue::IntValue(count), ExpressionValue::At(bytes, func->Properties[2]) });
			}
		}
	}
	else
	{
		fd_set readable;
		FD_ZERO(&readable);
		FD_SET(handle, &readable);
		timeval timeout = {};
		int result = select((int)handle + 1, &readable, nullptr, nullptr, &timeout);
		DataPending() = (result > 0) ? 1 : 0;
	}
}

int UUdpLink::BindPort(int port, bool bUseNextAvailable)
{
	// One bind a link, as the original's (AUdpLink::execBindPort, IpDrv.dll
	// 0x1070c810): a second, or one after a failed bind, answers 0, logged
	// as already bound (0x1070c88b)
	if (bindFailed || handle != invalid_socket_value)
	{
		LogMessage("BindPort: already bound");
		return 0;
	}
	handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (handle == invalid_socket_value)
	{
		LastError = LastSocketError();
		LogMessage("BindPort: socket failed");
	}
	else
	{
		int on = 1;
		int bound = 0;
		if (setsockopt(handle, SOL_SOCKET, SO_BROADCAST, (const char*)&on, sizeof(on)) != 0)
		{
			LastError = LastSocketError();
			LogMessage("BindPort: setsockopt failed");
		}
		else if ((bound = BindSocket(handle, port, bUseNextAvailable)) == 0)
		{
			LastError = LastSocketError();
			LogMessage("BindPort: bind failed");
		}
		else
		{
			// Non-blocking once bound, as the original's (0x1070c931)
			SetNonBlocking(handle);
			Socket() = (int)handle;
			Port() = bound;
			return bound;
		}
		CloseSocket(handle);
		handle = invalid_socket_value;
	}
	// The socket closed and Socket 0, as the original's leaves it
	// (0x1070c9a2-0x1070c9b1): a link that sends and reads nothing from then
	// on, a second BindPort refused
	Socket() = 0;
	bindFailed = true;
	return 0;
}

int UUdpLink::Receive(IpAddr& from, char* data, int size)
{
	sockaddr_in addr = {};
#ifdef WIN32
	int addrlen = sizeof(addr);
#else
	socklen_t addrlen = sizeof(addr);
#endif
	int count = (int)recvfrom(handle, data, size, 0, (sockaddr*)&addr, &addrlen);
	if (count < 0)
		return -1;
	from.Addr = (int32_t)ntohl(addr.sin_addr.s_addr);
	from.Port = ntohs(addr.sin_port);
	return count;
}

int UUdpLink::ReadBinary(IpAddr& Addr, int Count, uint8_t* B)
{
	if (handle == invalid_socket_value)
		return 0;
	char data[255];
	int count = Receive(Addr, data, std::clamp(Count, 0, 255));
	if (count <= 0)
		return 0;
	memcpy(B, data, count);
	return count;
}

bool UUdpLink::SendBinary(const IpAddr& Addr, int Count, const uint8_t* B)
{
	if (handle == invalid_socket_value)
		return false;
	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl((uint32_t)Addr.Addr);
	addr.sin_port = htons((uint16_t)Addr.Port);
	int result = (int)sendto(handle, (const char*)B, std::clamp(Count, 0, 255), 0, (const sockaddr*)&addr, sizeof(addr));
	// False when nothing went, a failed send among it (the original's answers
	// False always, docs/NATIVES.md)
	return result > 0;
}

int UUdpLink::ReadText(IpAddr& Addr, std::string& Str)
{
	if (handle == invalid_socket_value)
		return 0;
	char data[4096];
	int count = Receive(Addr, data, sizeof(data) - 1);
	if (count <= 0)
		return 0;
	data[count] = 0;
	Str = data;
	return count;
}

bool UUdpLink::SendText(const IpAddr& Addr, const std::string& Str)
{
	// The text as it is, no line end in any mode; the original counts only
	// a send of nothing as failing.
	if (handle == invalid_socket_value)
		return false;
	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl((uint32_t)Addr.Addr);
	addr.sin_port = htons((uint16_t)Addr.Port);
	int result = (int)sendto(handle, Str.data(), (int)Str.size(), 0, (const sockaddr*)&addr, sizeof(addr));
	if (result < 0)
		LastError = LastSocketError();
	return result > 0;
}
