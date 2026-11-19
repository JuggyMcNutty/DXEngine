//=============================================================================
// HighByteConsole (bugs.md NET.46): what a TcpLink's ReceivedText
// makes of bytes C3 A9 78, which read as UTF-8 are one character (233) and
// a byte a character three (195 169 120). The original's tick raises the
// text through appFromAnsi, a byte a character (ipdrv-dll.md, the script's
// links), as DXEngine's ReadText and SendText take them (NTcpLink.cpp);
// DXEngine's tick decodes UTF-8 (UTcpLink.cpp PollConnections,
// ExpressionValue::StringValue(std::string)).
// Expect: the original "length 3, codes 195 169 120"; DXEngine today
// "length 2, codes 233 120". Exits at 6 s. Any map.
//=============================================================================
class HighByteConsole extends Console;

var float RunTime;
var int Step;
var CapHighByteLink L, A;

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
		Step = 1;
	}
	else if (RunTime > 6.0)
	{
		Log("DXHIGH: exiting, A got text " $ A.bGot);
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
