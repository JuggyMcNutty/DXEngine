
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
#include <cerrno>
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
			// The port asked for, or the next one taken: an ephemeral one
			// leaves Port 0, as the original's writes the port it gave the
			// bind
			Port() = port != 0 ? bound : 0;
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

// A datagram longer than the room fails, as WinSock's read of one fails
// with WSAEMSGSIZE, the datagram gone either way: on Linux, whose read cuts
// it short, the read asks its whole length (MSG_TRUNC).
int UUdpLink::Receive(IpAddr& from, char* data, int size)
{
	sockaddr_in addr = {};
#ifdef WIN32
	int addrlen = sizeof(addr);
	int flags = 0;
#else
	socklen_t addrlen = sizeof(addr);
	int flags = MSG_TRUNC;
#endif
	int count = (int)recvfrom(handle, data, size, flags, (sockaddr*)&addr, &addrlen);
	if (count < 0)
	{
		LastError = LastSocketError();
		return -1;
	}
	if (count > size)
	{
#ifndef WIN32
		LastError = EMSGSIZE;
#endif
		return -1;
	}
	from.Addr = (int32_t)ntohl(addr.sin_addr.s_addr);
	from.Port = ntohs(addr.sin_port);
	// A read that takes a datagram leaves no error, as WinSock's under
	// Wine leaves the original's (GetLastError 0 after it)
	LastError = 0;
	return count;
}

// Whether a failed read only found nothing waiting
static bool ReadWouldBlock(int error)
{
#ifdef WIN32
	return error == WSAEWOULDBLOCK;
#else
	return error == EWOULDBLOCK || error == EAGAIN;
#endif
}

// Whether a failed read found a datagram longer than its room
static bool ReadTooLong(int error)
{
#ifdef WIN32
	return error == WSAEMSGSIZE;
#else
	return error == EMSGSIZE;
#endif
}

// Up to Count bytes, at most 255, of one datagram; a longer one fails and is
// gone, the failure logged as any but nothing waiting (AUdpLink::
// execReadBinary, IpDrv.dll 0x1070d6f0), its first Count bytes left in the
// array all the same, as the original's read fills it (UdpEdgesConsole)
int UUdpLink::ReadBinary(IpAddr& Addr, int Count, uint8_t* B)
{
	if (handle == invalid_socket_value)
		return 0;
	char data[255];
	int size = std::clamp(Count, 0, 255);
	int count = Receive(Addr, data, size);
	if (count < 0)
	{
		if (ReadTooLong(LastError))
			memcpy(B, data, size);
		if (!ReadWouldBlock(LastError))
			LogMessage("ReadBinary: Error reading text.");
		return 0;
	}
	if (count == 0)
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

// One datagram of up to 4,096 bytes as text, the string emptied first; a
// longer one fails and is gone, the failure logged as any but nothing
// waiting (AUdpLink::execReadText, IpDrv.dll 0x1070d380). The text is the
// datagram's alone, where the original's runs on past it into its buffer's
// earlier bytes to a NUL, at most 1,023 characters (bugs.md D.76)
int UUdpLink::ReadText(IpAddr& Addr, std::string& Str)
{
	Str.clear();
	if (handle == invalid_socket_value)
		return 0;
	char data[4097];
	int count = Receive(Addr, data, sizeof(data) - 1);
	if (count < 0)
	{
		if (!ReadWouldBlock(LastError))
			LogMessage("ReadText: Error reading text.");
		return 0;
	}
	if (count == 0)
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
