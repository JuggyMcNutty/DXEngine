//=============================================================================
// SaveWordsConsole: the console's SAVEGAME and DELETEGAME words (bugs.md
// PKG.37), logged with "DXSWORDS2:". The original's game engine takes
// either only when the text past the word begins with a digit, its slot
// appAtoi of that text, a save's description always "" (the level's
// Title), every form taken (DDeusExGameEngine::Exec, DeusEx.dll
// 0x1000ed70-0x1000eef9; SaveGame 0x1000ef10, DeleteGame 0x10010710).
// DXEngine's takes the second blank-separated word by std::stoi, a sign
// too, and a third as the description, and takes no other count of words
// (Engine.cpp:3146-3182).
// On Liberty Island (DXEngine's map, the original's DXCAP_START), with
// DXCAP_INI="Core.System:SavePath=..\DXCapSaves" so no player's save is
// touched, each command from a CmdHelper's tick 3 s apart, then the save
// folder listed as the Load Game screen lists it, each slot's SaveInfo
// description:
//   SAVEGAME 71 DxDesc     both save slot 71; its description the
//                          level's Title in the original, "DxDesc" in
//                          DXEngine
//   SAVEGAME 72 Two Words  the original saves slot 72 (the Title);
//                          DXEngine none ("Unknown command")
//   SAVEGAME +73           the original none; DXEngine slot 73
//   SAVEGAME -74           the original none; DXEngine Save-074
//   DELETEGAME +71         the original none, 71 kept; DXEngine 71 deleted
// Then an exit. Remove ..\DXCapSaves after each run.
//=============================================================================
class SaveWordsConsole extends Console;

var float RunTime;
var int Step;
var string Commands[8];

function ListSaves(DeusExPlayer P)
{
	local GameDirectory Dir;
	local DeusExSaveInfo Info;
	local int i;

	if (P == None)
		return;
	Dir = P.CreateGameDirectoryObject();
	Dir.SetDirType(Dir.EGameDirectoryTypes.GD_SaveGames);
	Dir.GetGameDirectory();
	for (i = 0; i < Dir.GetDirCount(); i++)
	{
		Info = Dir.GetSaveInfo(i);
		if (Info != None)
		{
			Log("DXSWORDS2: listed " $ Dir.GetDirFilename(i) $ ", directory index " $ Info.DirectoryIndex $ ", description '" $ Info.Description $ "'");
			Dir.DeleteSaveInfo(Info);
		}
		else
			Log("DXSWORDS2: listed " $ Dir.GetDirFilename(i) $ ", no SaveInfo");
	}
	Log("DXSWORDS2: listed " $ Dir.GetDirCount() $ " in all");
	CriticalDelete(Dir);
}

event Tick(float Delta)
{
	local CmdHelper H;

	Super.Tick(Delta);
	if (Step > ArrayCount(Commands) || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime < 3.0 + 3.0 * Step)
		return;
	if (Step == ArrayCount(Commands) || Commands[Step] == "")
	{
		ListSaves(DeusExPlayer(Viewport.Actor));
		Log("DXSWORDS2: done, exiting");
		Step = ArrayCount(Commands) + 1;
		Viewport.Actor.ConsoleCommand("exit");
		return;
	}
	H = Viewport.Actor.Spawn(class'CmdHelper', Viewport.Actor);
	H.Command = Commands[Step];
	Step++;
}

defaultproperties
{
	Commands(0)="SAVEGAME 71 DxDesc"
	Commands(1)="SAVEGAME 72 Two Words"
	Commands(2)="SAVEGAME +73"
	Commands(3)="SAVEGAME -74"
	Commands(4)="DELETEGAME +71"
}
