//=============================================================================
// StartConsole: a run started on a save -- DXEngine's ?loadgame=9 for its
// map, the original's DXCAP_START=?loadgame=9 --, which the console itself
// loads nothing of: 5 s into the loaded level, the island or SaveConsole's
// Via, what it holds is logged as SaveConsole logs it; then an exit. With
// no such level after 60 s it says so and exits.
//=============================================================================
class StartConsole extends SaveConsole;

var float RunTime;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super(Console).Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;

	if (Step == 0 && (CurrentMap ~= "01_NYC_UNATCOIsland" || (Via != "" && CurrentMap ~= Via)) && MapTime > 5.0)
	{
		Census(P, "loaded");
		Log("DXSAVE: exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
	else if (Step == 0 && RunTime > 60.0)
	{
		Log("DXSAVE: nothing loaded after 60 s, in " $ MapOf(P) $ "; exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
}
