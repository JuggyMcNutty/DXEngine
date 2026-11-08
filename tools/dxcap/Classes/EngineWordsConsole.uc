//=============================================================================
// EngineWordsConsole: what the local player's ConsoleCommand gives for the
// engine's own words that DXEngine has not (bugs.md WIN.31), logged
// with "DXEWORDS:". From the menu map, 2 s in, each of Words through
// Viewport.Actor.ConsoleCommand -- the original's viewport, then its
// player, then DDeusExGameEngine::Exec (DeusEx.dll 0x1000ed70),
// UGameEngine::Exec (Engine.dll 0x1038a030), ULevel::Exec (0x1039dfe0),
// UEngine::Exec (0x10380ee0): USystem::Exec (Core.dll 0x1016b790),
// UObject::StaticExec (0x10153110), FMemCache::Exec (0x10125500) -- each
// answer's length and first 100 characters, the answer being every line
// the word wrote to the command's output, run together (the string device,
// Engine.dll 0x10374500, adds no line break). The last word, EXIT, logged
// too: its answer comes before the exit.
//
// What the original's gives, by reading:
//   MEMSTAT     "Memory available: Phys=<n>K Pagef=<n>K Virt=<n>KMemory load = <n>%"
//   CONFIGHASH  the config cache's dump (GConfig's, DeusEx.exe's), not ''
//   DUMPCACHE   one "%02X [%i]: %s" line a cache item (Stale, Fresh, Empty,
//               Locked), not ''
//   GTIME       '' (it writes to the log alone, "GTime = <f>"; so do
//               DUMPNATIVES, "Native index <i> is available" for each free
//               index, and OBJ MARK, "Marking objects", left out here)
//   GET DxNoSuchClass X, SET DxNoSuchClass X 1
//               "Unrecognized class DxNoSuchClass"
//   OBJ LIST CLASS=DeusExLevelInfo   "Objects:" and the table, its last
//               line "<n> Objects (<f>M / <f>M)"
//   OBJ LINKERS "Linkers:" and a line a package loaded
//   OBJ HASH    the name table's hash figures (FName::DisplayHash)
//   OBJ REFS CLASS=Class NAME=DeusExPlayer   "Referencers of Class
//               DeusEx.DeusExPlayer:" and the list
//   CRACKURL DX.dx?Name=Two Words   "BAD URL" (a blank in an option; a
//               URL that reads crashes the original's CRACKURL at its
//               String line, 0x1038111d-0x10381129, which pushes the
//               string's first character for a %s: so only a bad one here)
//   SOCKETS     '' (no net driver in single player: no taker)
//   EXIT        "Closing by request"
// DXEngine fe46ad13, by reading: '' for every one, an "Unknown command"
// line in its log for each but GET and SET ("Could not find class") and
// EXIT.
//=============================================================================
class EngineWordsConsole extends Console;

var float RunTime;
var bool bDone;
var string Words[24];

event Tick(float Delta)
{
	local int i;
	local string R;

	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	bDone = true;
	for (i = 0; i < ArrayCount(Words); i++)
	{
		if (Words[i] == "")
			continue;
		R = Viewport.Actor.ConsoleCommand(Words[i]);
		Log("DXEWORDS: '" $ Words[i] $ "' gave " $ Len(R) $ " characters: '" $ Left(R, 100) $ "'");
	}
	R = Viewport.Actor.ConsoleCommand("EXIT");
	Log("DXEWORDS: 'EXIT' gave '" $ R $ "'");
}

defaultproperties
{
	Words(0)="MEMSTAT"
	Words(1)="CONFIGHASH"
	Words(2)="DUMPCACHE"
	Words(3)="GTIME"
	Words(4)="GET DxNoSuchClass X"
	Words(5)="SET DxNoSuchClass X 1"
	Words(6)="OBJ LIST CLASS=DeusExLevelInfo"
	Words(7)="OBJ LINKERS"
	Words(8)="OBJ HASH"
	Words(9)="OBJ REFS CLASS=Class NAME=DeusExPlayer"
	Words(10)="CRACKURL DX.dx?Name=Two Words"
	Words(11)="SOCKETS"
}
