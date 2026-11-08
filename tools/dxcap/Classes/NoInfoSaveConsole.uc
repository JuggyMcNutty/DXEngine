//=============================================================================
// NoInfoSaveConsole: a save on a level with no DeusExLevelInfo, and the
// name a level is saved under, logged with "DXNOINFO:".
//
// The original's SaveGame looks for the first DeusExLevelInfo among all
// objects and, with none, logs "WARNING!  DeusExLevelInfo MISSING!" and
// returns: no folder made, nothing copied, no SAVING frame (DeusEx.dll
// 0x1000f02d-0x1000f04d). DXEngine's SaveGameToSlot saves all the same.
// The original names the saved level's file, and the SaveInfo's MapName,
// by that DeusExLevelInfo's MapName (SaveCurrentLevel 0x1000fe35, SaveGame
// 0x1000f283); DXEngine by the level's package (Engine.cpp SaveCurrentLevel,
// SaveGameToSlot). 99_Endgame1's MapName is Endgame1; 99_Endgame4, the
// credits' DANCEPARTY egg (CreditsWindow: OPEN 99_ENDGAME4), has no
// DeusExLevelInfo.
//
// Before the run: no Save0009, no Current. From the menu map:
// 1. 3 s in, `open 99_Endgame1`.
// 2. 4 s in, `open 99_Endgame4`, within mission 99: the original's
//    Current\Endgame1.dxs, DXEngine's Current\99_Endgame1.dxs.
// 3. 4 s into 99_Endgame4, its DeusExLevelInfos counted, then slot 9 saved
//    (SaveHelper) -- a SAVING frame in DXEngine alone.
// 4. 3 s later GetSaveInfo(9): None in the original, DXEngine's with the
//    map 99_Endgame4; then an exit. After the run: Save\Current and
//    Save\Save0009 listed; remove both.
//=============================================================================
class NoInfoSaveConsole extends SaveConsole;

var float StepTime;
var bool bNewLevel;

event NotifyLevelChange()
{
	Super.NotifyLevelChange();
	bNewLevel = true;
}

event PostRender(canvas C)
{
	Super.PostRender(C);
	if (Viewport != None && Viewport.Actor != None && Viewport.Actor.Level.LevelAction != LEVACT_None)
		Log("DXNOINFO: frame drawn with level action " $ Viewport.Actor.Level.LevelAction $ " in " $ MapOf(Viewport.Actor));
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExLevelInfo Info;
	local GameDirectory Dir;
	local DeusExSaveInfo SaveInfo;
	local SaveHelper S;
	local int n;

	Super(Console).Tick(Delta);
	if (Viewport == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = Viewport.Actor;
	if (P.Level.LevelAction != LEVACT_None)
		return;
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	if (bNewLevel)
	{
		bNewLevel = false;
		CurrentMap = MapOf(P);
		MapTime = 0;
		StepTime = 0;
		Log("DXNOINFO: now in " $ CurrentMap);
	}

	if (Step == 0 && MapTime > 3.0)
	{
		Log("DXNOINFO: opening 99_Endgame1");
		P.ConsoleCommand("open 99_Endgame1");
		Step = 1;
	}
	else if (Step == 1 && CurrentMap ~= "99_Endgame1" && StepTime > 4.0)
	{
		Log("DXNOINFO: opening 99_Endgame4, within mission 99");
		P.ConsoleCommand("open 99_Endgame4");
		Step = 2;
	}
	else if (Step == 2 && CurrentMap ~= "99_Endgame4" && StepTime > 4.0)
	{
		foreach P.AllActors(class'DeusExLevelInfo', Info)
			n++;
		Log("DXNOINFO: " $ n $ " DeusExLevelInfo in the level; saving to slot 9");
		S = P.Spawn(class'SaveHelper', P);
		S.Slot = 9;
		S.Description = "DXCapture no info";
		Step = 3;
		StepTime = 0;
	}
	else if (Step == 3 && StepTime > 3.0)
	{
		Dir = DeusExPlayer(P).CreateGameDirectoryObject();
		Dir.SetDirType(Dir.EGameDirectoryTypes.GD_SaveGames);
		Dir.GetGameDirectory();
		SaveInfo = Dir.GetSaveInfo(9);
		if (SaveInfo == None)
			Log("DXNOINFO: GetSaveInfo(9) None");
		else
			Log("DXNOINFO: GetSaveInfo(9) map " $ SaveInfo.MapName $ ", location '" $ SaveInfo.MissionLocation $ "'");
		CriticalDelete(Dir);
		Log("DXNOINFO: done; exiting");
		P.ConsoleCommand("exit");
		Step = 4;
	}
}
