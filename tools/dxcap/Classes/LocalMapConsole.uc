//=============================================================================
// LocalMapConsole: the map the game starts on (bugs.md ENG.22), logged with
// "DXLOCALMAP:" at the console's first tick with a player, then an exit 2 s
// later. Run each engine with no start URL -- the original from its own
// start, DXEngine's dxe with the map - -- and DXCAP_INI naming another map
// as the ini's [URL] LocalMap ("URL:LocalMap=01_NYC_UNATCOIsland.dx"): the
// original's start is DX.dx whatever the ini names, its Init setting the
// default local map to DX.dx before it reads its command line (Engine.dll
// 0x10389429-0x103894a4).
//=============================================================================
class LocalMapConsole extends Console;

var float RunTime;
var bool bLogged, bDone;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || bDone)
		return;
	P = Viewport.Actor;
	if (!bLogged)
	{
		Log("DXLOCALMAP: started on " $ P.Level.GetLocalURL() $ ", title " $ P.Level.Title);
		bLogged = true;
	}
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	Log("DXLOCALMAP: done");
	bDone = true;
	P.ConsoleCommand("exit");
}
