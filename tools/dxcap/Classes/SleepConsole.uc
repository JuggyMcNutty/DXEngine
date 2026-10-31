//=============================================================================
// SleepConsole: a latent Sleep's time and its keeping across a save
// (docs/DEVELOPMENT.md, scripted runs). On Liberty Island, opened from
// wherever the run starts, 3 s in: two CapSleepers, one sleeping 0.105 s over
// and over -- 6.3 frames at 60 frames a second -- and one 10 s; 6 s in the
// game saved to slot 9 (a SaveHelper), 7 s in slot 9 loaded, and after the
// load -- a CapMarker placed after the save gone -- each sleeper's LatentFloat
// logged, and the 10 s one's waking; 12 s after the
// load an exit. Each line starts "DXSLEEP:"; run with DXCAP_FPS=60.
//=============================================================================
class SleepConsole extends Console;

var float MapTime, LoadedTime, Waited;
var string CurrentMap;
var int Phase;

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
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local CapSleeper Short, Long, S;
	local SaveHelper Helper;
	local CapMarker M;
	local bool bMarked;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		if (!(CurrentMap ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = -1;
			}
			return;
		}
		if (MapTime < 3.0)
			return;
		Short = P.Spawn(class'CapSleeper');
		Short.Seconds = 0.105;
		Short.GotoState('Sleeping');
		Long = P.Spawn(class'CapSleeper');
		Long.Seconds = 10.0;
		Long.GotoState('Sleeping');
		Log("DXSLEEP: " $ Short.Name $ " sleeps " $ Short.Seconds $ " and " $ Long.Name $ " " $ Long.Seconds $ " from " $ P.Level.TimeSeconds);
		Phase = 1;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
		{
			MapTime = 0;
			Phase = 0;
		}
	}
	else if (Phase == 1 && MapTime > 6.0)
	{
		Helper = P.Spawn(class'SaveHelper', P);
		Helper.Slot = 9;
		Helper.Description = "DXCapture sleep";
		Phase = 2;
	}
	else if (Phase == 2 && MapTime > 7.0)
	{
		foreach P.AllActors(class'CapSleeper', S)
			Log("DXSLEEP: before the load, at " $ P.Level.TimeSeconds $ ", " $ S.Name $ " woke " $ S.Wakes $ " times, LatentFloat " $ S.LatentFloat);
		P.Spawn(class'CapMarker');
		DeusExPlayer(P).LoadGame(9);
		Phase = 3;
	}
	else if (Phase == 3)
	{
		foreach P.AllActors(class'CapMarker', M)
			bMarked = true;
		if (bMarked)
		{
			Waited += Delta;
			if (Waited > 2.0)
			{
				Log("DXSLEEP: waiting for the load, at " $ P.Level.TimeSeconds);
				Waited = 0;
			}
			return;
		}
		foreach P.AllActors(class'CapSleeper', S)
			Log("DXSLEEP: after the load, at " $ P.Level.TimeSeconds $ ", " $ S.Name $ " sleeps " $ S.Seconds $ ", woke " $ S.Wakes $ " times, LatentFloat " $ S.LatentFloat);
		LoadedTime = MapTime;
		Phase = 4;
	}
	else if (Phase == 4 && MapTime - LoadedTime > 12.0)
	{
		Log("DXSLEEP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
