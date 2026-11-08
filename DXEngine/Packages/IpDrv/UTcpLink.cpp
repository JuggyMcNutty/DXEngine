
#include "Precomp.h"
#include "UTcpLink.h"
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
#define SHUT_WR SD_SEND
static bool WouldBlock(int error) { return error == WSAEWOULDBLOCK; }
static bool InProgress(int error) { return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS; }
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
static bool WouldBlock(int error) { return error == EWOULDBLOCK || error == EAGAIN; }
static bool InProgress(int error) { return error == EINPROGRESS || error == EWOULDBLOCK || error == EAGAIN; }
#endif

static bool Readable(socket_t s)
{
	fd_set set;
	FD_ZERO(&set);
	FD_SET(s, &set);
	timeval timeout = {};
	return select((int)s + 1, &set, nullptr, nullptr, &timeout) > 0;
}

// Whether a socket holds a connection. Windows' select shows one that does
// not as neither readable nor writable, and its shutdown fails it, where
// Linux's select calls it both, and its shutdown passes a listening socket
// and ends a connect under way.
static bool HasPeer(socket_t s)
{
	sockaddr_in peer = {};
#ifdef WIN32
	int size = sizeof(peer);
#else
	socklen_t size = sizeof(peer);
#endif
	return getpeername(s, (sockaddr*)&peer, &size) == 0;
}

// Whether a connection takes a send now, as Windows' select answers
static bool Writable(socket_t s)
{
	if (s == invalid_socket_value)
		return false;
	fd_set set;
	FD_ZERO(&set);
	FD_SET(s, &set);
	timeval timeout = {};
	return select((int)s + 1, nullptr, &set, nullptr, &timeout) > 0 && HasPeer(s);
}

UTcpLink::~UTcpLink()
{
	ActorDestroyed();
}

// The sockets close with the actor, as the original's AInternetLink::Destroy
// closes them: a destroyed link held its port until a collection.
void UTcpLink::ActorDestroyed()
{
	if (remote != invalid_socket_value)
		CloseSocket(remote);
	remote = invalid_socket_value;
	if (handle != invalid_socket_value)
		CloseSocket(handle);
	handle = invalid_socket_value;
}

// The socket BindPort makes, as the original's (ATcpLink::execBindPort,
// IpDrv.dll 0x10706960): address reuse on, and its close abortive -- SO_LINGER
// on with 0 s (0x10706a22) --, so closing it resets the connection it holds
// where a FIN would end it: a link destroyed while connected, one whose peer
// closed first, and a scripted Close, whose shutdown has sent its FIN
// first (ShutdownConnection). The connections a listening link accepts
// inherit it. An option that does not set fails the bind, the socket
// closed, as the original's.
bool UTcpLink::EnsureSocket()
{
	if (handle != invalid_socket_value)
		return true;
	handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (handle == invalid_socket_value)
	{
		LastError = LastSocketError();
		LogMessage("BindPort: socket failed");
		return false;
	}
	int on = 1;
	setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
	linger abortive = {};
	abortive.l_onoff = 1;
	abortive.l_linger = 0;
	if (setsockopt(handle, SOL_SOCKET, SO_LINGER, (const char*)&abortive, sizeof(abortive)) != 0)
	{
		LastError = LastSocketError();
		LogMessage("BindPort: setsockopt failed");
		CloseSocket(handle);
		handle = invalid_socket_value;
		return false;
	}
	SetNonBlocking(handle);
	Socket() = (int)handle;
	handleLive = false;
	return true;
}

// Whether the tick sees the socket, as Windows shows it: the link's own one
// that listens, or holds or held a connection, and a connection accepted; not
// the link's own one that never connected -- bound alone, or its connect
// under way or failed --, which reads nothing and never ends, where Linux's
// calls it hung up and reads a failed connect as ended.
bool UTcpLink::Shown(socket_t s)
{
	if (s != handle || handleLive)
		return true;
	if (!HasPeer(s))
		return false;
	handleLive = true;
	return true;
}

void UTcpLink::Tick(float elapsed)
{
	UInternetLink::Tick(elapsed);

	// A link whose BindPort failed does nothing, its Socket 0; any other goes
	// by its state, socket or none, as the original's (ATcpLink::Tick,
	// IpDrv.dll 0x10706fb0)
	if (bindFailed)
		return;

	switch (LinkState())
	{
	case STATE_Listening:
		CheckConnectionQueue();
		PollConnections();
		FlushSendBuffer();
		break;
	case STATE_Connecting:
		// The connect checked, then a read in the same tick, so what came
		// with the connection is raised at once, after Opened; one still
		// being made, or failed, reads nothing, as on Windows
		if (CheckConnectionAttempt())
			PollConnections();
		else if (ReceiveMode() == RMODE_Manual)
			DataPending() = 0;
		break;
	case STATE_Connected:
		PollConnections();
		FlushSendBuffer();
		break;
	case STATE_ListenClosePending:
	case STATE_ConnectClosePending:
		PollConnections();
		if (FlushSendBuffer() == 0)
			ShutdownConnection();
		break;
	case STATE_ListenClosing:
	case STATE_ConnectClosing:
	{
		// What the peer sends after the FIN read and let go, no event raised,
		// as the original's closing link takes it (LingerConsole: no
		// ReceivedText of the peer's bye), so its end is found as it comes:
		// a listening link's of the connection it accepted, if it holds one
		socket_t s = LinkState() == STATE_ListenClosing ? remote : handle;
		if (s != invalid_socket_value && Shown(s) && Readable(s))
		{
			char data[1000];
			if (recv(s, data, sizeof(data), MSG_PEEK) > 0)
				(void)recv(s, data, sizeof(data), 0);
		}
		break;
	}
	default:
		break;
	}

	// The other end going away.
	switch (LinkState())
	{
	case STATE_Listening:
	case STATE_ListenClosePending:
	case STATE_ListenClosing:
		CheckClosed(remote);
		break;
	case STATE_Connected:
	case STATE_ConnectClosePending:
	case STATE_ConnectClosing:
		CheckClosed(handle);
		break;
	default:
		break;
	}
}

void UTcpLink::CheckClosed(socket_t& s)
{
	if (s == invalid_socket_value || !Shown(s) || !Readable(s))
		return;
	char peek;
	int result = (int)recv(s, &peek, 1, MSG_PEEK);
	if (result > 0 || (result < 0 && WouldBlock(LastSocketError())))
		return;

	// The connection's socket closed, the link back to Initialized unless it
	// is listening, and Closed, as the original's: a listening link's own
	// socket is kept, listening, through its Close and the end of the
	// connection it accepted, the port held
	if (LinkState() != STATE_Listening)
		LinkState() = STATE_Initialized;
	CloseSocket(s);
	s = invalid_socket_value;
	Socket() = handle == invalid_socket_value ? -1 : (int)handle;
	RemoteSocket() = remote == invalid_socket_value ? -1 : (int)remote;
	CallEvent(this, "Closed");
}

void UTcpLink::CheckConnectionQueue()
{
	if (handle == invalid_socket_value || !Readable(handle))
		return;
	sockaddr_in peer = {};
#ifdef WIN32
	int size = sizeof(peer);
#else
	socklen_t size = sizeof(peer);
#endif
	socket_t s = accept(handle, (sockaddr*)&peer, &size);
	if (s == invalid_socket_value)
		return;
	SetNonBlocking(s);
	IpAddr addr = { (int32_t)ntohl(peer.sin_addr.s_addr), ntohs(peer.sin_port) };

	if (AcceptClass())
	{
		// A new link of the accept class takes the connection, in the
		// listener's link mode.
		UTcpLink* link = UObject::TryCast<UTcpLink>(Spawn(AcceptClass(), this, {}, Location(), Rotation()));
		if (!link)
		{
			CloseSocket(s);
			return;
		}
		link->handle = s;
		link->handleLive = true;
		link->Socket() = (int)s;
		link->LinkState() = STATE_Connected;
		link->LinkMode() = LinkMode();
		link->RemoteAddr() = addr;
		CallEvent(link, "Accepted");
	}
	else if (remote == invalid_socket_value)
	{
		remote = s;
		RemoteSocket() = (int)s;
		RemoteAddr() = addr;
		CallEvent(this, "Accepted");
	}
	else
	{
		CloseSocket(s);
	}
}

bool UTcpLink::CheckConnectionAttempt()
{
	if (connectFailed || handle == invalid_socket_value)
		return false;
	fd_set writable;
	FD_ZERO(&writable);
	FD_SET(handle, &writable);
	timeval timeout = {};
	if (select((int)handle + 1, nullptr, &writable, nullptr, &timeout) <= 0)
		return false;

	// A connection that failed stays waiting, as on the original's Windows,
	// where only a made one turns writable; the script's own timer gives up.
	int error = 0;
#ifdef WIN32
	int size = sizeof(error);
#else
	socklen_t size = sizeof(error);
#endif
	if (getsockopt(handle, SOL_SOCKET, SO_ERROR, (char*)&error, &size) != 0 || error != 0)
	{
		LastError = error;
		connectFailed = true;
		return false;
	}

	handleLive = true;
	LinkState() = STATE_Connected;
	CallEvent(this, "Opened");
	return true;
}

void UTcpLink::PollConnections()
{
	// Nothing pending on a socket Windows shows nothing of, nor on none
	socket_t s = DataSocket();
	if (s == invalid_socket_value || !Shown(s))
	{
		if (ReceiveMode() == RMODE_Manual)
			DataPending() = 0;
		return;
	}

	if (ReceiveMode() == RMODE_Manual)
	{
		DataPending() = Readable(s) ? 1 : 0;
		return;
	}

	// One read a tick, of up to 999 bytes, as the original's.
	char data[1000];
	int count = (int)recv(s, data, 999, 0);
	if (count < 0)
		return;
	data[count] = 0;

	if (LinkMode() == MODE_Text)
	{
		CallEvent(this, "ReceivedText", { ExpressionValue::StringValue(std::string(data)) });
	}
	else if (LinkMode() == MODE_Line)
	{
		CallEvent(this, "ReceivedLine", { ExpressionValue::StringValue(std::string(data)) });
	}
	else if (LinkMode() == MODE_Binary && count > 0)
	{
		UFunction* func = FindEventFunction(this, "ReceivedBinary");
		if (func && func->Properties.size() >= 2)
		{
			for (int start = 0; start < count; start += 255)
			{
				uint8_t bytes[255] = {};
				int n = std::min(count - start, 255);
				memcpy(bytes, data + start, n);
				CallEvent(this, "ReceivedBinary", { ExpressionValue::IntValue(n), ExpressionValue::At(bytes, func->Properties[1]) });
			}
		}
	}
}

int UTcpLink::FlushSendBuffer()
{
	socket_t s = DataSocket();
	if (s == invalid_socket_value || sendBuffer.empty())
		return (int)sendBuffer.size();
	// No SIGPIPE on a connection the peer has reset: the error comes back
	// instead, as on Windows, where nothing masks the signal (a headless
	// dedicated server, the X11 and Wayland windows).
#ifdef MSG_NOSIGNAL
	const int sendFlags = MSG_NOSIGNAL;
#else
	const int sendFlags = 0;
#endif
	int sent = (int)send(s, (const char*)sendBuffer.data(), (int)sendBuffer.size(), sendFlags);
	if (sent > 0)
		sendBuffer.erase(sendBuffer.begin(), sendBuffer.begin() + sent);
	else if (sent < 0 && !WouldBlock(LastSocketError()))
		LastError = LastSocketError();
	return (int)sendBuffer.size();
}

void UTcpLink::ShutdownConnection()
{
	// The sending side shut, a FIN, the link reading on until the peer's
	// end (the tick reads a closing link), then closed -- abortively -- with
	// Closed: as the original's under Wine, whose closing link takes the
	// peer's last words and closes as its end comes (LingerConsole). A
	// listening link shuts the connection it accepted, any other its own
	// socket (IpDrv.dll 0x10708040). One that holds no connection fails, as
	// Windows' shutdown fails it, where Linux's would end a connect under way
	// or pass a listening socket: logged, and the close pending still, tried
	// again each tick, as the original's
	socket_t s = LinkState() == STATE_ListenClosePending ? remote : handle;
	if (s == invalid_socket_value || !HasPeer(s) || shutdown(s, SHUT_WR) != 0)
	{
		LogMessage("Close: Error while attempting to close socket.");
		if (s == invalid_socket_value)
			LogMessage("Close: Tried to close an invalid socket.");
		return;
	}
	LinkState() = (LinkState() == STATE_ListenClosePending) ? STATE_ListenClosing : STATE_ConnectClosing;
}

int UTcpLink::BindPort(int port, bool bUseNextAvailable)
{
	// One bind a link: a second, or one after a failed bind, answers 0, as
	// the original's (ATcpLink::execBindPort, IpDrv.dll 0x10706960)
	if (bindFailed || handle != invalid_socket_value)
	{
		LogMessage("BindPort: already bound");
		return 0;
	}
	int bound = 0;
	if (EnsureSocket() && (bound = BindSocket(handle, port, bUseNextAvailable)) == 0)
	{
		LastError = LastSocketError();
		LogMessage("BindPort: bind failed");
		CloseSocket(handle);
		handle = invalid_socket_value;
	}
	if (bound == 0)
	{
		// The socket gone and Socket 0, as the original's leaves it: a link
		// that does nothing from then on, its Open, Listen and Close true
		// and its sends and reads 0
		Socket() = 0;
		bindFailed = true;
		return 0;
	}
	// The port asked for, or the next one taken: an ephemeral one leaves
	// Port 0, as the original's writes the port it gave the bind
	Port() = port != 0 ? bound : 0;
	LinkState() = STATE_Ready;
	return bound;
}

bool UTcpLink::Listen()
{
	// A backlog of one, and the send queue emptied, as the original's
	// (0x10706df0): from Ready alone, or again from ListenClosing, its
	// socket listening still; a link whose BindPort failed true and nothing
	// done
	if (bindFailed)
		return true;
	if (LinkState() != STATE_ListenClosing)
	{
		if (LinkState() != STATE_Ready)
			return false;
		if (listen(handle, 1) != 0)
		{
			LastError = LastSocketError();
			LogMessage("Listen: listen failed");
			return false;
		}
		handleLive = true;
	}
	LinkState() = STATE_Listening;
	sendBuffer.clear();
	return true;
}

bool UTcpLink::Open(const IpAddr& Addr)
{
	// The send queue emptied, as the original's (0x10707c90); a link whose
	// BindPort failed true and nothing done
	if (bindFailed)
		return true;
	// A link never bound has no socket to connect, as the original's
	// Socket -1 fails to
	if (handle == invalid_socket_value)
	{
		LogMessage("Open: An error occured while attempting to connect: no socket");
		return false;
	}
	// A new connect: the failure kept was the last one's (a script opens a
	// link again after closing it, UBrowser's HTTP client among them)
	connectFailed = false;
	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl((uint32_t)Addr.Addr);
	addr.sin_port = htons((uint16_t)Addr.Port);
	if (connect(handle, (const sockaddr*)&addr, sizeof(addr)) != 0 && !InProgress(LastSocketError()))
	{
		LastError = LastSocketError();
		LogMessage("Open: An error occured while attempting to connect: " + std::to_string(LastError));
		return false;
	}
	LinkState() = STATE_Connecting;
	sendBuffer.clear();
	return true;
}

bool UTcpLink::Close()
{
	// True, as the original's (ATcpLink::execClose, IpDrv.dll 0x10707fb0),
	// and nothing done on a link whose BindPort failed. A listening link
	// holding no connection of its own closes at once (ListenClosing), its
	// socket listening still and nothing raised, for Listen to take back;
	// one holding one once its queue is sent (ListenClosePending); any other
	// link once its queue is sent (ConnectClosePending), one not connected
	// keeping its socket, its shutdown failing each tick
	if (bindFailed)
		return true;
	if (LinkState() == STATE_Listening)
		LinkState() = remote == invalid_socket_value ? STATE_ListenClosing : STATE_ListenClosePending;
	else
		LinkState() = STATE_ConnectClosePending;
	return true;
}

bool UTcpLink::IsConnected()
{
	// Whether the socket takes a send now, in any state but Initialized, as
	// the original's asks (0x10706cb0): true while a close is pending, false
	// on a connection whose send buffer is full; a listening link asks of
	// the connection it accepted, then of its own socket
	if (bindFailed || LinkState() == STATE_Initialized)
		return false;
	if (LinkState() == STATE_Listening && Writable(remote))
		return true;
	return Writable(handle);
}

int UTcpLink::ReadBinary(int Count, uint8_t* B)
{
	// Read only while listening or connected, as the original's (0x10708b40)
	socket_t s = DataSocket();
	if (s == invalid_socket_value || (LinkState() != STATE_Listening && LinkState() != STATE_Connected))
		return 0;
	int count = (int)recv(s, (char*)B, std::clamp(Count, 0, 255), 0);
	return count > 0 ? count : 0;
}

int UTcpLink::SendBinary(int Count, const uint8_t* B)
{
	// Nothing queued on a link whose BindPort failed (0x10708670)
	if (bindFailed)
		return 0;
	Count = std::clamp(Count, 0, 255);
	sendBuffer.insert(sendBuffer.end(), B, B + Count);
	FlushSendBuffer();
	return Count;
}

int UTcpLink::ReadText(std::string& Str)
{
	// 255 bytes at most, and only while listening or connected, as the
	// original's (0x107081b0); -1 where nothing was read, the script's
	// string left as it was
	socket_t s = DataSocket();
	if (s == invalid_socket_value || (LinkState() != STATE_Listening && LinkState() != STATE_Connected))
		return -1;
	char data[256];
	int count = (int)recv(s, data, 255, 0);
	if (count < 0)
		return -1;
	data[count] = 0;
	Str = data;
	return count;
}

int UTcpLink::SendText(const std::string& Str)
{
	// Queued whole, a CR LF after it in line mode, and sent as the
	// connection takes it; the script is told all of it went, the line
	// end's two bytes too, as the original's answers (ATcpLink::
	// execSendText, IpDrv.dll 0x10708870). A link with no socket yet, or
	// whose connection went, queues it alike: the original's answers 0 only
	// where its Socket is 0, which a failed BindPort leaves.
	if (bindFailed)
		return 0;
	sendBuffer.insert(sendBuffer.end(), Str.begin(), Str.end());
	size_t length = Str.size();
	if (LinkMode() == MODE_Line)
	{
		sendBuffer.push_back('\r');
		sendBuffer.push_back('\n');
		length += 2;
	}
	FlushSendBuffer();
	return (int)length;
}
