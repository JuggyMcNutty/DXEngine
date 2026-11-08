//=============================================================================
// LocalMapConsole: the map the game starts on (bugs.md ENG.22, ENG.26),
// logged with "DXLOCALMAP:" at the console's first tick with a player, then
// an exit 2 s later. Run each engine with no start URL -- the original from
// its own start, DXEngine's dxe with the map - -- and DXCAP_INI naming
// another map as the ini's [URL] LocalMap
// ("URL:LocalMap=01_NYC_UNATCOIsland.dx"): the original's start is DX.dx
// whatever the ini names, its Init setting the default local map to DX.dx
// before it reads its command line (Engine.dll 0x10389429-0x103894a4). Or
// with a start URL whose browse fails -- a map that is not there
// (NoSuchMap.dx), a slot with no save (?loadgame=9990), a server's address
// no server answers (127.0.0.1:7799): DXEngine's dxe map, the original's
// DXCAP_START. The original's Deus Ex browse loads Dx.Dx on the URL for a
// map not there, having no level whose map to load again (DeusEx.dll
// 0x10010b90), and its Init browses DX.dx once more after a browse that
// fails with no error's text, the slot's and the address's (Engine.dll
// 0x103895f0-0x10389663). Or with a start in a package that does not load
// (bugs.md ENG.28), each written by tools/dxcap/badpackage.py: a map whose
// tag is zeroed, in DXCAP_MAPS's folder (BadMap.dx), and slot 9 with its
// level's tag zeroed (?loadgame=9, SaveConsole's save): the map load's
// check fails each (LOAD_Verify, Engine.dll 0x1038c1f0), so the map's
// browse loads Dx.Dx and the slot's start browses DX.dx. With --cut, the
// file cut short in its header: fatal in both, the original's reader
// stopping at "ReadFile beyond EOF".
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
