//=============================================================================
// MirrorMoverConsole (bugs.md PHYS.29): lines round a mirrored mover --
// TargetMap ([DXCapture.MirrorMoverConsole], 05_NYC_UNATCOHQ by default;
// 06_HongKong_Helibase, 06_HongKong_MJ12lab, 05_NYC_UNATCOMJ12lab,
// 09_NYC_Ship) opened from wherever the run starts; 2 s into the level's
// time, each mover with a MainScale axis under 0: its MainScale, place,
// turn and box; then level lines, each with its FastTrace and the actor its
// Trace (actors too) meets, where, with what normal:
//   a. across the box's thinner side through its middle, 32 past each face,
//      each way (VisibleConsole's line);
//   b. along the thinner side, 32 past each face, at 8, 24 and 48 units
//      beyond each end of the box's wider side, at its middle's height;
//   c. along the wider side, 48 past each end, at 4, 12, 32 and 64 units out
//      from each face, at its middle's height and 24 above and below.
// The original files each polygon through FPoly::Transform (0x10387360),
// its normal turned by the brush's vector transform -- MainScale's mirror in
// it -- and its corners reversed for a mirroring brush, so the filed solid
// is the mover's; DXEngine's tracker (BrushTracker::File) turns the normal
// by the rotation alone, so a face across the mirror's axis faces in and the
// space behind it, out beyond the face, is solid within its leaf. Lines b
// and c miss the mover and should be clear in the original wherever the
// level is; in DXEngine those beyond a face turned in are blocked, at their
// start or where they enter that space; line a meets the face itself in the
// original. Each line starts "DXMIRROR:".
//=============================================================================
class MirrorMoverConsole extends Console;

var config string TargetMap;

var float MapTime;
var int Step;
var string CurrentMap;

function string Target()
{
	if (TargetMap == "")
		return "05_NYC_UNATCOHQ";
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
	Log("DXMIRROR: " $ M.Name $ " " $ Label $ " from " $ Start $ " fast " $ bClear $ " hit " $ Hit $ " at " $ HitLoc $ " normal " $ HitNorm);
}

function Movers(PlayerPawn P)
{
	local Mover M;
	local vector Min, Max, C, E, Thin, Wide, S;
	local float T, W;
	local int i, j, k, n;
	local float Beyond[3], Out[4];

	Beyond[0] = 8; Beyond[1] = 24; Beyond[2] = 48;
	Out[0] = 4; Out[1] = 12; Out[2] = 32; Out[3] = 64;
	foreach P.AllActors(class'Mover', M)
	{
		if (M.MainScale.Scale.X >= 0 && M.MainScale.Scale.Y >= 0 && M.MainScale.Scale.Z >= 0)
			continue;
		n++;
		M.GetBoundingBox(Min, Max);
		C = (Min + Max) * 0.5;
		E = (Max - Min) * 0.5;
		if (E.X <= E.Y)
		{
			Thin = vect(1,0,0);
			Wide = vect(0,1,0);
			T = E.X;
			W = E.Y;
		}
		else
		{
			Thin = vect(0,1,0);
			Wide = vect(1,0,0);
			T = E.Y;
			W = E.X;
		}
		Log("DXMIRROR: mover " $ M.Name $ " main " $ M.MainScale.Scale $ " at " $ M.Location $ " turn " $ M.Rotation
			$ " box " $ Min $ " to " $ Max $ " collide " $ M.bCollideActors);
		// a. across, both ways
		Probe(P, M, C + Thin * (T + 32), C - Thin * (T + 32), "a+");
		Probe(P, M, C - Thin * (T + 32), C + Thin * (T + 32), "a-");
		// b. along the thinner side, beyond each end of the wider
		for (i = 0; i < 3; i++)
		{
			for (j = -1; j <= 1; j += 2)
			{
				S = C + Wide * (j * (W + Beyond[i]));
				Probe(P, M, S + Thin * (T + 32), S - Thin * (T + 32), "b" $ j $ " " $ Beyond[i]);
			}
		}
		// c. along the wider side, before each face
		for (i = 0; i < 4; i++)
		{
			for (j = -1; j <= 1; j += 2)
			{
				for (k = -1; k <= 1; k++)
				{
					S = C + Thin * (j * (T + Out[i])) + vect(0,0,1) * (k * 24);
					Probe(P, M, S - Wide * (W + 48), S + Wide * (W + 48), "c" $ j $ " " $ Out[i] $ " z" $ (k * 24));
				}
			}
		}
	}
	Log("DXMIRROR: " $ n $ " mirrored movers on " $ CurrentMap);
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
			Log("DXMIRROR: opening " $ Target());
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
		MapTime = 0;
		Step = 1;
	}
	else if (Step == 1 && MapTime > 1.0)
	{
		Log("DXMIRROR: exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
}
