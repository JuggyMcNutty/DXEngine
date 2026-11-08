//=============================================================================
// LoadTravelConsole: a travel asked while a travel's own frame is painted,
// logged with "DXLOADTRAVEL:" (its frames with a level action as
// LoadingConsole's PostRender logs them, "DXLOADING:").
//
// The original's game engine empties the viewport's travel after the browse
// that took it (UGameEngine::Tick, Engine.dll 0x10390c01 the Browse,
// 0x10390c07 the TravelURL set empty), so an OPEN asked from inside that
// browse -- the console's tick that its SAVING or LOADING paint runs -- is
// dropped. Both engines alike (bugs.md PKG.34).
//
// From the menu map:
// 1. 3 s in, `open 01_NYC_UNATCOIsland`; at the console's first tick under
//    LOADING (the menu map's paint), `open 01_NYC_UNATCOHQ`, dropped: the
//    run stays on the island.
// 2. 6 s after the last level change, a travel within mission 1 to the
//    other of the two maps (SAVING, then LOADING); at its first tick under
//    SAVING, `open DXOnly`: dropped in both, the travel's map reached.
// 3. 6 s later the map, then an exit.
//=============================================================================
class LoadTravelConsole extends LoadingConsole;

var bool bAskedLoading, bAskedSaving;
var int Levels;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super(Console).Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (P.Level.LevelAction != LEVACT_None)
	{
		Log("DXLOADTRAVEL: ticked with " $ ActionName(P.Level.LevelAction) $ " in " $ MapOf(P));
		if (Step == 1 && P.Level.LevelAction == LEVACT_Loading && !bAskedLoading)
		{
			bAskedLoading = true;
			Log("DXLOADTRAVEL: asking for 01_NYC_UNATCOHQ during LOADING");
			P.ConsoleCommand("open 01_NYC_UNATCOHQ");
		}
		else if (Step == 3 && P.Level.LevelAction == LEVACT_Saving && !bAskedSaving)
		{
			bAskedSaving = true;
			Log("DXLOADTRAVEL: asking for DXOnly during SAVING");
			P.ConsoleCommand("open DXOnly");
		}
		return;
	}
	RunTime += Delta;
	MapTime += Delta;
	P.ReducedDamageType = 'All';
	bShotDue = false;
	if (bNewLevel)
	{
		bNewLevel = false;
		CurrentMap = MapOf(P);
		MapTime = 0;
		Levels++;
		Log("DXLOADTRAVEL: now in " $ CurrentMap $ " at " $ RunTime $ " (level " $ Levels $ ")");
	}

	if (Step == 0 && MapTime > 3.0)
	{
		Log("DXLOADTRAVEL: opening 01_NYC_UNATCOIsland");
		P.ConsoleCommand("open 01_NYC_UNATCOIsland");
		Step = 1;
	}
	else if (Step == 1 && Levels > 0 && MapTime > 6.0)
	{
		Log("DXLOADTRAVEL: after the first travel on " $ CurrentMap);
		Step = 3;
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
		{
			Log("DXLOADTRAVEL: opening 01_NYC_UNATCOHQ, within mission 1");
			P.ConsoleCommand("open 01_NYC_UNATCOHQ");
		}
		else
		{
			Log("DXLOADTRAVEL: opening 01_NYC_UNATCOIsland, within mission 1");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
		}
	}
	else if (Step == 3 && bAskedSaving && MapTime > 6.0)
	{
		Log("DXLOADTRAVEL: done on " $ CurrentMap $ ", " $ Levels $ " levels; exiting");
		P.ConsoleCommand("exit");
		Step = 4;
	}
}
