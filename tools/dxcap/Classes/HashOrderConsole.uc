//=============================================================================
// HashOrderConsole (bugs.md PHYS.36, PHYS.39): the order the collision hash
// gives actors, logged with "DXHASH:" -- TargetMap ([DXCapture.
// HashOrderConsole], 01_NYC_UNATCOIsland by default) opened from wherever the
// run starts; 2 s into the level's time, over the player, Q the middle of a
// 256-unit square whose corners lie on multiples of 256 (on Liberty Island
// x -4736, which the original's hash, its cells' middles on multiples of
// 256, takes for a cell's bound):
//   A, B and C, CapHashMarks spawned in that order 20 units apart along X, 40
//   under a multiple of 256 in Z; D 40 past the next multiple in X; F 40 over
//   the one in Z. Then:
//   1. foreach VisibleCollidingActors(class'CapHashMark', M, 400, Q): each
//      mark as given -- HurtRadius's order of its victims;
//   2. a CapHashMover, Mover, spawned 3000 units off, SetLocation'd onto B:
//      each touch as made; then Mover's TouchingActors, its slots' order;
//   3. A SetLocation'd a unit along X and back (filed in the hash again),
//      Mover sent off and onto B again: its untouches, its touches, its
//      slots; VisibleCollidingActors again;
//   4. 600 units along Y: Rider, a mark 20 over Mover2 and based on it,
//      Mover2 SetLocation'd 10 up so that Rider overlaps it there; Mover3,
//      based on the mark Base 40 under it, SetLocation'd 10 over Base.
// Each mark's FastTrace from Q is logged, VisibleCollidingActors needing it.
//
// The original (Engine.dll): a coordinate's cell is it plus 32768 over 256,
// rounded to the nearest whole, a half to even (GetHashIndices
// 0x10357590), so -4736 is a bound: A lies in cell 109, C and D in 110, B
// and F in both, all five in one Z cell; an actor is filed in the cells of
// its cylinder a unit wider and taller each way (0x103d8460); the hash walks
// its cells X outermost, then Y, then Z (FCollisionHash::ActorRadiusCheck
// 0x103589e0, ActorEncroachmentCheck 0x10358d50), each cell's chain newest
// first (AddActor 0x103580b0 puts each link at its bucket's head, and every
// FarMoveActor files the actor again, 0x10398ba0); the radius check puts each
// find at its list's head, so its list comes out reversed, the encroachment
// check at its tail; touches fill the last free slot (0x10369630); and
// CheckEncroachment touches whatever does not block it at the spot
// (0x1039a350), based or not. The original's run: 1. C D A B F; 2. touches
// B, A, C, slots C A B; 3. untouches C, A, B, touches A, B, C, slots C B A,
// the radius C D B F A; 4. "Mover2 touch Rider", "Rider touch Mover2", then
// Rider's basechange, and "Mover3 touch Base", "Base touch Mover3".
// DXEngine before its fix (bugs.md PHYS.36): cells floored from multiples of
// 256, Z outermost, each cell oldest first, every list in the order found:
// 1. A B C D F; 2. A, B, C and C B A; 3. C, B, A, then B, C, A and A C B,
// the radius B C A D F; 4. as the original's.
//=============================================================================
class HashOrderConsole extends Console;

var config string TargetMap;
// A spot to take for the player's own place when the cells over the player
// are not open air (the FastTrace line logs it): all three set to use it
var config float SpotX, SpotY, SpotZ;

var float MapTime;
var int Step;
var string CurrentMap;

function Say(string S)
{
	Log("DXHASH: " $ S);
}

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

// The cell bound at or under V: 256 x floor(V / 256)
function float CellFloor(float V)
{
	local int i;

	i = int(V / 256.0);
	if (float(i) * 256.0 > V)
		i--;
	return float(i) * 256.0;
}

function CapHashMark Mark(PlayerPawn P, name T, vector At)
{
	local CapHashMark M;

	M = P.Spawn(class'CapHashMark',, T, At);
	if (M == None)
		Say("no mark " $ T $ " at " $ At);
	return M;
}

function Radius(PlayerPawn P, vector Q, string What)
{
	local CapHashMark M;
	local string S;

	foreach P.VisibleCollidingActors(class'CapHashMark', M, 400, Q)
		S = S $ " " $ M.Tag;
	Say(What $ " VisibleCollidingActors:" $ S);
}

function Slots(CapHashMover Mover, string What)
{
	local Actor A;
	local string S;

	foreach Mover.TouchingActors(class'Actor', A)
		S = S $ " " $ A.Tag;
	Say(What $ " " $ Mover.Tag $ " TouchingActors:" $ S);
}

function Run(PlayerPawn P)
{
	local vector Q, Far, S;
	local float bx, by, zb;
	local CapHashMark A, B, C, D, F, Rider, BaseMark;
	local CapHashMover Mover, Mover2, Mover3;
	local bool bMoved;
	local vector At;

	At = P.Location;
	if (SpotX != 0 && SpotY != 0 && SpotZ != 0)
	{
		At.X = SpotX;
		At.Y = SpotY;
		At.Z = SpotZ;
	}
	bx = CellFloor(At.X);
	by = CellFloor(At.Y);
	zb = CellFloor(At.Z + 100) + 256;
	Q.X = bx + 128;
	Q.Y = by + 128;
	Q.Z = zb - 40;
	Say("player at " $ P.Location $ ", Q " $ Q $ ", X bound " $ (bx + 256) $ ", Z bound " $ zb);

	A = Mark(P, 'A', Q - vect(20, 0, 0));
	B = Mark(P, 'B', Q);
	C = Mark(P, 'C', Q + vect(20, 0, 0));
	S = Q;
	S.X = bx + 256 + 40;
	D = Mark(P, 'D', S);
	F = Mark(P, 'F', Q + vect(0, 0, 80));
	if (A == None || B == None || C == None || D == None || F == None)
		return;
	Say("clear lines from Q: A " $ P.FastTrace(A.Location, Q) $ " B " $ P.FastTrace(B.Location, Q)
		$ " C " $ P.FastTrace(C.Location, Q) $ " D " $ P.FastTrace(D.Location, Q) $ " F " $ P.FastTrace(F.Location, Q));

	// 1. the radius check's order
	Radius(P, Q, "1.");

	// 2. FarMoveActor's touches, as found at the spot
	Far = Q + vect(3000, 0, 0);
	Mover = P.Spawn(class'CapHashMover',, 'Mover', Far);
	if (Mover == None)
	{
		Say("no Mover");
		return;
	}
	Say("2. Mover onto B");
	bMoved = Mover.SetLocation(B.Location);
	Say("2. moved " $ bMoved $ " to " $ Mover.Location);
	Slots(Mover, "2.");

	// 3. A filed again; Mover off and onto B again
	A.SetLocation(A.Location + vect(1, 0, 0));
	A.SetLocation(A.Location - vect(1, 0, 0));
	Say("3. A filed again at " $ A.Location);
	Mover.SetLocation(Far);
	Say("3. Mover off at " $ Mover.Location);
	Slots(Mover, "3. off");
	Mover.SetLocation(B.Location);
	Say("3. Mover onto B again at " $ Mover.Location);
	Slots(Mover, "3.");
	Radius(P, Q, "3.");

	// 4. the touches with a rider and a base at the spot
	S = Q + vect(0, 600, 0);
	Mover2 = P.Spawn(class'CapHashMover',, 'Mover2', S);
	Rider = Mark(P, 'Rider', S + vect(0, 0, 20));
	if (Mover2 == None || Rider == None)
		return;
	Rider.SetBase(Mover2);
	Say("4. Mover2 10 up, Rider on it");
	bMoved = Mover2.SetLocation(S + vect(0, 0, 10));
	Say("4. moved " $ bMoved $ ", Rider's base " $ Rider.Base);

	BaseMark = Mark(P, 'Base', S + vect(200, 0, 0));
	Mover3 = P.Spawn(class'CapHashMover',, 'Mover3', S + vect(200, 0, 40));
	if (BaseMark == None || Mover3 == None)
		return;
	Mover3.SetBase(BaseMark);
	Say("4. Mover3 onto Base, based on it");
	bMoved = Mover3.SetLocation(BaseMark.Location + vect(0, 0, 10));
	Say("4. moved " $ bMoved $ ", Mover3's base " $ Mover3.Base);
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
			Say("opening " $ Target());
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
		Say("exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
}
