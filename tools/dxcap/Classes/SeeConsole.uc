//=============================================================================
// SeeConsole: the pawns' sight in the tick -- Liberty Island opened from
// wherever the run starts; 2 s into the level's time, two CapWatchers spawned
// 300 units in front of the player, 200 apart, facing it: A ignoring
// EnemyNotVisible, B probing it with the player as its enemy. Each SeePlayer
// and EnemyNotVisible is logged by the watcher; each tick for 3 s, both
// watchers' SightCounter; at 8 s an exit.
//=============================================================================
class SeeConsole extends Console;

var float MapTime;
var int Step;
var string CurrentMap;
var CapWatcher A, B;

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

function CapWatcher Watcher(PlayerPawn P, vector Spot, bool bProbeEnemy)
{
	local CapWatcher W;

	W = P.Spawn(class'CapWatcher',,, Spot, rotator(P.Location - Spot));
	if (W == None)
	{
		Log("DXSEE: no room for a watcher at " $ Spot);
		return None;
	}
	W.InitializePawn();
	W.bInvincible = true;
	W.bProbeEnemy = bProbeEnemy;
	if (bProbeEnemy)
		W.Enemy = P;
	W.GotoState('CapWatching');
	Log("DXSEE: " $ W.Name $ " probes EnemyNotVisible " $ bProbeEnemy $ " at " $ int(VSize(W.Location - P.Location)));
	return W;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local vector X, Y, Z;

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

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0)
		{
			Log("DXSEE: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && P.Level.TimeSeconds > 2.0)
	{
		GetAxes(P.Rotation, X, Y, Z);
		X.Z = 0;
		X = Normal(X);
		A = Watcher(P, P.Location + X * 300 - Y * 100, false);
		B = Watcher(P, P.Location + X * 300 + Y * 100, true);
		Step = 1;
	}
	else if (Step == 1)
	{
		if (A != None && B != None && P.Level.TimeSeconds < 5.0)
			Log("DXSEE: counters " $ A.SightCounter $ " " $ B.SightCounter);
		if (P.Level.TimeSeconds > 8.0)
		{
			Log("DXSEE: exiting");
			P.ConsoleCommand("exit");
			Step = 2;
		}
	}
}
