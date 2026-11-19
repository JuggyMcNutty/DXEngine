//=============================================================================
// HighByteConsole (bugs.md NET.46): what a TcpLink's ReceivedText
// makes of bytes C3 A9 78, which read as UTF-8 are one character (233) and
// a byte a character three (195 169 120), and a UdpLink's (CapUdpEcho, the
// same bytes sent to itself). The original's tick raises the text a byte a
// character (appFromAnsi: ipdrv-dll.md, the script's links), as both
// engines' ReadText and SendText take it.
// Expect, in both engines: "A ReceivedText length 3, codes 195 169 120"
// and "DXLINK: received 195 169 120". Exits at 6 s. Any map.
//=============================================================================
class HighByteConsole extends Console;

var float RunTime;
var int Step;
var CapHighByteLink L, A;
var CapUdpEcho U;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Step == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (Step == 0 && RunTime > 2.0)
	{
		L = P.Spawn(class'CapHighByteLink');
		L.Who = "L";
		L.bGreet = true;
		Log("DXHIGH: L BindPort " $ L.BindPort(7896) $ ", Listen " $ L.Listen());
		A = P.Spawn(class'CapHighByteLink');
		A.Who = "A";
		Log("DXHIGH: A BindPort " $ A.BindPort(7897) $ ", Open " $ A.OpenLocal(7896));
		U = P.Spawn(class'CapUdpEcho');
		U.SendSelf(Chr(195) $ Chr(169) $ "x");
		Step = 1;
	}
	else if (RunTime > 6.0)
	{
		Log("DXHIGH: exiting, A got text " $ A.bGot);
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
