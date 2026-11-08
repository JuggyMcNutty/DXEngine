//=============================================================================
// RestartNameConsole: the console's RestartLevel on a map whose
// DeusExLevelInfo's MapName names no map, logged with "DXRESTARTNAME:".
//
// ?restart loads the DeusExLevelInfo's MapName with ?loadonly (Deus Ex's
// Browse, DeusEx.dll 0x10010b90, its restart branch, then the common load
// at LABEL_71: LoadMap, the engine's virtual +172, whose package check fails
// a map that is not there, Engine.dll 0x1038c6ab/0x1038e62c: LOADING,
// "Failed launching URL" over the error for 6 s, the level playing on,
// Current emptied first). 00_Intro's DeusExLevelInfo1 names "Intro";
// 99_Endgame1-3 name "Endgame1"-"Endgame3": none is a map. Both engines
// alike (bugs.md PKG.30).
//
// From the menu map: 3 s in, `open 00_Intro`; 3 s into the intro the
// console's RestartLevel; 4 s later the map and the player's progress
// lines; then an exit. The log: the restart's LoadMap of Intro and its
// failure, the intro playing on.
//=============================================================================
class RestartNameConsole extends SaveConsole;

var float StepTime;
var bool bNewLevel;

event NotifyLevelChange()
{
	Super.NotifyLevelChange();
	bNewLevel = true;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExLevelInfo Info;

	Super(Console).Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (P.Level.LevelAction != LEVACT_None)
	{
		Log("DXRESTARTNAME: ticked with level action " $ P.Level.LevelAction $ " in " $ MapOf(P));
		return;
	}
	MapTime += Delta;
	StepTime += Delta;
	if (bNewLevel)
	{
		bNewLevel = false;
		CurrentMap = MapOf(P);
		StepTime = 0;
		Log("DXRESTARTNAME: now in " $ CurrentMap);
	}
	if (Step == 0 && MapTime > 3.0)
	{
		Log("DXRESTARTNAME: opening 00_Intro");
		P.ConsoleCommand("open 00_Intro");
		Step = 1;
	}
	else if (Step == 1 && CurrentMap ~= "00_Intro" && StepTime > 3.0)
	{
		foreach P.AllActors(class'DeusExLevelInfo', Info)
			Log("DXRESTARTNAME: " $ Info.Name $ " MapName '" $ Info.MapName $ "'");
		Log("DXRESTARTNAME: RestartLevel");
		P.ConsoleCommand("RestartLevel");
		Step = 2;
		StepTime = 0;
	}
	else if (Step == 2 && StepTime > 4.0)
	{
		Log("DXRESTARTNAME: on " $ MapOf(P) $ ", progress '" $ P.ProgressMessage[0] $ "' '" $ P.ProgressMessage[1] $ "'");
		Log("DXRESTARTNAME: done; exiting");
		P.ConsoleCommand("exit");
		Step = 3;
	}
}
