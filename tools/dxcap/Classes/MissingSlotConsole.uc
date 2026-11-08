//=============================================================================
// MissingSlotConsole: the Load Game screen's row for a slot whose SaveInfo
// does not load, logged with "DXMISSING:" (its frames with a level action as
// LoadingConsole's PostRender logs them, "DXLOADING:").
//
// MenuScreenLoadGame.PopulateGames gives such a slot the row
// "SaveInfoMissing;;;;-2": selecting it calls GetSaveInfo(-2) and
// GetSaveDirectorySize(-2) (UpdateSaveInfo), its Load sends LoadGame(-2),
// ?loadgame=-2. In the original GameDirectory's GetSaveInfo and
// GetSaveDirectorySize take -1 for the quick save and any other number as
// Save%04d, so -2 is Save-002 (DeusEx.dll 0x10017eb0, 0x100179d0); DXEngine
// takes -2 for Current (UGameDirectory.cpp GetSaveInfo, SaveSlotFolderName).
// The original's SaveGame writes the SaveInfo to the slot alone (0x1000f8e6),
// Current getting one only from a load's copy of a slot; DXEngine writes it
// into Current at every save (Engine.cpp SaveGameToSlot). The original's
// ?loadgame copies the QuickSave folder when the SaveInfo it read has
// DirectoryIndex -1, else Save%04d of the URL's number (Browse, the test of
// SaveInfo+0x40); DXEngine copies the number's own folder, Current for -2.
//
// Before the run: an empty folder Save\Save0077 (a slot whose SaveInfo is
// not there; or one badpackage.py made, its tag zeroed), no Save0009, and
// Save\Current there but empty, as any install played has it (DXEngine
// writes Current's SaveInfo only into a Current folder that is there,
// Engine.cpp SaveGameToSlot). From the menu map:
// 1. 3 s in, 01_NYC_UNATCOIsland opened.
// 2. 6 s in, the listing as the Load Game screen reads it, GetSaveInfo(-2)
//    and GetSaveDirectorySize(-2); then slot 9 saved (SaveHelper).
// 3. 3 s later the same: the original's GetSaveInfo(-2) None, DXEngine's
//    Current's, DirectoryIndex 9. Then LoadGame(-2), as the row's Load:
//    the original finds no SaveInfo in Current and does nothing; DXEngine
//    empties Current, paints LOADING and fails ("Can't find file").
// 4. 4 s later the player's progress lines; the quick save (SaveHelper,
//    slot -1); 3 s later LoadGame(-1).
// 5. 4 s into the loaded level, GetSaveInfo(-2) and LoadGame(-2) again:
//    the original's Current holds the quick save's SaveInfo (DirectoryIndex
//    -1), so it copies QuickSave and loads the island again (a new level);
//    DXEngine empties Current and fails.
// 6. 5 s later the progress lines and the map, then an exit. After the
//    run: the game's Save\Current listed (the original's the quick save's
//    files, DXEngine's empty); remove Save0009, Save0077 and QuickSave's
//    copy as SaveConsole's runs leave them.
//=============================================================================
class MissingSlotConsole extends LoadingConsole;

var float StepTime;
var int Levels;

function Say(string S)
{
	Log("DXMISSING: " $ S);
}

function string InfoText(DeusExSaveInfo Info)
{
	if (Info == None)
		return "None";
	return "index " $ Info.DirectoryIndex $ ", map " $ Info.MapName $ ", location '" $ Info.MissionLocation
		$ "', count " $ Info.saveCount $ ", '" $ Info.Description $ "'";
}

// As MenuScreenLoadGame's PopulateGames and UpdateSaveInfo read them
function Listing(DeusExPlayer P, string When)
{
	local GameDirectory Dir;
	local DeusExSaveInfo Info;
	local int i;

	Dir = P.CreateGameDirectoryObject();
	Dir.SetDirType(Dir.EGameDirectoryTypes.GD_SaveGames);
	Dir.GetGameDirectory();
	for (i = 0; i < Dir.GetDirCount(); i++)
	{
		Info = Dir.GetSaveInfoFromDirectoryIndex(i);
		Say(When $ ": row " $ Dir.GetDirFilename(i) $ ": " $ InfoText(Info));
		if (Info != None)
			Dir.DeleteSaveInfo(Info);
	}
	CriticalDelete(Dir);

	Dir = P.CreateGameDirectoryObject();
	Dir.SetDirType(Dir.EGameDirectoryTypes.GD_SaveGames);
	Dir.GetGameDirectory();
	Info = Dir.GetSaveInfo(-2);
	Say(When $ ": GetSaveInfo(-2) " $ InfoText(Info) $ ", GetSaveDirectorySize(-2) "
		$ Dir.GetSaveDirectorySize(-2) $ ", (9) " $ Dir.GetSaveDirectorySize(9) $ ", (-1) "
		$ Dir.GetSaveDirectorySize(-1));
	CriticalDelete(Dir);
}

function Report(PlayerPawn P, string When)
{
	Say(When $ ": on " $ MapOf(P) $ ", progress '" $ P.ProgressMessage[0] $ "' '" $ P.ProgressMessage[1] $ "'");
}

function Save(PlayerPawn P, int Slot)
{
	local SaveHelper S;

	Say("saving to slot " $ Slot);
	S = P.Spawn(class'SaveHelper', P);
	S.Slot = Slot;
	S.Description = "DXCapture missing " $ Slot;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer D;

	Super(Console).Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	D = DeusExPlayer(P);
	if (P.Level.LevelAction != LEVACT_None)
	{
		Say("ticked with " $ ActionName(P.Level.LevelAction) $ " in " $ MapOf(P));
		return;
	}
	RunTime += Delta;
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	bShotDue = false;
	if (bNewLevel)
	{
		bNewLevel = false;
		CurrentMap = MapOf(P);
		MapTime = 0;
		StepTime = 0;
		Levels++;
		Say("now in " $ CurrentMap $ " at " $ RunTime $ " (level " $ Levels $ ")");
	}
	if (D == None)
		return;

	if (Step == 0 && MapTime > 3.0)
	{
		Say("opening 01_NYC_UNATCOIsland");
		P.ConsoleCommand("open 01_NYC_UNATCOIsland");
		Step = 1;
	}
	else if (Step == 1 && CurrentMap ~= "01_NYC_UNATCOIsland" && StepTime > 6.0)
	{
		Listing(D, "before any save");
		Save(P, 9);
		Step = 2;
		StepTime = 0;
	}
	else if (Step == 2 && StepTime > 3.0)
	{
		Listing(D, "after slot 9's save");
		Say("LoadGame(-2)");
		D.LoadGame(-2);
		Step = 3;
		StepTime = 0;
	}
	else if (Step == 3 && StepTime > 4.0)
	{
		Report(P, "after LoadGame(-2)");
		Listing(D, "after LoadGame(-2)");
		Save(P, -1);
		Step = 4;
		StepTime = 0;
	}
	else if (Step == 4 && StepTime > 3.0)
	{
		Say("LoadGame(-1)");
		D.LoadGame(-1);
		Step = 5;
		StepTime = 0;
	}
	else if (Step == 5 && Levels >= 2 && StepTime > 4.0)
	{
		Listing(D, "after the quick load");
		Say("LoadGame(-2) after the quick load");
		D.LoadGame(-2);
		Step = 6;
		StepTime = 0;
	}
	else if (Step == 6 && StepTime > 5.0)
	{
		Report(P, "at the end");
		Say("done, " $ Levels $ " levels; exiting");
		P.ConsoleCommand("exit");
		Step = 7;
	}
}
