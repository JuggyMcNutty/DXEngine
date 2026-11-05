//=============================================================================
// DilationConsole: the level's time and an actor's ticks with the level's
// TimeDilation at 0, logged with "DXDILATION:".
//
// The original's ULevel::Tick (Engine.dll 0x103a51a0) adds the frame's time
// times TimeDilation to the level's time, and gives the actors that time
// held to 0.005..0.4 s (engine-dll.md, a level's tick): at 0 the level's time
// stands, each actor's tick 5 ms.
//
// From the menu map, 01_NYC_UNATCOIsland; 3 s in, TimeDilation 0 and the
// pawn first by name of those with a BurnPeriod set burning, its burnTimer 0
// and its BurnPeriod 1000, so that burnTimer sums its ticks' times (its
// native Tick adds each). Half a second later (or 30 console ticks, whichever
// is first) the level's time before and after and the pawn's burnTimer
// logged, then TimeDilation 1 again, the fire put out and an exit. Run
// DXEngine's from the menu map, as the original's.
//=============================================================================
class DilationConsole extends Console;

var float MapTime;
var int Phase;
var string CurrentMap;
var ScriptedPawn Paul;
var int Ticks;
var float HeldAt;

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

event Tick(float Delta)
{
	local PlayerPawn P;
	local ScriptedPawn S;
	local string M;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	M = MapOf(P);
	if (M != CurrentMap)
	{
		Log("DXCAP: now in " $ M);
		CurrentMap = M;
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0 && MapTime > 3.0)
	{
		Log("DXCAP: opening 01_NYC_UNATCOIsland");
		P.ConsoleCommand("open 01_NYC_UNATCOIsland");
		Phase = 1;
	}
	else if (Phase == 1 && M ~= "01_NYC_UNATCOIsland" && MapTime > 3.0)
	{
		foreach P.AllActors(class'ScriptedPawn', S)
			if (S.BurnPeriod > 0 && (Paul == None || string(S.Name) < string(Paul.Name)))
				Paul = S;
		if (Paul == None)
		{
			Log("DXDILATION: no pawn with a BurnPeriod");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		P.Level.TimeDilation = 0.0;
		Paul.BurnPeriod = 1000;
		Paul.bOnFire = True;
		Paul.burnTimer = 0;
		HeldAt = P.Level.TimeSeconds;
		Log("DXDILATION: TimeDilation 0, " $ Paul.Name $ " burning from 0"); 
		MapTime = 0;
		Ticks = 0;
		Phase = 2;
	}
	else if (Phase == 2 && (MapTime > 0.5 || ++Ticks > 30))
	{
		Log("DXDILATION: half a second on, the level's time on by " $ (P.Level.TimeSeconds - HeldAt) $ ", the pawn's ticks' time " $ Paul.burnTimer);
		P.Level.TimeDilation = 1.0;
		if (Paul.bOnFire)
			Paul.ExtinguishFire();
		Log("DXDILATION: done");
		P.ConsoleCommand("exit");
		Phase = 3;
	}
}
