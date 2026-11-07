//=============================================================================
// CapRstLink: RstConsole's link -- a TcpLink, bound first as an Open needs,
// to 127.0.0.1:7877, a peer that resets the connection a second after it
// opens; a line sent as it opens and two more, a second apart, after the
// reset, each send's answer and the link's error logged ("DXRST:").
//=============================================================================
class CapRstLink extends TcpLink;

var int Sends;

function Start()
{
	local IpAddr Addr;

	BindPort();
	StringToIpAddr("127.0.0.1", Addr);
	Addr.Port = 7877;
	Log("DXRST: opening " $ IpAddrToString(Addr) $ ": " $ Open(Addr));
}

event Opened()
{
	Log("DXRST: opened, sent " $ SendText("hello" $ Chr(10)) $ " bytes");
	SetTimer(2.0, true);
}

event Closed()
{
	Log("DXRST: closed, error " $ GetLastError());
}

event Timer()
{
	Sends++;
	Log("DXRST: send " $ Sends $ " after the reset: " $ SendText("after the reset" $ Chr(10)) $ " bytes, error " $ GetLastError() $ ", connected " $ IsConnected());
	if (Sends >= 2)
		SetTimer(0, false);
}
