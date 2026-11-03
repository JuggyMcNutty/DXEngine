//=============================================================================
// CapUdpEcho: a UdpLink of the package's own for LinkConsole, its text
// received as an event and logged by its characters' codes, the log being
// 8-bit: what a link sends of characters past 0xFF.
//=============================================================================
class CapUdpEcho extends UdpLink;

// The text sent to its own port on 127.0.0.1
function SendSelf(string Text)
{
	local IpAddr A;
	local int port;

	port = BindPort(2200, true);
	StringToIpAddr("127.0.0.1", A);
	A.Port = port;
	Log("DXLINK: echo bound " $ (port > 0) $ ", sent " $ SendText(A, Text));
}

event ReceivedText(IpAddr Addr, string Text)
{
	local string R;
	local int i;

	for (i = 0; i < Len(Text); i++)
		R = R $ " " $ Asc(Mid(Text, i, 1));
	Log("DXLINK: received" $ R);
}

defaultproperties
{
	LinkMode=MODE_Text
	ReceiveMode=RMODE_Event
}
