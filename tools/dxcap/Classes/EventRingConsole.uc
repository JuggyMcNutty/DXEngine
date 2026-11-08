//=============================================================================
// EventRingConsole: the AI event manager's ring, and a sender pending kill.
// The map is the ini's TargetMap ([DXCapture.EventRingConsole]), Liberty
// Island by default, opened from wherever the run starts. 2 s into its time
// a CapNoiseMaker is spawned 40 units over the player and four CapListeners
// 40 units round it, A, B, C and D; A is given D as its Partner, and A, B
// and C are made to listen for CapPing, in that order. From then on, each
// console tick, the maker raises a CapPing pulse (AISendEvent, audio, 1,
// 4000), which each listener hears at the level's next tick: a Pulse call.
// A's first call has D listen, from within the manager's calls.
//
// The original's manager links a listener made at any time before the
// receiver its walk began at (AISetEventCallback, Engine.dll 0x10382990),
// which during the calls is still this frame's first, and starts the next
// walk at the receiver after the last that had its turn (AIProcess
// 0x10384080, its end 0x1038478e): after a full pass, the first one made
// during the calls. So from the tick after D's first listen, each tick's
// calls run D, A, B, C, in both engines (UEventManager::Tick).
// (A walk the original's 2 ms cut ends early resumes elsewhere: should the
// original's ticks alternate, run it on a quieter TargetMap.)
//
// At 3.5 s the pulses stop; at 4 s the maker destroys itself and raises one
// more pulse after its Destroy. The original's AISendEvent gives a sender
// pending kill nothing (0x10382c60): no call at the tick after, in both
// engines. Each console tick, which
// comes between the level's ticks, logs "DXEVTRING: tick" with the level's
// time; at 5 s an exit.
//
// With SaveAt (DXCAP_CONFIG "SaveAt=3.7") the game is saved to slot 9 at
// that level time, after the pulses stop and before the maker goes (by a
// SaveHelper, on the next actors' tick): aisave.py on the slot's
// 01_NYC_UNATCOIsland.dxs with the prefix CapListener lists the listeners'
// receivers. The original sets a receiver's best score and sender at every
// turn (AIProcess 0x1038445f-0x1038446b), so after its last turns with
// nothing heard each has bestScore 0 and bestSender None, and keeps its last
// call's state, 2 (Pulse), in both engines.
//=============================================================================
class EventRingConsole extends Console;

var config string TargetMap;
var config float SaveAt;

var bool bSaved;

var float MapTime;
var int Step;
var string CurrentMap;
var CapListener A, B, C, D;
var CapNoiseMaker M;

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

function string Target()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOIsland";
	return TargetMap;
}

function CapListener Listener(PlayerPawn P, vector Spot)
{
	local CapListener L;

	L = P.Spawn(class'CapListener',,, Spot);
	if (L == None)
		Log("DXEVTRING: no room for a listener at " $ Spot);
	else
		Log("DXEVTRING: " $ L.Name $ " spawned " $ int(VSize(L.Location - M.Location)) $ " from the maker");
	return L;
}

// The save as SaveConsole makes it: from an actor's tick, slot 9.
function SaveSlot(PlayerPawn P)
{
	local SaveHelper S;

	S = P.Spawn(class'SaveHelper', P);
	if (S == None)
	{
		Log("DXEVTRING: no save helper");
	}
	else
	{
		S.Slot = 9;
		S.Description = "EventRingConsole";
		Log("DXEVTRING: saving to slot 9 at " $ P.Level.TimeSeconds);
	}
	bSaved = True;
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
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
			Log("DXEVTRING: opening " $ Target());
			P.ConsoleCommand("open " $ Target());
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= Target()))
		Step = 0;

	if (Step == 0 && P.Level.TimeSeconds > 2.0)
	{
		M = P.Spawn(class'CapNoiseMaker',,, P.Location + vect(0,0,40));
		if (M == None)
		{
			Log("DXEVTRING: no room for the noise maker, exiting");
			P.ConsoleCommand("exit");
			Step = 9;
			return;
		}
		A = Listener(P, M.Location + vect(40,0,0));
		B = Listener(P, M.Location + vect(0,40,0));
		C = Listener(P, M.Location + vect(-40,0,0));
		D = Listener(P, M.Location + vect(0,-40,0));
		if (A == None || B == None || C == None || D == None)
		{
			Log("DXEVTRING: exiting");
			P.ConsoleCommand("exit");
			Step = 9;
			return;
		}
		A.Partner = D;
		A.Listen();
		B.Listen();
		C.Listen();
		Log("DXEVTRING: " $ A.Name $ ", " $ B.Name $ " and " $ C.Name $ " listen; " $ D.Name $ " is " $ A.Name $ "'s partner");
		Step = 1;
	}

	if (Step >= 1 && Step <= 3)
		Log("DXEVTRING: tick " $ P.Level.TimeSeconds);

	if (Step == 1)
	{
		M.Ping();
		if (P.Level.TimeSeconds > 3.5)
		{
			Log("DXEVTRING: pulses stopped");
			Step = 2;
		}
	}
	else if (Step == 2)
	{
		if (SaveAt > 0 && !bSaved && P.Level.TimeSeconds > SaveAt)
			SaveSlot(P);
		if (P.Level.TimeSeconds > 4.0 && (SaveAt <= 0 || bSaved))
		{
			M.Vanish();
			Step = 3;
		}
	}
	else if (Step == 3 && P.Level.TimeSeconds > 5.0)
	{
		Log("DXEVTRING: exiting");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
