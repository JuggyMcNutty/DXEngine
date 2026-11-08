//=============================================================================
// FailLoadingConsole: the frames of a travel in play whose map is not there,
// logged with "DXFAILLOAD:" (its frames with a level action as
// LoadingConsole's PostRender logs them, "DXLOADING:").
//
// The original's map load paints LOADING before its package check -- the
// level's LevelAction 1, its Pauser emptied, the player's bShowMenu cleared,
// PaintProgress, the sounds stopped -- and only then fails the load
// (UGameEngine::LoadMap, Engine.dll 0x1038c1f0, the check at 0x1038c6ab, the
// catch at 0x1038e62c); Deus Ex's Browse then, outside any mission, loads the
// level's own map again, a second LOADING (DeusEx.dll 0x10010b90); and a
// URL's player keys are saved only after a load that succeeds (LoadMap's
// SaveURLConfig, 0x1038e33d). Both engines alike (bugs.md PKG.33).
//
// From the menu map:
// 1. 3 s in, 01_NYC_UNATCOIsland opened.
// 2. 6 s into the island, `open 01_DXCapNoSuchMap?Name=DXCapBob`, within
//    mission 1: SAVING, then LOADING. 4 s later the player's progress
//    lines, the pauser and the default URL's Name, still Player.
// 3. Paused (SetPause), 1 s later `open 01_DXCapNoSuchMap2`: 3 s later the
//    pauser '', the LOADING frame having emptied it.
// 4. Unpaused, `open DXCapNoSuchMap`, outside any mission: LOADING twice,
//    the failed load, then the island again.
// 5. 4 s into the island again, the lines once more, then an exit.
//=============================================================================
class FailLoadingConsole extends LoadingConsole;

var float StepTime;

function Report(PlayerPawn P, string When)
{
	local DeusExPlayer D;

	D = DeusExPlayer(P);
	Log("DXFAILLOAD: " $ When $ ": on " $ MapOf(P) $ ", progress '" $ P.ProgressMessage[0] $ "' '"
		$ P.ProgressMessage[1] $ "', pauser '" $ P.Level.Pauser $ "', bShowMenu " $ P.bShowMenu
		$ ", default Name '" $ P.GetDefaultURL("Name") $ "'");
	if (D != None)
		Log("DXFAILLOAD: " $ When $ ": augs " $ (D.AugmentationSystem != None) $ " skills "
			$ (D.SkillSystem != None) $ " flags " $ (D.flagBase != None));
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super(Console).Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (P.Level.LevelAction != LEVACT_None)
	{
		Log("DXFAILLOAD: ticked with " $ ActionName(P.Level.LevelAction) $ " in " $ MapOf(P)
			$ ", pauser '" $ P.Level.Pauser $ "'");
		return;
	}
	RunTime += Delta;
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	// No shots: the frames' lines are what is compared
	bShotDue = false;
	if (bNewLevel)
	{
		bNewLevel = false;
		CurrentMap = MapOf(P);
		MapTime = 0;
		StepTime = 0;
		Log("DXFAILLOAD: now in " $ CurrentMap $ " at " $ RunTime);
	}

	if (Step == 0 && MapTime > 3.0)
	{
		Log("DXFAILLOAD: opening 01_NYC_UNATCOIsland");
		P.ConsoleCommand("open 01_NYC_UNATCOIsland");
		Step = 1;
	}
	else if (Step == 1 && CurrentMap ~= "01_NYC_UNATCOIsland" && StepTime > 6.0)
	{
		Report(P, "before");
		Log("DXFAILLOAD: opening 01_DXCapNoSuchMap?Name=DXCapBob, within mission 1");
		P.ConsoleCommand("open 01_DXCapNoSuchMap?Name=DXCapBob");
		Step = 2;
		StepTime = 0;
	}
	else if (Step == 2 && StepTime > 4.0)
	{
		Report(P, "after the travel within the mission");
		P.SetPause(True);
		Log("DXFAILLOAD: paused, pauser '" $ P.Level.Pauser $ "'");
		Step = 3;
		StepTime = 0;
	}
	else if (Step == 3 && StepTime > 1.0)
	{
		Log("DXFAILLOAD: opening 01_DXCapNoSuchMap2 while paused");
		P.ConsoleCommand("open 01_DXCapNoSuchMap2");
		Step = 4;
		StepTime = 0;
	}
	else if (Step == 4 && StepTime > 3.0)
	{
		Report(P, "after the paused travel");
		if (P.Level.Pauser != "")
			P.SetPause(False);
		Log("DXFAILLOAD: opening DXCapNoSuchMap, outside any mission");
		P.ConsoleCommand("open DXCapNoSuchMap");
		Step = 5;
		StepTime = 0;
	}
	else if (Step == 5 && CurrentMap ~= "01_NYC_UNATCOIsland" && StepTime > 4.0)
	{
		Report(P, "after the travel outside any mission");
		Log("DXFAILLOAD: done, " $ Frames $ " frames drawn with a level action; exiting");
		P.ConsoleCommand("exit");
		Step = 6;
	}
}
