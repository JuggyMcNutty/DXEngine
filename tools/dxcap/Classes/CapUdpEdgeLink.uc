//=============================================================================
// CapUdpEdgeLink: a UdpLink of the package's own for UdpEdgesConsole.
// bugs.md NET.31, NET.37. bInCall is set by the console round each of its
// Resolve calls, so that an answer raised within the call is told from one
// raised by a later tick; each answer and each text received as an event is
// logged with "DXUDPEDGE:", the text by its length. The console's sends to
// the link's own port and its reads by hand are here, as a console can name
// no IpAddr (UCC's "Unrecognized type").
//=============================================================================
class CapUdpEdgeLink extends UdpLink;

var string Label;
var bool bInCall;
var int Ticks;
var IpAddr ToSelf;

function AimAtSelf(int Bound)
{
	StringToIpAddr("127.0.0.1", ToSelf);
	ToSelf.Port = Bound;
}

function bool SendBytes(int Count)
{
	local byte B[255];
	local int i;

	for (i = 0; i < Count; i++)
		B[i] = byte(i + 1);
	return SendBinary(ToSelf, Count, B);
}

function bool SendToSelf(string Text)
{
	return SendText(ToSelf, Text);
}

function ReadBin(int Count)
{
	local IpAddr From;
	local byte B[255];
	local int N;

	N = ReadBinary(From, Count, B);
	Log("DXUDPEDGE: ReadBinary of " $ Count $ ": " $ N $ ", first byte " $ B[0] $ ", GetLastError " $ GetLastError());
}

function ReadTxt()
{
	local IpAddr From;
	local string S;
	local int N;

	S = "keep";
	N = ReadText(From, S);
	Log("DXUDPEDGE: ReadText: " $ N $ ", the string's length " $ Len(S) $ " ('" $ Left(S, 5) $ "'), GetLastError " $ GetLastError());
}

function ToEvents()
{
	ReceiveMode = RMODE_Event;
}

// By hand from its spawn: the link's C++ constructor sets receiving by
// events over the class's defaults (IpDrv.dll 0x10702480), in both engines
function ToManual()
{
	ReceiveMode = RMODE_Manual;
}

function string When()
{
	if (bInCall)
		return "within the call";
	return "at tick " $ Ticks;
}

event Resolved(IpAddr Addr)
{
	Log("DXUDPEDGE: " $ Label $ " resolved " $ IpAddrToString(Addr) $ " " $ When());
}

event ResolveFailed()
{
	Log("DXUDPEDGE: " $ Label $ " resolve failed " $ When());
}

event ReceivedText(IpAddr Addr, string Text)
{
	Log("DXUDPEDGE: " $ Label $ " event text of " $ Len(Text) $ " from port " $ Addr.Port);
}

defaultproperties
{
	LinkMode=MODE_Text
}
