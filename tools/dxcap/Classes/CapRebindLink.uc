//=============================================================================
// CapRebindLink: a UdpLink of the package's own for UdpRebindConsole. bugs.md
// NET.30. Its text received as an event and logged with the
// sender's port, with "DXREBIND:"; SendTo sends text to a port on 127.0.0.1
// and answers what SendText answered.
//=============================================================================
class CapRebindLink extends UdpLink;

var string Label;

function bool SendTo(int ToPort, string Text)
{
	local IpAddr A;

	StringToIpAddr("127.0.0.1", A);
	A.Port = ToPort;
	return SendText(A, Text);
}

event ReceivedText(IpAddr Addr, string Text)
{
	Log("DXREBIND: " $ Label $ " received '" $ Text $ "' from port " $ Addr.Port);
}

defaultproperties
{
	LinkMode=MODE_Text
	ReceiveMode=RMODE_Event
}
