//=============================================================================
// IniShrinkConsole: a config dynamic array grown, then cut short, and the
// system ini written back each time, logged with "DXINISHRINK:"
// (docs/DEVELOPMENT.md, scripted runs; bugs.md ENG.49).
//
// From wherever the run starts, 2 s in: the game engine's ServerActors set
// to three, which saves the class's config into the run's system ini, and a
// second later to one; its GET logged before and after each; then an exit.
// The original's SaveConfig removes the section's lines of a dynamic
// array's name and writes one an element (Core.dll 0x10151540), so the
// run's ini holds one ServerActors line after it.
//=============================================================================
class IniShrinkConsole extends Console;

var float RunTime;
var int Phase;

function SetActors(string List)
{
	Log("DXINISHRINK: before " $ Viewport.Actor.ConsoleCommand("get Engine.GameEngine ServerActors"));
	Viewport.Actor.ConsoleCommand("set Engine.GameEngine ServerActors " $ List);
	Log("DXINISHRINK: after " $ Viewport.Actor.ConsoleCommand("get Engine.GameEngine ServerActors"));
}

event Tick(float Delta)
{
	local string Q;

	Super.Tick(Delta);
	if (Phase == 3 || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	Q = Chr(34);
	if (Phase == 0 && RunTime > 2.0)
	{
		SetActors("(" $ Q $ "IpDrv.UdpBeacon" $ Q $ "," $ Q $ "IpServer.UdpServerQuery" $ Q $ "," $ Q $ "IpServer.UdpServerUplink" $ Q $ ")");
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		SetActors("(" $ Q $ "IpDrv.UdpBeacon" $ Q $ ")");
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 4.0)
	{
		Log("DXINISHRINK: done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 3;
	}
}
