//=============================================================================
// CapEdgeLink: LinkEdgesConsole's TcpLinks. Each counts its own ticks (the
// script's Tick runs before the native one reads and raises the link's
// events, in both engines), and logs with "DXEDGE:" and its name each event
// with that count and its state: Accepted with the peer's port as the link
// holds it, Opened, ReceivedText, Closed. A greeting link answers its
// accepted connection with "hello" at once.
//=============================================================================
class CapEdgeLink extends TcpLink;

var string Who;
var int Ticks, OpenedTick, TextTick;
var bool bGreet, bOpened, bGotText;

event Tick(float Delta)
{
	Ticks++;
}

function string At()
{
	return " at tick " $ Ticks $ ", state " $ LinkState;
}

// Open to a port of this machine (the console names no IpAddr, a struct of
// another package)
function bool OpenLocal(int P)
{
	local IpAddr Addr;

	StringToIpAddr("127.0.0.1", Addr);
	Addr.Port = P;
	return Open(Addr);
}

event Accepted()
{
	Log("DXEDGE: " $ Who $ " accepted the port " $ RemoteAddr.Port $ At());
	if (bGreet)
		Log("DXEDGE: " $ Who $ " greets, sent " $ SendText("hello"));
}

event Opened()
{
	bOpened = true;
	OpenedTick = Ticks;
	Log("DXEDGE: " $ Who $ " opened" $ At() $ ", connected " $ IsConnected());
}

event ReceivedText(string T)
{
	if (!bGotText)
	{
		bGotText = true;
		TextTick = Ticks;
	}
	Log("DXEDGE: " $ Who $ " received '" $ T $ "'" $ At());
}

event Closed()
{
	Log("DXEDGE: " $ Who $ " closed" $ At());
}

defaultproperties
{
	LinkMode=MODE_Text
	ReceiveMode=RMODE_Event
}
