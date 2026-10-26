//=============================================================================
// ScoutConsole: the pawn generators' scouts and their zones, logged with
// "DXSCOUT:" from wherever the run starts (docs/DEVELOPMENT.md, scripted
// runs; bugs.md E5b.N1). 2 s in, and again 3 s later, each PawnGenerator
// with its location, zone and physics, and its scout's location, zone,
// physics, velocity and collision; at 2 s also a marker actor (a
// CapExtender) set by SetLocation at each generator's spot, 16 units under
// it and 16 over it, its zone read after each; a GeneratorScout and a
// marker spawned at each of the original's four FlyGenerator spots; and the
// FlyGenerator objects left, destroyed or not (bugs.md E5b.N2: DXEngine has
// none). Then an exit. The original has Liberty Island's five scouts flying
// still at their generators, the fish's in zone 1, its water; DXEngine once
// had their locations NaN, the first frame being 0 s (bugs.md E5b.N1).
//=============================================================================
class ScoutConsole extends Console;

var float RunTime;
var int Phase;

function Census(PlayerPawn P, string When)
{
	local PawnGenerator G;

	foreach P.AllActors(class'PawnGenerator', G)
	{
		Log("DXSCOUT: " $ When $ ": " $ G.Name $ " at " $ G.Location $ " zone " $ G.Region.ZoneNumber $ " (" $ G.Region.Zone.Class.Name $ "), physics " $ G.Physics $ ", ground " $ G.GroundLocation);
		if (G.Scout == None)
			Log("DXSCOUT: " $ When $ ": " $ G.Name $ " has no scout");
		else
			Log("DXSCOUT: " $ When $ ": " $ G.Name $ "'s scout at " $ G.Scout.Location $ " zone " $ G.Scout.Region.ZoneNumber $ " (" $ G.Scout.Region.Zone.Class.Name $ "), physics " $ G.Scout.Physics $ ", velocity " $ G.Scout.Velocity $ ", collide world " $ G.Scout.bCollideWorld $ ", radius " $ G.Scout.CollisionRadius $ ", height " $ G.Scout.CollisionHeight $ ", hidden " $ G.Scout.bHidden);
	}
}

function Markers(PlayerPawn P)
{
	local PawnGenerator G;
	local CapExtender M;

	M = P.Spawn(class'CapExtender');
	if (M == None)
	{
		Log("DXSCOUT: no marker");
		return;
	}
	foreach P.AllActors(class'PawnGenerator', G)
	{
		M.SetLocation(G.Location);
		Log("DXSCOUT: a marker at " $ G.Name $ "'s spot: zone " $ M.Region.ZoneNumber $ " (" $ M.Region.Zone.Class.Name $ ")");
		M.SetLocation(G.Location - vect(0,0,16));
		Log("DXSCOUT: a marker 16 under " $ G.Name $ ": zone " $ M.Region.ZoneNumber);
		M.SetLocation(G.Location + vect(0,0,16));
		Log("DXSCOUT: a marker 16 over " $ G.Name $ ": zone " $ M.Region.ZoneNumber);
	}
	M.Destroy();
}

// A scout spawned at each FlyGenerator's spot, as the original's log has
// them (DXEngine's FlyGenerators are gone by 2 s: a generator whose scout
// fails to spawn destroys itself), and a marker there
function SpawnTests(PlayerPawn P)
{
	local vector Spots[4];
	local int i;
	local GeneratorScout S;
	local CapExtender M;

	Spots[0] = vect(-3880.807617,10100.102539,-277.140381);
	Spots[1] = vect(1299.726318,-1755.369019,490.869995);
	Spots[2] = vect(-2389.856934,86.097374,-117.140076);
	Spots[3] = vect(2220.493408,-2174.030518,-149.140366);
	for (i = 0; i < 4; i++)
	{
		S = P.Spawn(class'GeneratorScout',,, Spots[i]);
		if (S == None)
			Log("DXSCOUT: a GeneratorScout at FlyGenerator" $ i $ "'s spot: none spawned");
		else
		{
			Log("DXSCOUT: a GeneratorScout at FlyGenerator" $ i $ "'s spot: at " $ S.Location $ " zone " $ S.Region.ZoneNumber $ ", radius " $ S.CollisionRadius $ ", height " $ S.CollisionHeight $ ", collide world " $ S.bCollideWorld $ ", collide actors " $ S.bCollideActors);
			S.Destroy();
		}
		M = P.Spawn(class'CapExtender',,, Spots[i]);
		if (M == None)
			Log("DXSCOUT: a marker at FlyGenerator" $ i $ "'s spot: none spawned");
		else
		{
			Log("DXSCOUT: a marker at FlyGenerator" $ i $ "'s spot: zone " $ M.Region.ZoneNumber);
			M.Destroy();
		}
	}
}

// The FlyGenerators as objects, destroyed or not (AllObjects still walks a
// destroyed actor until it is collected): which of a PawnGenerator's own
// ends took them in DXEngine -- no pawn classes (PostBeginPlay), no scout
// (Tick), or dying with no pawns (a stop)
function Gone(PlayerPawn P)
{
	local FlyGenerator G;
	local int n;

	foreach AllObjects(class'FlyGenerator', G)
	{
		n++;
		Log("DXSCOUT: " $ G.Name $ ": deleted " $ G.bDeleteMe $ ", dying " $ G.bDying $ ", active " $ G.bActive $ ", pawns " $ G.PawnCount $ ", scout " $ G.Scout $ ", first class " $ G.PawnClasses[0].PawnClass $ " x " $ G.PawnClasses[0].Count $ ", zone " $ G.Region.ZoneNumber $ ", at " $ G.Location);
	}
	Log("DXSCOUT: FlyGenerator objects: " $ n);
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	P.ReducedDamageType = 'All';
	if (Phase == 0 && RunTime > 2.0)
	{
		Census(P, "at 2 s");
		Markers(P);
		SpawnTests(P);
		Gone(P);
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 5.0)
	{
		Census(P, "at 5 s");
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 5.5)
	{
		Log("DXSCOUT: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
