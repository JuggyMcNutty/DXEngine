//=============================================================================
// DebrisConsole: where a broken glass mover's pieces and a destroyed
// container's trash can be spawned, in both engines (bugs.md PHYS.15). On
// TargetMap ([DXCapture.DebrisConsole], Liberty Island by default), 6 s in:
// - for each glass mover (a DeusExMover whose FragmentClass is
//   GlassFragment), its fragment class spawned where BlowItUp spawns one --
//   FragmentSpread units from the brush's origin, Location - (PrePivot >>
//   Rotation) -- toward each of Dirs directions spread evenly over the
//   sphere (128 by default), the glass whole and colliding as BlowItUp has
//   it then; each spawn's spot and the place it was given, or "refused",
//   and the piece destroyed at once; then a line of how many fitted;
// - with TrashRadius set, TrashPaper spawned the same way at Steps^3 spots
//   (5 by default) over the box Containers.Destroyed draws its spots from,
//   TrashX, TrashY, TrashZ give or take half of TrashRadius and of
//   TrashHeight;
// - the TrashPaper already there (what a container destroyed before left),
//   and whether ContainerName is still in the level;
// - unless Keep, each glass mover broken (BlowItUp), the pieces each break
//   made counted at once, and all of them again 2 s on; an exit.
// Each line is logged with "DXDEBRIS:" in front.
//=============================================================================
class DebrisConsole extends Console;

var config string TargetMap;
var config int Dirs;
var config bool Keep;
var config float TrashX, TrashY, TrashZ, TrashRadius, TrashHeight;
var config int Steps;
var config string ContainerName;

var string CurrentMap;
var float MapTime;
var float StepTime;
var int Phase;

function string Target()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOIsland";
	return TargetMap;
}

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

// The i-th of N directions spread evenly over the sphere (a Fibonacci
// lattice): the same in both engines, where BlowItUp's VRand is not
function vector Direction(int i, int N)
{
	local vector D;
	local float Z, R, Phi;
	Z = 1.0 - (2.0 * i + 1.0) / N;
	R = Sqrt(FMax(0.0, 1.0 - Z * Z));
	Phi = 2.39996323 * i;
	D.X = R * Cos(Phi);
	D.Y = R * Sin(Phi);
	D.Z = Z;
	return D;
}

function GlassProbe(PlayerPawn P)
{
	local DeusExMover G;
	local vector Origin, Spot;
	local Actor F;
	local int i, N, Fitted, Moved;

	N = Dirs;
	if (N <= 0)
		N = 128;
	foreach P.AllActors(class'DeusExMover', G)
	{
		if (G.FragmentClass != class'GlassFragment')
			continue;
		Origin = G.Location - (G.PrePivot >> G.Rotation);
		Fitted = 0;
		Moved = 0;
		for (i = 0; i < N; i++)
		{
			Spot = Origin + G.FragmentSpread * Direction(i, N);
			F = P.Spawn(G.FragmentClass,,, Spot);
			if (F == None)
			{
				Log("DXDEBRIS: " $ G.Name $ " piece " $ i $ " at " $ Spot $ " refused");
				continue;
			}
			Fitted++;
			if (VSize(F.Location - Spot) > 0.01)
				Moved++;
			Log("DXDEBRIS: " $ G.Name $ " piece " $ i $ " at " $ Spot $ " placed at " $ F.Location);
			F.Destroy();
		}
		Log("DXDEBRIS: " $ G.Name $ " at " $ G.Location $ " origin " $ Origin $ " spread " $ G.FragmentSpread
			$ ": " $ Fitted $ " of " $ N $ " fitted, " $ Moved $ " of them moved");
	}
}

function TrashProbe(PlayerPawn P)
{
	local vector Spot;
	local Actor T;
	local int i, j, k, N, Fitted, Total;

	if (TrashRadius <= 0)
		return;
	N = Steps;
	if (N < 2)
		N = 5;
	for (i = 0; i < N; i++)
		for (j = 0; j < N; j++)
			for (k = 0; k < N; k++)
			{
				Spot.X = TrashX + TrashRadius / 2 - TrashRadius * i / (N - 1);
				Spot.Y = TrashY + TrashRadius / 2 - TrashRadius * j / (N - 1);
				Spot.Z = TrashZ + TrashHeight / 2 - TrashHeight * k / (N - 1);
				Total++;
				T = P.Spawn(class'TrashPaper',,, Spot);
				if (T == None)
				{
					Log("DXDEBRIS: trash " $ i $ " " $ j $ " " $ k $ " at " $ Spot $ " refused");
					continue;
				}
				Fitted++;
				Log("DXDEBRIS: trash " $ i $ " " $ j $ " " $ k $ " at " $ Spot $ " placed at " $ T.Location);
				T.Destroy();
			}
	Log("DXDEBRIS: trash: " $ Fitted $ " of " $ Total $ " fitted");
}

// What a container destroyed before the probe left, and whether it is gone
function Leftovers(PlayerPawn P)
{
	local TrashPaper T;
	local Containers C;
	local int Count;
	local bool bThere;

	foreach P.AllActors(class'TrashPaper', T)
	{
		Count++;
		Log("DXDEBRIS: TrashPaper " $ T.Name $ " at " $ T.Location);
	}
	Log("DXDEBRIS: " $ Count $ " TrashPaper in the level");
	if (ContainerName != "")
	{
		foreach P.AllActors(class'Containers', C)
			if (string(C.Name) ~= ContainerName)
				bThere = true;
		Log("DXDEBRIS: " $ ContainerName $ " in the level: " $ bThere);
	}
}

function int Pieces(PlayerPawn P)
{
	local GlassFragment F;
	local int Count;
	foreach P.AllActors(class'GlassFragment', F)
		Count++;
	return Count;
}

function BreakAll(PlayerPawn P)
{
	local DeusExMover G;
	local int Before;

	foreach P.AllActors(class'DeusExMover', G)
	{
		if (G.FragmentClass != class'GlassFragment' || G.bDestroyed)
			continue;
		Before = Pieces(P);
		G.BlowItUp(P);
		Log("DXDEBRIS: " $ G.Name $ " broken: " $ (Pieces(P) - Before) $ " of " $ G.NumFragments $ " pieces");
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local GlassFragment F;
	local string M;
	local int Count;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	M = MapOf(P);
	if (M != CurrentMap)
	{
		CurrentMap = M;
		MapTime = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (!(M ~= Target()))
		{
			if (MapTime > 3.0)
			{
				Log("DXDEBRIS: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Phase = -1;
			}
		}
		else if (MapTime > 6.0 && DXP != None)
		{
			Leftovers(P);
			GlassProbe(P);
			TrashProbe(P);
			if (!Keep)
				BreakAll(P);
			StepTime = 0;
			Phase = 1;
		}
	}
	else if (Phase == -1 && (M ~= Target()))
		Phase = 0;
	else if (Phase == 1 && StepTime > 2.0)
	{
		foreach P.AllActors(class'GlassFragment', F)
		{
			Count++;
			Log("DXDEBRIS: GlassFragment " $ F.Name $ " at " $ F.Location);
		}
		Log("DXDEBRIS: " $ Count $ " GlassFragment in the level 2 s on; done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
