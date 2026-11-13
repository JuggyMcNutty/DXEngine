//=============================================================================
// SenderTieConsole (bugs.md AI.23): which of several senders of one score an
// AI event's receiver takes for its best, logged with "DXSENDTIE:" --
// TargetMap ([DXCapture.SenderTieConsole], Liberty Island by default) opened
// from wherever the run starts; from 2 s into the level's time, three
// stages, each a CapTieListener at Base (the player's place cut to whole
// units, 200 over it) and CapEdgeMakers about it pinging their event each
// console tick (AISendEvent, audio, 1, 4000), in their order, so the
// event's senders are listed in it; after half a second the listener's
// calls, its last state and its best sender, by the maker's number; then
// their actors destroyed:
//
// 1. Three senders, CapTie1: maker 0 60 units off, makers 1 and 2 40 units
//    off on either side, a squared distance of 1,601 each.
// 2. Twelve senders, CapTie2: makers 0 to 8 at 100 to 180 units, 10 apart,
//    makers 9, 10 and 11 at 40.
// 3. Twenty senders, CapTie3, the listener's score callback OneScore
//    answering 1 for each.
//
// The original sorts a receiver's senders by score with the C runtime's
// qsort (UEventManager::AIProcess, Engine.dll 0x103842d2), equal scores
// ordered by their places in its array as each comparison finds them
// (0x10382420), and takes the first it senses: the best maker 2, 11 and 0.
// DXEngine before its fix (bugs.md AI.23) sorted them by std::sort: 1, 9 and
// 10. Then an exit. Compared: every line.
//=============================================================================
class SenderTieConsole extends Console;

var config string TargetMap;

var float MapTime;
var float StageTime;
var int Step;
var string CurrentMap;
var vector Base;
var CapTieListener L;
var CapEdgeMaker M[20];
var int Count;

function string Target()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOIsland";
	return TargetMap;
}

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	U = P.Level.GetLocalURL();
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	i = InStr(U, "\\");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "\\");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

function TieLog(string S)
{
	Log("DXSENDTIE: " $ S);
}

function Finish(PlayerPawn P)
{
	TieLog("exiting");
	P.ConsoleCommand("exit");
	Step = 99;
}

// A maker at Base plus Offset, whole units, raising EventName
function bool Maker(PlayerPawn P, int i, vector Offset, name EventName)
{
	M[i] = P.Spawn(class'CapEdgeMaker',,, Base + Offset);
	if (M[i] == None)
	{
		TieLog("no room for maker " $ i);
		return false;
	}
	M[i].EventName = EventName;
	return true;
}

function bool Listener(PlayerPawn P, name EventName, optional name ScoreName)
{
	L = P.Spawn(class'CapTieListener',,, Base);
	if (L == None)
	{
		TieLog("no room for the listener");
		return false;
	}
	L.Listen(EventName, ScoreName);
	return true;
}

function bool Stage(PlayerPawn P, int n)
{
	local int i;

	if (n == 1)
	{
		Count = 3;
		if (!Listener(P, 'CapTie1'))
			return false;
		if (!Maker(P, 0, vect(0, 60, 0), 'CapTie1') || !Maker(P, 1, vect(40, 0, 0), 'CapTie1') || !Maker(P, 2, vect(-40, 0, 0), 'CapTie1'))
			return false;
	}
	else if (n == 2)
	{
		Count = 12;
		if (!Listener(P, 'CapTie2'))
			return false;
		for (i = 0; i < 9; i++)
		{
			if (!Maker(P, i, vect(0, 1, 0) * (100 + 10 * i), 'CapTie2'))
				return false;
		}
		if (!Maker(P, 9, vect(40, 0, 0), 'CapTie2') || !Maker(P, 10, vect(-40, 0, 0), 'CapTie2') || !Maker(P, 11, vect(0, -40, 0), 'CapTie2'))
			return false;
	}
	else
	{
		Count = 20;
		if (!Listener(P, 'CapTie3', 'OneScore'))
			return false;
		for (i = 0; i < 20; i++)
		{
			if (!Maker(P, i, vect(10, 0, 0) * (i - 10) + vect(0, 0, 50), 'CapTie3'))
				return false;
		}
	}
	TieLog("stage " $ n $ ": " $ L.Name $ " at " $ L.Location $ ", " $ Count $ " makers, maker 0 " $ M[0].Name $ " at " $ M[0].Location);
	return true;
}

function Tally(int n)
{
	local int i, Best;

	Best = -1;
	for (i = 0; i < Count; i++)
		if (L.LastBest != None && L.LastBest == M[i])
			Best = i;
	TieLog("stage " $ n $ ": calls " $ L.Calls $ " state " $ L.LastState $ " best maker " $ Best);
	for (i = 0; i < Count; i++)
	{
		M[i].Destroy();
		M[i] = None;
	}
	L.Destroy();
	L = None;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local float T;
	local int i;

	Super.Tick(Delta);
	if (Step == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	StageTime += Delta;
	P.ReducedDamageType = 'All';

	if (Step == 0 && !(CurrentMap ~= Target()))
	{
		if (MapTime > 3.0)
		{
			TieLog("opening " $ Target());
			P.ConsoleCommand("open " $ Target());
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= Target()))
		Step = 0;

	T = P.Level.TimeSeconds;
	if (Step == 0 && T > 2.0)
	{
		Base.X = float(int(P.Location.X));
		Base.Y = float(int(P.Location.Y));
		Base.Z = float(int(P.Location.Z)) + 200;
		Step = 1;
	}
	if (Step == 1 || Step == 3 || Step == 5)
	{
		if (!Stage(P, (Step + 1) / 2))
		{
			Finish(P);
			return;
		}
		StageTime = 0;
		Step++;
	}
	else if (Step == 2 || Step == 4 || Step == 6)
	{
		for (i = 0; i < Count; i++)
			M[i].Ping();
		if (StageTime > 0.5)
		{
			Tally(Step / 2);
			Step++;
		}
	}
	else if (Step == 7)
		Finish(P);
}
