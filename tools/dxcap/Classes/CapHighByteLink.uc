//=============================================================================
// CapHighByteLink (bugs.md NET.46): HighByteConsole's TcpLinks. A
// greeting link answers its accepted connection with the text Chr(195)
// Chr(169) "x" -- bytes C3 A9 78 on the wire (SendText narrows each
// character to a byte). The other logs, with "DXHIGH:", every
// ReceivedText's length and each character's Asc.
//=============================================================================
class CapHighByteLink extends TcpLink;

var string Who;
var bool bGreet, bGot;

function bool OpenLocal(int P)
{
	local IpAddr Addr;

	StringToIpAddr("127.0.0.1", Addr);
	Addr.Port = P;
	return Open(Addr);
}

event Accepted()
{
	if (bGreet)
		Log("DXHIGH: " $ Who $ " sent " $ SendText(Chr(195) $ Chr(169) $ "x"));
}

event ReceivedText(string T)
{
	local int i;
	local string codes;

	if (Len(T) == 0)
		return;
	for (i = 0; i < Len(T); i++)
		codes = codes $ " " $ Asc(Mid(T, i, 1));
	Log("DXHIGH: " $ Who $ " ReceivedText length " $ Len(T) $ ", codes" $ codes);
	bGot = true;
}

defaultproperties
{
	LinkMode=MODE_Text
	ReceiveMode=RMODE_Event
}
