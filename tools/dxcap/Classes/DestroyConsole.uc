//=============================================================================
// DestroyConsole: an actor's destruction, event by event -- Liberty Island
// opened from wherever the run starts; 2 s into the level's time, CapDoomeds
// spawned 200 units over the player: Doomed, in state Waiting, owned by
// Owner, standing on Platform, with Rider standing on it, Child owned by it
// and Toucher touching it. Doomed is destroyed; each event is logged by the
// actor taking it (CapDoomed), then what the others are left with; an exit.
//=============================================================================
class DestroyConsole extends Console;

var float MapTime;
var int Step;
var string CurrentMap;

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

function CapDoomed Make(PlayerPawn P, name T, vector Spot)
{
	local CapDoomed D;

	D = P.Spawn(class'CapDoomed',, T, Spot);
	if (D == None)
		Log("DXDESTROY: no room for " $ T);
	return D;
}

function string TagOf(Actor A)
{
	if (A == None)
		return "None";
	return string(A.Tag);
}

function Run(PlayerPawn P)
{
	local CapDoomed Doomed, Owner, Platform, Rider, Child, Toucher;
	local vector Spot;
	local bool bResult;

	Spot = P.Location + vect(0,0,200);
	Doomed = Make(P, 'Doomed', Spot);
	Owner = Make(P, 'Owner', Spot + vect(100,0,0));
	Platform = Make(P, 'Platform', Spot + vect(0,100,0));
	Rider = Make(P, 'Rider', Spot + vect(0,-100,0));
	Child = Make(P, 'Child', Spot + vect(-100,0,0));
	Toucher = Make(P, 'Toucher', Spot + vect(0,0,100));
	if (Doomed == None || Owner == None || Platform == None || Rider == None || Child == None || Toucher == None)
		return;

	Doomed.SetOwner(Owner);
	Child.SetOwner(Doomed);
	Doomed.SetBase(Platform);
	Rider.SetBase(Doomed);
	Doomed.GotoState('Waiting');
	Doomed.SetCollisionSize(40, 40);
	Toucher.SetCollisionSize(40, 40);
	Doomed.SetCollision(true, false, false);
	Toucher.SetCollision(true, false, false);
	Toucher.SetLocation(Doomed.Location);

	Log("DXDESTROY: destroying");
	bResult = Doomed.Destroy();
	Log("DXDESTROY: destroy returned " $ bResult);
	Log("DXDESTROY: rider base " $ TagOf(Rider.Base) $ ", child owner " $ TagOf(Child.Owner)
		$ ", toucher touching " $ TagOf(Toucher.Touching[0]) $ TagOf(Toucher.Touching[1]));
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

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0)
		{
			Log("DXDESTROY: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && P.Level.TimeSeconds > 2.0)
	{
		Run(P);
		Step = 1;
	}
	else if (Step == 1 && P.Level.TimeSeconds > 3.0)
	{
		Log("DXDESTROY: exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
}
