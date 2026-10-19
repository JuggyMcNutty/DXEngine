//=============================================================================
// CensusConsole: what a save loaded in the running game spawns and keeps --
// the loaded level's actors counted by class each second. From the menu map,
// or from From (DXCAP_CONFIG "From=<map>") opened first, slot 9 is loaded
// Wait seconds in (8 by default), by the game's LoadGame or, with Open, by
// the console's open of that URL (LoadConsole's); with Load=False nothing is
// loaded and the run's own level is counted from its start: a run started
// on the save (DXEngine's ?loadgame=9 for its map, the original's
// DXCAP_START=?loadgame=9). The new level is known by NotifyLevelChange,
// which both engines send before the old one goes, and the console holds no
// actor across a load. In the new level a CensusNotify hears every spawn
// (Notify=False: none, and every actor is walked each second instead).
// Each second, for Seconds seconds (90 by default), a line of totals and,
// for each class whose numbers changed (each class in the first second), up
// to 1024 classes (Liberty Island has some 260): its live actors, the most
// of them live in any second so far, those spawned in that second (as the
// notify hears them: never one destroyed in its own spawn), and the highest
// number in a name of its actors: where that number rises by more than the
// spawns, the gap counts deaths in spawn. Eight classes a line, under the
// original's line limit,
// with "DXCENSUS: <second> <class>=<live>/<peak>/<spawned>/<name> ...";
// then an exit. The player takes no damage (ReducedDamageType 'All', as
// SaveConsole sets it): the island's start is under fire, and the death's
// view ends in a travel to DXOnly. tools/dxcap/census.py lays two runs side
// by side.
//=============================================================================
class CensusConsole extends LoadConsole;

var config string From;
var config float Wait;
var config bool Load;
var config int Seconds;
var config bool Notify;

var bool LevelChanged, Opened, Overflow;
var float CensusTime, NextCensus;
var int Second;
var int NumClasses, LastIndex;
var class<Actor> Classes[1024];
var int Live[1024];
var int Peak[1024];
var int Spawned[1024];
var int MaxName[1024];
var int LastLive[1024];
var int LastName[1024];

event NotifyLevelChange()
{
	Super.NotifyLevelChange();
	LevelChanged = True;
}

// The number a name ends in after its class's name, as a spawn names an
// actor; -1 for any other name.
function int NameNumber(Actor A)
{
	local string N, C;

	N = string(A.Name);
	C = string(A.Class.Name);
	if (Len(N) <= Len(C) || !(Left(N, Len(C)) ~= C))
		return -1;
	return int(Mid(N, Len(C)));
}

function int ClassIndex(class<Actor> C)
{
	local int i;

	if (LastIndex >= 0 && Classes[LastIndex] == C)
		return LastIndex;
	for (i = 0; i < NumClasses; i++)
	{
		if (Classes[i] == C)
		{
			LastIndex = i;
			return i;
		}
	}
	if (NumClasses >= 1024)
	{
		if (!Overflow)
			Log("DXCENSUS: over 1024 classes: " $ C.Name $ " and those after it not counted");
		Overflow = True;
		return -1;
	}
	Classes[NumClasses] = C;
	Live[NumClasses] = 0;
	Peak[NumClasses] = 0;
	Spawned[NumClasses] = 0;
	MaxName[NumClasses] = -1;
	LastLive[NumClasses] = 0;
	LastName[NumClasses] = -1;
	LastIndex = NumClasses;
	NumClasses++;
	return LastIndex;
}

// CensusNotify's call, for each actor spawned into the counted level.
function NoteSpawn(Actor A)
{
	local int i, n;

	if (A == None || Step != 2)
		return;
	i = ClassIndex(A.Class);
	if (i < 0)
		return;
	Spawned[i]++;
	n = NameNumber(A);
	if (n > MaxName[i])
		MaxName[i] = n;
}

function BeginCensus(PlayerPawn P)
{
	local CensusNotify N;

	LevelChanged = False;
	NumClasses = 0;
	LastIndex = -1;
	Overflow = False;
	if (Notify)
	{
		N = P.Spawn(class'CensusNotify');
		if (N != None)
			N.Census = Self;
		else
			Log("DXCENSUS: no CensusNotify spawned");
	}
	Log("DXCENSUS: counting " $ CurrentMap $ " for " $ Seconds $ " s, spawns heard: " $ (Notify && N != None));
	CensusTime = 0;
	NextCensus = 0;
	Second = 0;
	Step = 2;
}

function Count(PlayerPawn P)
{
	local Actor A;
	local int i, n, TotalLive, TotalSpawned, Shown;
	local string Line;

	for (i = 0; i < NumClasses; i++)
		Live[i] = 0;
	if (Second == 0 || !Notify)
	{
		// Every actor: the first second's classes, which no spawn named.
		foreach P.AllActors(class'Actor', A)
		{
			i = ClassIndex(A.Class);
			if (i >= 0)
			{
				Live[i]++;
				n = NameNumber(A);
				if (n > MaxName[i])
					MaxName[i] = n;
			}
		}
	}
	else
	{
		// A class's own actors, by its iterator: every class since was
		// named by a spawn the notify heard.
		for (i = 0; i < NumClasses; i++)
		{
			foreach P.AllActors(Classes[i], A)
			{
				if (A.Class == Classes[i])
					Live[i]++;
			}
		}
	}
	for (i = 0; i < NumClasses; i++)
	{
		TotalLive += Live[i];
		TotalSpawned += Spawned[i];
		if (Live[i] > Peak[i])
			Peak[i] = Live[i];
	}
	Log("DXCENSUS: " $ Second $ " total live " $ TotalLive $ " spawned " $ TotalSpawned $ " classes " $ NumClasses);
	for (i = 0; i < NumClasses; i++)
	{
		if (Second == 0 || Live[i] != LastLive[i] || Spawned[i] != 0 || MaxName[i] != LastName[i])
		{
			Line = Line $ " " $ Classes[i].Name $ "=" $ Live[i] $ "/" $ Peak[i] $ "/" $ Spawned[i] $ "/" $ MaxName[i];
			Shown++;
			if (Shown == 8)
			{
				Log("DXCENSUS: " $ Second $ Line);
				Line = "";
				Shown = 0;
			}
			LastLive[i] = Live[i];
			LastName[i] = MaxName[i];
			Spawned[i] = 0;
		}
	}
	if (Shown > 0)
		Log("DXCENSUS: " $ Second $ Line);
}

function Finish(PlayerPawn P, string Why)
{
	Log("DXCENSUS: " $ Why $ "; exiting");
	P.ConsoleCommand("exit");
	Step = 3;
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super(Console).Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	P.ReducedDamageType = 'All';
	RunTime += Delta;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;

	if (Step == 0 && !Load)
	{
		if (CurrentMap != "" && !(CurrentMap ~= "Entry"))
			BeginCensus(P);
	}
	else if (Step == 0 && From != "" && !(CurrentMap ~= From))
	{
		if (!Opened && MapTime > 2.0)
		{
			Log("DXCENSUS: opening " $ From);
			P.ConsoleCommand("open " $ From);
			Opened = True;
		}
		else if (RunTime > 60.0)
			Finish(P, "no " $ From $ " after 60 s, in " $ CurrentMap);
	}
	else if (Step == 0 && MapTime > Wait)
	{
		LevelChanged = False;
		if (Open != "")
		{
			Log("DXCENSUS: opening " $ Open $ " in " $ CurrentMap $ " at " $ int(MapTime) $ " s");
			P.ConsoleCommand("open " $ Open);
		}
		else
		{
			Log("DXCENSUS: loading slot 9 in " $ CurrentMap $ " at " $ int(MapTime) $ " s");
			DeusExPlayer(P).LoadGame(9);
		}
		Step = 1;
	}
	else if (Step == 1)
	{
		if (LevelChanged)
			BeginCensus(P);
		else if (MapTime > Wait + 60.0)
			Finish(P, "nothing loaded after 60 s, in " $ CurrentMap);
	}
	else if (Step == 2)
	{
		if (LevelChanged)
		{
			Finish(P, "the level changed at " $ Second $ " s");
			return;
		}
		CensusTime += Delta;
		if (CensusTime >= NextCensus)
		{
			Count(P);
			NextCensus += 1.0;
			Second++;
		}
		if (Second > Seconds)
			Finish(P, "counted " $ Seconds $ " s");
	}
}

defaultproperties
{
	Wait=8.0
	Load=True
	Seconds=90
	Notify=True
}
