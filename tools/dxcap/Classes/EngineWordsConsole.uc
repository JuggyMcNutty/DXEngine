//=============================================================================
// EngineWordsConsole: what the local player's ConsoleCommand gives for the
// engine's own words (bugs.md WIN.31, WIN.38), logged with "DXEWORDS:".
// From the menu map, 2 s in, each of Words through
// Viewport.Actor.ConsoleCommand -- the original's viewport, then its
// player, then DDeusExGameEngine::Exec (DeusEx.dll 0x1000ed70),
// UGameEngine::Exec (Engine.dll 0x1038a030), ULevel::Exec (0x1039dfe0),
// UEngine::Exec (0x10380ee0): USystem::Exec (Core.dll 0x1016b790),
// UObject::StaticExec (0x10153110), FMemCache::Exec (0x10125500), the
// client's, the renderer's, then the audio's (Galaxy.dll 0x10607070) --
// each answer's length, its first 100 characters and its last 100, the
// answer being every line the word wrote to the command's output, run
// together (the string device, Engine.dll 0x10374500, adds no line break).
// The music's place is logged as the words go (the player's SongSection,
// CdTrack and Transition): before MUSICORDER, 1.5 s after it, after
// CDTRACK, and 1.5 s after that. With Record (DXCAP_CONFIG "Record=True",
// DXEngine alone: the original's RECORDSOUND records the desktop's
// microphone, and a failing waveInOpen ends it in a critical error),
// RECORDSOUND with the words and ENDRECORDSOUND 1.5 s after, with
// MUSICORDER's check: the recording is played at the player, and Test.wav is
// left in System, for the run to delete. The last word, EXIT, logged too:
// its answer comes before the exit.
//
// What the original's gives, by reading:
//   MEMSTAT     "Memory available: Phys=<n>K Pagef=<n>K Virt=<n>KMemory load = <n>%"
//   CONFIGHASH  "Files map:", "TMapBase: <n> items, <n> hash slots" and a
//               "   Hash[<i>] = <n>" a slot (the executable's
//               FConfigCacheIni::Dump): 153 characters, 5 files in 8 slots
//   DUMPCACHE   one "%02X [%i]: %s" line a cache item (Stale, Fresh, Empty,
//               Locked): 478 characters
//   GTIME, DUMPNATIVES  '' (to the log alone: "GTime = <f>", and "Native
//               index <i> is available" for each free index)
//   GET DxNoSuchClass X, SET DxNoSuchClass X 1
//               "Unrecognized class DxNoSuchClass"
//   OBJ LIST CLASS=DeusExLevelInfo   "Objects:" and the table, its last
//               line "<n> Objects (<f>M / <f>M)", the bytes what the
//               serializer counts of each object (FArchiveCountMem)
//   OBJ LIST CLASS=Texture INSIDE=DeusExUI   the same of the menu's
//               textures, their mips' bytes among them
//   OBJ LINKERS "Linkers:" and a line a package loaded: 2,880 characters
//   OBJ HASH    "Hash: <n> names, <n>/4096 hash bins" (FName::DisplayHash):
//               38 characters
//   OBJ REFS CLASS=Class NAME=DeusExPlayer   "Referencers of Class
//               DeusEx.DeusExPlayer:" and the list, then "Shortest
//               reachability from root to Class DeusEx.DeusExPlayer:" and
//               the chain: 62,118 characters
//   OBJ DEPENDENCIES PACKAGE=DeusExUI EXCLUDE0=Core EXCLUDE1=Engine
//               "Dependencies of DeusExUI:", and each top object's
//               references outside it and the two left out (each heading
//               the original's first reference, DXEngine's object)
//   OBJ VFHASH  "Class VfHashes:", each state's name and its 256 slots'
//               counts
//   CRACKURL DX.dx?Name=Two Words   "BAD URL" (a blank in an option; a
//               URL that reads crashes the original's CRACKURL at its
//               String line, 0x1038111d-0x10381129, which pushes the
//               string's first character for a %s: so only a bad one here)
//   SOCKETS     '' (no net driver in single player: no taker)
//   MLOD, TLOD, MLMODE, MLFIX   the renderer's settings, none given:
//               "0.250000", "3.000000", "1", "1.000000" (URender::Exec,
//               Render.dll 0x10b147b0; URender::Init 0x10b134d7-0x10b134ef)
//   REND LEAK   "Rendering option recognized", twice, the option back off
//   MEM         '' (the allocator's dump, whose lines the game's build
//               leaves out)
//   OBJ MARK, OBJ MARKCHECK     '' (to the log: "Marking objects", then
//               "Unmarked objects:" and each object made between)
//   CDVOLUME 0.5, ASTAT, ASTAT AUDIO, ASTAT DETAIL   '' (ASTAT's twice
//               each, its overlay back off: ViewWordsConsole shoots it)
//   ENDRECORDSOUND   '' (no recording under way: nothing)
//   With Record, RECORDSOUND   '' (to the log "RecordSound begin", or
//               DXEngine's "RecordSound: the capture device will not open")
//   and ENDRECORDSOUND   '' (to the log "RecordSound end (<n> samples)",
//               some 12,000 in 1.5 s, then "wFormatTag: 1", "nChannels: 1",
//               "nSamplesPerSec: 8000", "nAvgBytesPerSec: 16000",
//               "nBlockAlign: 2", "nBitsPerSample: 16" as Test.wav is
//               written)
//   MUSICORDER 1     "Galaxy order 1", the player's SongSection 1 after
//               (while the menu's song plays; no song, no taker: '')
//   CDTRACK 2   "CD Track 2", the player's CdTrack 2 and Transition 1
//               (MTRAN_Instant) after it, Transition 0 once the music's
//               update has taken it
//   EXIT        "Closing by request"
//=============================================================================
class EngineWordsConsole extends Console;

var config bool Record;
var float RunTime;
var int Phase;
var string Words[40];

function Ask(PlayerPawn P, string W)
{
	local string R;

	R = P.ConsoleCommand(W);
	Log("DXEWORDS: '" $ W $ "' gave " $ Len(R) $ " characters: '" $ Left(R, 100) $ "'");
	if (Len(R) > 100)
		Log("DXEWORDS: '" $ W $ "' ends '" $ Right(R, 100) $ "'");
}

function ShowMusic(PlayerPawn P, string When)
{
	Log("DXEWORDS: " $ When $ ": Song " $ P.Song $ " SongSection " $ P.SongSection $ " CdTrack " $ P.CdTrack $ " Transition " $ P.Transition);
}

event Tick(float Delta)
{
	local int i;
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		for (i = 0; i < ArrayCount(Words); i++)
		{
			if (Words[i] != "")
				Ask(P, Words[i]);
		}
		if (Record)
			Ask(P, "RECORDSOUND");
		ShowMusic(P, "before MUSICORDER");
		Ask(P, "MUSICORDER 1");
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.5)
	{
		if (Record)
			Ask(P, "ENDRECORDSOUND");
		ShowMusic(P, "1.5 s after MUSICORDER");
		Ask(P, "CDTRACK 2");
		ShowMusic(P, "after CDTRACK");
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 5.0)
	{
		ShowMusic(P, "1.5 s after CDTRACK");
		Ask(P, "EXIT");
		Phase = 99;
	}
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
	Words(12)="MLOD"
	Words(13)="TLOD"
	Words(14)="MLMODE"
	Words(15)="MLFIX"
	Words(16)="REND LEAK"
	Words(17)="REND LEAK"
	Words(18)="MEM"
	Words(19)="OBJ MARK"
	Words(20)="OBJ MARKCHECK"
	Words(21)="DUMPNATIVES"
	Words(22)="OBJ LIST CLASS=Texture INSIDE=DeusExUI"
	Words(23)="OBJ DEPENDENCIES PACKAGE=DeusExUI EXCLUDE0=Core EXCLUDE1=Engine"
	Words(24)="OBJ VFHASH"
	Words(25)="CDVOLUME 0.5"
	Words(26)="ASTAT"
	Words(27)="ASTAT AUDIO"
	Words(28)="ASTAT AUDIO"
	Words(29)="ASTAT DETAIL"
	Words(30)="ASTAT DETAIL"
	Words(31)="ENDRECORDSOUND"
}
