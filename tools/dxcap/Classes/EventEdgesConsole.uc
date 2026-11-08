//=============================================================================
// EventEdgesConsole (bugs.md AI.21): the AI event manager's edges, logged
// with "DXEVTEDGE:" -- TargetMap ([DXCapture.EventEdgesConsole], Liberty
// Island by default) opened from wherever the run starts; from 2 s into the
// level's time, four stages, each ending with each listener's count of
// calls and its actors destroyed, each console tick (between the level's
// ticks) raising its pulses, each logged "tick" with the level's time:
//
// 1. A None event and a score that is not a number, to 2.5 s: a
//    CapEdgeListener listening for None ('') beside a CapEdgeMaker raising
//    None; another listening for CapNaN with NaNScore, beside a maker
//    raising CapNaN. The original's AISetEventCallback and AISendEvent do
//    nothing for a None name (Engine.dll 0x10382990, 0x10382c60), and its
//    walk drops a sender whose score is not over 0, not a number among them
//    (AIProcess 0x1038424e): no call to either.
// 2. 256 senders, to 3.1 s: a listener for CapCrowd 20 units before 256
//    makers raising it, 3 units apart along X; from 2.85 s a 257th. The
//    original warns at each of the listener's turns from the 256th sender
//    (0x1038426f), "*** WARNING: Greater than 255 objects emitting event
//    "AIEventType<n>"! (Some events will be dropped)", naming the type's
//    own object, its calls the nearest maker's.
// 3. The Entry level's manager, to 3.8 s: a listener for CapPing made in
//    the Entry level (its LevelInfo's Spawn) at the player's place, a
//    CapNoiseMaker in this level 40 units over it pinging, and from 3.5 s
//    one made in the Entry level 80 units over it pinging too. The
//    original's natives take the actor's own level's manager (0x1036d7f6)
//    and each level's full tick ticks its own (0x103a5a2b): no call to the
//    Entry listener until the Entry maker pings, then calls naming it.
// 4. Many slow listeners, to 5.3 s: Slow listeners (60 by default) for
//    CapSlow with SlowScore of BurnLoops steps (20000), quiet, beside a
//    maker raising it. The original's walk stops past 2 ms, and a receiver
//    16 frames without a turn has its next slot moved on and is counted in
//    one warning a frame (0x1038462d-0x10384681): "*** WARNING: Event
//    manager not cycling quickly enough.  <n> events have been dropped."
//    and "There may be too many events occurring on this level.". How many,
//    and how often, follows each engine's speed.
//
// Then an exit. Compared: every line but the times, and the logs' WARNING
// lines.
//=============================================================================
class EventEdgesConsole extends Console;

var config string TargetMap;
var config int Slow;
var config int BurnLoops;

var float MapTime;
var int Step;
var string CurrentMap;

var CapEdgeListener NoneL, NaNL, CrowdL, EntryL;
var CapEdgeMaker NoneM, NaNM, SlowM;
var CapEdgeMaker Crowd[257];
var CapNoiseMaker HereM, EntryM;
var CapEdgeListener SlowL[200];
var int SlowCount;

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

function EdgeLog(string S)
{
	Log("DXEVTEDGE: " $ S);
}

function CapEdgeListener Listener(Actor Spawner, vector Spot)
{
	local CapEdgeListener L;

	L = Spawner.Spawn(class'CapEdgeListener',,, Spot);
	if (L == None)
		EdgeLog("no room for a listener at " $ Spot);
	return L;
}

function CapEdgeMaker Maker(PlayerPawn P, vector Spot, name EventName)
{
	local CapEdgeMaker M;

	M = P.Spawn(class'CapEdgeMaker',,, Spot);
	if (M == None)
		EdgeLog("no room for a maker at " $ Spot);
	else
		M.EventName = EventName;
	return M;
}

function Tally(string What, CapEdgeListener L)
{
	if (L != None)
		EdgeLog(What $ " " $ L.Name $ " calls " $ L.Calls);
}

function Finish(PlayerPawn P)
{
	EdgeLog("exiting");
	P.ConsoleCommand("exit");
	Step = 99;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local LevelInfo Entry;
	local float T;
	local int i, n;

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
	P.ReducedDamageType = 'All';

	if (Step == 0 && !(CurrentMap ~= Target()))
	{
		if (MapTime > 3.0)
		{
			EdgeLog("opening " $ Target());
			P.ConsoleCommand("open " $ Target());
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= Target()))
		Step = 0;

	T = P.Level.TimeSeconds;
	if (Step >= 1)
		EdgeLog("tick " $ T);

	// 1. A None event and a score that is not a number
	if (Step == 0 && T > 2.0)
	{
		NoneM = Maker(P, P.Location + vect(0,0,40), '');
		NoneL = Listener(P, P.Location + vect(40,0,40));
		NaNM = Maker(P, P.Location + vect(0,40,40), 'CapNaN');
		NaNL = Listener(P, P.Location + vect(-40,0,40));
		if (NoneM == None || NoneL == None || NaNM == None || NaNL == None)
		{
			Finish(P);
			return;
		}
		NoneL.Listen('');
		NaNL.Listen('CapNaN', 'NaNScore');
		EdgeLog("stage 1: " $ NoneL.Name $ " listens for None beside " $ NoneM.Name $ ", " $ NaNL.Name $ " for CapNaN scored NaN beside " $ NaNM.Name);
		Step = 1;
	}
	else if (Step == 1)
	{
		NoneM.Ping();
		NaNM.Ping();
		if (T > 2.5)
		{
			Tally("stage 1 None", NoneL);
			Tally("stage 1 NaN", NaNL);
			NoneM.Destroy();
			NaNM.Destroy();
			NoneL.Destroy();
			NaNL.Destroy();
			Step = 2;
		}
	}

	// 2. 256 senders, then 257
	else if (Step == 2)
	{
		CrowdL = Listener(P, P.Location + vect(-20,0,100));
		if (CrowdL == None)
		{
			Finish(P);
			return;
		}
		for (i = 0; i < 257; i++)
		{
			Crowd[i] = Maker(P, P.Location + vect(0,0,100) + vect(3,0,0) * i, 'CapCrowd');
			if (Crowd[i] == None)
			{
				Finish(P);
				return;
			}
		}
		CrowdL.Listen('CapCrowd');
		EdgeLog("stage 2: " $ CrowdL.Name $ " listens for CapCrowd 20 units before 256 makers, " $ Crowd[0].Name $ " the nearest; " $ Crowd[256].Name $ " the 257th from 2.85 s");
		Step = 3;
	}
	else if (Step == 3)
	{
		n = 256;
		if (T > 2.85)
			n = 257;
		for (i = 0; i < n; i++)
			Crowd[i].Ping();
		if (T > 3.1)
		{
			Tally("stage 2 crowd", CrowdL);
			for (i = 0; i < 257; i++)
				Crowd[i].Destroy();
			CrowdL.Destroy();
			Step = 4;
		}
	}

	// 3. The Entry level's manager
	else if (Step == 4)
	{
		Entry = P.GetEntryLevel();
		if (Entry == None)
		{
			EdgeLog("no Entry level");
			Finish(P);
			return;
		}
		EntryL = Listener(Entry, P.Location);
		HereM = P.Spawn(class'CapNoiseMaker',,, P.Location + vect(0,0,40));
		EntryM = Entry.Spawn(class'CapNoiseMaker',,, P.Location + vect(0,0,80));
		if (EntryL == None || HereM == None || EntryM == None)
		{
			EdgeLog("no room in stage 3");
			Finish(P);
			return;
		}
		EntryL.Listen('CapPing');
		EdgeLog("stage 3: " $ EntryL.Name $ " of " $ EntryL.Level $ " listens for CapPing, " $ HereM.Name $ " of " $ HereM.Level $ " 40 over it, " $ EntryM.Name $ " of " $ EntryM.Level $ " 80 over it from 3.5 s");
		Step = 5;
	}
	else if (Step == 5)
	{
		HereM.Ping();
		if (T > 3.5)
			EntryM.Ping();
		if (T > 3.8)
		{
			Tally("stage 3 Entry", EntryL);
			HereM.Destroy();
			EntryM.Destroy();
			EntryL.Destroy();
			Step = 6;
		}
	}

	// 4. Many slow listeners
	else if (Step == 6)
	{
		SlowCount = Slow;
		if (SlowCount <= 0)
			SlowCount = 60;
		if (SlowCount > 200)
			SlowCount = 200;
		SlowM = Maker(P, P.Location + vect(0,0,300), 'CapSlow');
		if (SlowM == None)
		{
			Finish(P);
			return;
		}
		for (i = 0; i < SlowCount; i++)
		{
			SlowL[i] = Listener(P, P.Location + vect(0,0,300) + vect(0,4,0) * (i + 1));
			if (SlowL[i] == None)
			{
				Finish(P);
				return;
			}
			SlowL[i].bQuiet = True;
			SlowL[i].BurnLoops = BurnLoops;
			if (BurnLoops <= 0)
				SlowL[i].BurnLoops = 20000;
			SlowL[i].Listen('CapSlow', 'SlowScore');
		}
		EdgeLog("stage 4: " $ SlowCount $ " listeners for CapSlow, " $ SlowL[0].BurnLoops $ " steps a score");
		Step = 7;
	}
	else if (Step == 7)
	{
		SlowM.Ping();
		if (T > 5.3)
		{
			n = 0;
			for (i = 0; i < SlowCount; i++)
			{
				n += SlowL[i].Calls;
				SlowL[i].Destroy();
			}
			EdgeLog("stage 4 slow calls " $ n);
			SlowM.Destroy();
			Step = 8;
		}
	}
	else if (Step == 8 && T > 5.6)
		Finish(P);
}
