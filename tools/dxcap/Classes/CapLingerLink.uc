//=============================================================================
// CapLingerLink: LingerConsole's links (bugs.md NET.23) -- a TcpLink of the
// package's own, bound and opened to tools/dxcap/lingerpeer.py on
// 127.0.0.1:7879, sending its line once opened. It logs with "DXLINGER:"
// and its name each native's answer and each event -- Opened, ReceivedText,
// Closed, its own destruction --, the last three with the time since it
// opened.
//=============================================================================
class CapLingerLink extends TcpLink;

var string Who, Line;
var bool bOpened;
var float OpenedAt;

function string Since()
{
	return " " $ (Level.TimeSeconds - OpenedAt) $ " s after opening";
}

function Start(string Name, string L)
{
	local IpAddr Addr;

	Who = Name;
	Line = L;
	Log("DXLINGER: " $ Who $ " bound " $ (BindPort() > 0));
	StringToIpAddr("127.0.0.1", Addr);
	Addr.Port = 7879;
	Log("DXLINGER: " $ Who $ " Open " $ Open(Addr));
}

event Opened()
{
	Log("DXLINGER: " $ Who $ " opened, sent " $ SendText(Line));
	bOpened = true;
	OpenedAt = Level.TimeSeconds;
}

event ReceivedText(string T)
{
	Log("DXLINGER: " $ Who $ " received " $ Len(T) $ ", '" $ T $ "'," $ Since());
}

event Closed()
{
	Log("DXLINGER: " $ Who $ " closed," $ Since());
}

event Destroyed()
{
	Log("DXLINGER: " $ Who $ " destroyed, state " $ LinkState $ "," $ Since());
	Super.Destroyed();
}

defaultproperties
{
	LinkMode=MODE_Text
	ReceiveMode=RMODE_Event
}
