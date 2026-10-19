//=============================================================================
// MoveConsole: how the level's animals and bots move -- every in-world
// ScriptedPawn's state, orders, move target and destination logged with
// "DXMOVE:" every two seconds, with the distance it moved since the last
// line, so a pawn that stands still in a moving state (a patrol that never
// leaves its node, a wanderer that never picks a spot) shows as a run of
// moved-0 lines beside the original's moving ones (docs/DEVELOPMENT.md,
// scripted runs).
//
// On Liberty Island, 120 s of the level's own time, the player left alone
// at its start. What is compared between the engines: which pawns move at
// all, which states they pass through, what they chase (move target,
// destination) and how far they get -- the animals (the rats and the
// gulls) and the UNATCO security bots among them.
//
// With Watch (DXCAP_CONFIG "Watch=Terrorist34"), that pawn every tick too,
// from WatchFrom to WatchTo of the level's seconds (all the 120 s by
// default): the level's time and the tick's length, its place, velocity,
// physics, base, state, move target, MoveTimer and destination, and the
// nearest other actor that collides within 80 units, with "DXMOVE: watch";
// move.py --pawn finds where two runs' paths part.
//=============================================================================
class MoveConsole extends Console;

var config string Watch;
var config float WatchFrom, WatchTo;
var float WatchedAt;

var float CapTime;
var float MapTime;
var float StepTime;
var float LogTime;
var string CurrentMap;
var int Phase;

// Each watched pawn's place at the last line, for the distance moved.
var ScriptedPawn Watched[64];
var vector LastPos[64];
var int NumWatched;

function string MapOf(PlayerPawn P)
{
	local string URL;
	local int i;
	URL = P.Level.GetLocalURL();
	i = InStr(URL, "/");
	while (i >= 0)
	{
		URL = Mid(URL, i + 1);
		i = InStr(URL, "/");
	}
	i = InStr(URL, ".");
	if (i >= 0)
		URL = Left(URL, i);
	i = InStr(URL, "?");
	if (i >= 0)
		URL = Left(URL, i);
	return URL;
}

function Travel(PlayerPawn P, string Map)
{
	Log("DXCAP: opening " $ Map);
	P.ConsoleCommand("open " $ Map);
}

// One line per in-world scripted pawn: what it is doing and where it is
// going, with the distance moved since the last line.
function Census(PlayerPawn P)
{
	local ScriptedPawn S;
	local int i, Count;
	local string MT;

	foreach P.AllActors(class'ScriptedPawn', S)
	{
		// The level keeps pawns out of the world, hidden; they are not
		// moving and would only pad the log.
		if (!S.bInWorld || S.bHidden)
			continue;
		Count++;

		for (i = 0; i < NumWatched; i++)
			if (Watched[i] == S)
				break;
		if (i == NumWatched)
		{
			if (NumWatched < ArrayCount(Watched))
			{
				i = NumWatched++;
				Watched[i] = S;
				LastPos[i] = S.Location;
			}
			else
				i = -1;
		}
		if (i < 0)
			continue;

		MT = "None";
		if (S.MoveTarget != None)
			MT = string(S.MoveTarget.Name);
		Log("DXMOVE: " $ int(P.Level.TimeSeconds) $ " " $ S.Name $ " " $ S.Class.Name
		    $ " state " $ S.GetStateName()
		    $ " orders " $ S.Orders
		    $ " mt " $ MT
		    $ " dest " $ int(S.Destination.X) $ "," $ int(S.Destination.Y) $ "," $ int(S.Destination.Z)
		    $ " vel " $ int(VSize(S.Velocity))
		    $ " at " $ int(S.Location.X) $ "," $ int(S.Location.Y) $ "," $ int(S.Location.Z)
		    $ " moved " $ int(VSize(S.Location - LastPos[i])));
		LastPos[i] = S.Location;
	}
	Log("DXMOVE: " $ int(P.Level.TimeSeconds) $ " " $ Count $ " in-world pawns");
}

// The watched pawn this tick, as it stands.
function WatchTick(PlayerPawn P, float Delta)
{
	local ScriptedPawn S;
	local Actor A, Near;
	local string MT, B, N;
	local float T, D, NearD;

	T = P.Level.TimeSeconds;
	if (T < WatchFrom || (WatchTo > 0 && T > WatchTo))
		return;
	foreach P.AllActors(class'ScriptedPawn', S)
		if (string(S.Name) ~= Watch)
			break;
	if (S == None || !(string(S.Name) ~= Watch))
		return;
	MT = "None";
	if (S.MoveTarget != None)
		MT = string(S.MoveTarget.Name);
	B = "None";
	if (S.Base != None)
		B = string(S.Base.Name);
	NearD = 80;
	foreach S.RadiusActors(class'Actor', A, 80)
	{
		D = VSize(A.Location - S.Location);
		if (A != S && A.bCollideActors && D < NearD)
		{
			Near = A;
			NearD = D;
		}
	}
	N = "None";
	if (Near != None)
		N = Near.Name $ " at " $ Near.Location;
	Log("DXMOVE: watch " $ T $ " dt " $ (T - WatchedAt) $ " " $ Delta $ " " $ S.Name
	    $ " at " $ S.Location $ " vel " $ S.Velocity $ " physics " $ int(S.Physics) $ " base " $ B
	    $ " state " $ S.GetStateName() $ " mt " $ MT $ " movetimer " $ S.MoveTimer $ " dest " $ S.Destination
	    $ " near " $ N);
	WatchedAt = T;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string M;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	CapTime += Delta;
	M = MapOf(P);
	if (M != CurrentMap)
	{
		Log("DXCAP: now in " $ M $ " at " $ CapTime);
		CurrentMap = M;
		MapTime = 0;
		StepTime = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	LogTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				Travel(P, "01_NYC_UNATCOIsland");
				Phase = 1;
			}
		}
		else
			Phase = 1;
	}
	else if (Phase == 1)
	{
		// Two seconds of quiet, then a line every two seconds for two
		// minutes.
		if (StepTime > 2.0)
		{
			Census(P);
			LogTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		if (Watch != "")
			WatchTick(P, Delta);
		if (LogTime > 2.0)
		{
			Census(P);
			LogTime = 0;
		}
		if (MapTime > 122.0)
		{
			Log("DXCAP: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 3;
		}
	}
}
