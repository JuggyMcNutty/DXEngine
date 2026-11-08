//=============================================================================
// EncroachMoverConsole (bugs.md PHYS.30): what a spawn or a SetLocation
// against a closed door is asked -- TargetMap ([DXCapture.
// EncroachMoverConsole], 01_NYC_UNATCOIsland by default) opened from
// wherever the run starts; 2 s into the level's time the first closed door
// in the level's order (a DeusExMover with bIsDoor at key 0), its box and
// its thinner side; a level line from 32 units out at the eyes' height to
// the door's middle, as DeusExWeapon.NearWallCheck traces a grenade's place;
// then, each logged as spawned or not and where, each spawned one destroyed
// at once:
//   - a LAM, an EMPGrenade, a GasGrenade and a NanoVirusGrenade at the line's
//     hit, turned to its normal, as PlaceGrenade spawns them;
//   - a Fireball and a Dart one unit out from the hit;
//   - a CapReacher with its side 5 units into the door, and one 40 units
//     clear of it;
//   - the clear CapReacher SetLocation'd to the spot 5 units in: the answer
//     and where it stands;
//   - each of the first three again 60 units out, clear of everything.
// The original's encroachment check (ULevel::CheckEncroachment 0x1039a350)
// lists only what FCollisionHash::ActorEncroachmentCheck (0x10358d50) gives,
// which passes over every brush that is not static (0x10358ee6-0x10358f16):
// no mover is ever asked about. DXEngine's lists movers by their brushes
// (OverlapTester::EncroachedActors), and a Projectile's and a Pawn's
// EncroachingOn answer True for a brush, so its spawn is destroyed: the
// grenades, the Fireball and the CapReacher 5 units in should be None in
// DXEngine and spawned in the original. Each line starts "DXENCROACH:".
//=============================================================================
class EncroachMoverConsole extends Console;

var config string TargetMap;
var config string DoorName;

var float MapTime;
var int Step;
var string CurrentMap;

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
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

function Report(string What, Actor A, vector Spot)
{
	if (A == None)
		Log("DXENCROACH: " $ What $ " at " $ Spot $ ": none");
	else
	{
		Log("DXENCROACH: " $ What $ " at " $ Spot $ ": spawned at " $ A.Location $ " size " $ A.CollisionRadius $ "," $ A.CollisionHeight);
		A.Destroy();
	}
}

function Run(PlayerPawn P)
{
	local DeusExMover M, Door;
	local vector Min, Max, C, E, N, Start, End, HitLoc, HitNorm, Spot;
	local float Thick;
	local Actor Hit, A;
	local CapReacher R;
	local bool bMoved;

	foreach P.AllActors(class'DeusExMover', M)
	{
		if (DoorName != "" && !(string(M.Name) ~= DoorName))
			continue;
		if (!M.bIsDoor || M.KeyNum != 0)
			continue;
		if (!M.GetBoundingBox(Min, Max))
			continue;
		// A door stands: thinnest across X or Y, not a hatch lying flat
		E = (Max - Min) * 0.5;
		if (DoorName == "" && E.Z <= FMin(E.X, E.Y))
			continue;
		Door = M;
		break;
	}
	if (Door == None)
	{
		Log("DXENCROACH: no closed door");
		return;
	}
	C = (Min + Max) * 0.5;
	E = (Max - Min) * 0.5;
	if (E.X <= E.Y)
	{
		N = vect(1,0,0);
		Thick = E.X;
	}
	else
	{
		N = vect(0,1,0);
		Thick = E.Y;
	}
	Log("DXENCROACH: door " $ Door.Name $ " middle " $ C $ " half " $ E $ " axis " $ N
		$ " collide " $ Door.bCollideActors $ " blockactors " $ Door.bBlockActors $ " blockplayers " $ Door.bBlockPlayers);

	// NearWallCheck's line: 32 units, at the eyes' height, toward the door
	Start = C + N * (Thick + 32);
	End = C;
	Hit = P.Trace(HitLoc, HitNorm, End, Start);
	Log("DXENCROACH: the wall line meets " $ Hit $ " at " $ HitLoc $ " normal " $ HitNorm);
	if (Hit == None)
		return;

	Report("LAM", P.Spawn(class'LAM', P,, HitLoc, rotator(HitNorm)), HitLoc);
	Report("EMPGrenade", P.Spawn(class'EMPGrenade', P,, HitLoc, rotator(HitNorm)), HitLoc);
	Report("GasGrenade", P.Spawn(class'GasGrenade', P,, HitLoc, rotator(HitNorm)), HitLoc);
	Report("NanoVirusGrenade", P.Spawn(class'NanoVirusGrenade', P,, HitLoc, rotator(HitNorm)), HitLoc);
	Spot = HitLoc + HitNorm;
	Report("Fireball", P.Spawn(class'Fireball', P,, Spot, rotator(-HitNorm)), Spot);
	Report("Dart", P.Spawn(class'Dart', P,, Spot, rotator(-HitNorm)), Spot);

	Spot = C + N * (Thick + 20 - 5);
	Report("CapReacher 5 in", P.Spawn(class'CapReacher',,, Spot), Spot);

	Spot = C + N * (Thick + 60);
	R = P.Spawn(class'CapReacher',,, Spot);
	if (R == None)
		Log("DXENCROACH: no clear CapReacher at " $ Spot);
	else
	{
		Spot = C + N * (Thick + 20 - 5);
		bMoved = R.SetLocation(Spot);
		Log("DXENCROACH: SetLocation 5 in to " $ Spot $ ": " $ bMoved $ ", at " $ R.Location);
		R.Destroy();
	}

	Spot = C + N * (Thick + 60);
	Report("LAM clear", P.Spawn(class'LAM', P,, Spot, rotator(N)), Spot);
	Report("Fireball clear", P.Spawn(class'Fireball', P,, Spot, rotator(-N)), Spot);
	Report("CapReacher clear", P.Spawn(class'CapReacher',,, Spot), Spot);
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
			Log("DXENCROACH: opening " $ Target());
			P.ConsoleCommand("open " $ Target());
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= Target()))
		Step = 0;

	if (Step == 0 && P.Level.TimeSeconds > 2.0)
	{
		Run(P);
		MapTime = 0;
		Step = 1;
	}
	else if (Step == 1 && MapTime > 1.0)
	{
		Log("DXENCROACH: exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
}
