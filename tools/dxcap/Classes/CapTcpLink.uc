//=============================================================================
// CapTcpLink: TcpLinkConsole's links -- a TcpLink of the package's own whose
// steps log what each native answers, and whose events log what they bring:
// Opened, Accepted, Closed, and each text received by its length and first
// characters. Each line starts "DXTCP:" and the link's name.
//=============================================================================
class CapTcpLink extends TcpLink;

var string Who, Text;

function string SocketText()
{
	if (Socket == 0 || Socket == -1)
		return string(Socket);
	return "a socket";
}

// A BindPort that fails -- port 1, which no user may bind -- then every
// native after it
function DeadSteps()
{
	local IpAddr Addr;
	local string S;
	local byte B[255];

	Log("DXTCP: " $ Who $ " BindPort(1) " $ BindPort(1) $ ", Socket " $ SocketText());
	Log("DXTCP: " $ Who $ " BindPort() " $ BindPort());
	Log("DXTCP: " $ Who $ " Listen " $ Listen() $ ", state " $ LinkState);
	StringToIpAddr("127.0.0.1", Addr);
	Addr.Port = 7881;
	Log("DXTCP: " $ Who $ " Open " $ Open(Addr) $ ", state " $ LinkState);
	Log("DXTCP: " $ Who $ " SendText " $ SendText("dead"));
	B[0] = 65;
	Log("DXTCP: " $ Who $ " SendBinary " $ SendBinary(1, B));
	S = "kept";
	Log("DXTCP: " $ Who $ " ReadText " $ ReadText(S) $ ", '" $ S $ "'");
	Log("DXTCP: " $ Who $ " ReadBinary " $ ReadBinary(1, B));
	Log("DXTCP: " $ Who $ " IsConnected " $ IsConnected());
	Log("DXTCP: " $ Who $ " Close " $ Close() $ ", state " $ LinkState $ ", Socket " $ SocketText());
}

// An Open with no BindPort before it, to a port no one listens on
function NeverBound()
{
	local IpAddr Addr;

	StringToIpAddr("127.0.0.1", Addr);
	Addr.Port = 7881;
	Log("DXTCP: " $ Who $ " Open, never bound, " $ Open(Addr) $ ", state " $ LinkState);
}

// Bound from 7880, a line queued, then listening, read by hand; the port
function int StartListener()
{
	local int Port;

	ReceiveMode = RMODE_Manual;
	Port = BindPort(7880, true);
	Log("DXTCP: " $ Who $ " bound " $ (Port > 0) $ ", queued " $ SendText("lost"));
	Log("DXTCP: " $ Who $ " Listen " $ Listen() $ ", state " $ LinkState);
	return Port;
}

// Bound, a line queued, then opened to the listener; opened, it sends 600
// characters
function StartOpener(int Port)
{
	local IpAddr Addr;
	local int i;

	for (i = 0; i < 600; i++)
		Text = Text $ "a";
	Log("DXTCP: " $ Who $ " bound " $ (BindPort() > 0) $ ", queued " $ SendText("early"));
	StringToIpAddr("127.0.0.1", Addr);
	Addr.Port = Port;
	Log("DXTCP: " $ Who $ " Open " $ Open(Addr));
}

// A read by hand: what it gives, when it gives something
function ReadByHand()
{
	local string S;
	local int n;

	n = ReadText(S);
	if (n > 0)
		Log("DXTCP: " $ Who $ " read " $ n $ ", '" $ Left(S, 5) $ "' first");
}

event Opened()
{
	Log("DXTCP: " $ Who $ " opened");
	if (Text != "")
		Log("DXTCP: " $ Who $ " sent " $ SendText(Text));
}

event Accepted()
{
	Log("DXTCP: " $ Who $ " accepted");
}

event Closed()
{
	Log("DXTCP: " $ Who $ " closed");
}

event ReceivedText(string T)
{
	Log("DXTCP: " $ Who $ " received " $ Len(T) $ ", '" $ Left(T, 5) $ "' first");
}

defaultproperties
{
	LinkMode=MODE_Text
	ReceiveMode=RMODE_Event
}
