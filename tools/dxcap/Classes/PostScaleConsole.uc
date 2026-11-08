//=============================================================================
// PostScaleConsole (bugs.md PHYS.28): a mover's PostScale --
// TargetMap ([DXCapture.PostScaleConsole], 06_HongKong_WanChai_Market by
// default; DXMP_CMD, 12_Vandenberg_Tunnels, 02_NYC_Warehouse) opened from
// wherever the run starts; 2 s into the level's time, each mover whose
// PostScale is not 1 on every axis: its MainScale, PostScale, place, turn,
// PrePivot and GetBoundingBox; then a grid of level lines through its
// middle's neighbourhood, along X and along Y, 160 long, at sideways offsets
// of -150 to 150 by 30 and heights of -80 to 80 by 20 about its place: each
// line's FastTrace and the actor its Trace meets, where; then the player put
// 160 units out from the mover's place along the box's thinner side, facing
// it, and a shot; an exit.
//
// The original's mover is drawn, filed for the lines and checked as an
// actor through its brush's coordinates with PostScale in them
// (ABrush::BuildCoords 0x1032f9e0, FPoly::Transform 0x10387360, the tracker
// 0x1037a400; ABrush::ToWorld 0x1030f310); DXEngine's leaves it out. On
// WanChai Market DeusExMover3 (a locked door, PostScale 1.8125, 1.00023,
// 0.6875) the original's box should be X 268..500, Z 44..132 and
// DXEngine's X 319..447, Z 20..148; on DXMP_CMD DeusExMover10 the original's
// some 6 x 80 x 80 and DXEngine's some 118 x 347 x 301. What differs between
// the engines: the box lines, the lines' hits at the box's edges, the shot.
// Each line starts "DXPOST:".
//=============================================================================
class PostScaleConsole extends Console;

var config string TargetMap;

var float MapTime;
var int Step;
var string CurrentMap;
var Mover Shown;
var vector ShownMiddle, ShownAxis;

function string Target()
{
	if (TargetMap == "")
		return "06_HongKong_WanChai_Market";
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

function Probe(PlayerPawn P, Mover M, vector Start, vector End, string Label)
{
	local Actor Hit;
	local vector HitLoc, HitNorm;
	local bool bClear;

	bClear = P.FastTrace(End, Start);
	Hit = P.Trace(HitLoc, HitNorm, End, Start, true);
	Log("DXPOST: " $ M.Name $ " " $ Label $ " fast " $ bClear $ " hit " $ Hit $ " at " $ HitLoc $ " normal " $ HitNorm);
}

function Movers(PlayerPawn P)
{
	local Mover M;
	local vector Min, Max, C, E, S, Off;
	local int i, j;

	foreach P.AllActors(class'Mover', M)
	{
		if (M.PostScale.Scale == vect(1,1,1))
			continue;
		M.GetBoundingBox(Min, Max);
		C = (Min + Max) * 0.5;
		E = (Max - Min) * 0.5;
		Log("DXPOST: mover " $ M.Name $ " main " $ M.MainScale.Scale $ " post " $ M.PostScale.Scale
			$ " at " $ M.Location $ " turn " $ M.Rotation $ " prepivot " $ M.PrePivot $ " box " $ Min $ " to " $ Max);
		if (Shown == None)
		{
			Shown = M;
			ShownMiddle = M.Location;
			if (E.X <= E.Y)
				ShownAxis = vect(1,0,0);
			else
				ShownAxis = vect(0,1,0);
		}
		for (i = -5; i <= 5; i++)
		{
			for (j = -4; j <= 4; j++)
			{
				// Along X, offset sideways in Y
				Off = vect(0,1,0) * (i * 30) + vect(0,0,1) * (j * 20);
				S = M.Location + Off;
				Probe(P, M, S - vect(80,0,0), S + vect(80,0,0), "x y" $ (i * 30) $ " z" $ (j * 20));
				// Along Y, offset sideways in X
				Off = vect(1,0,0) * (i * 30) + vect(0,0,1) * (j * 20);
				S = M.Location + Off;
				Probe(P, M, S - vect(0,80,0), S + vect(0,80,0), "y x" $ (i * 30) $ " z" $ (j * 20));
			}
		}
	}
	if (Shown == None)
		Log("DXPOST: no mover with a PostScale on " $ CurrentMap);
}

function View(PlayerPawn P)
{
	local vector Spot;

	if (Shown == None)
		return;
	Spot = ShownMiddle + ShownAxis * 160;
	if (!P.SetLocation(Spot))
	{
		Spot = ShownMiddle - ShownAxis * 160;
		P.SetLocation(Spot);
	}
	P.SetRotation(rotator(ShownMiddle - P.Location));
	P.ViewRotation = rotator(ShownMiddle - P.Location);
	Log("DXPOST: the player at " $ P.Location $ " facing " $ Shown.Name);
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
			Log("DXPOST: opening " $ Target());
			P.ConsoleCommand("open " $ Target());
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= Target()))
		Step = 0;

	if (Step == 0 && P.Level.TimeSeconds > 2.0)
	{
		Movers(P);
		View(P);
		MapTime = 0;
		Step = 1;
	}
	else if (Step == 1 && MapTime > 1.0)
	{
		P.ConsoleCommand("shot");
		Log("DXPOST: shot taken");
		MapTime = 0;
		Step = 2;
	}
	else if (Step == 2 && MapTime > 1.0)
	{
		Log("DXPOST: exiting");
		P.ConsoleCommand("exit");
		Step = 3;
	}
}
