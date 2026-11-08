//=============================================================================
// ProxyWallConsole (bugs.md REN.63): whether an actor just
// behind a wall counts as drawn (LastRendered()), as the walk's planes cut
// its proxy.
//
// Render.dll makes every sprite at the BSP's root (SetupDynamics 0x10b22d90
// hands FDynamicSprite's constructor 0x10b233c0 node 0): its rectangle, set
// back at the actor's depth, is cut at every plane on its way down, each
// piece tested where it lies in the front-to-back walk. DXEngine's proxy
// starts at the node the actor is filed in (VisibleFrame::ProcessNode, the
// node's ActorList; UActor::UpdateBspInfo, the first node whose plane its
// mesh box crosses), so the planes above that node never cut it: a piece
// lying past a wall's plane is tested on the actor's own side of the walk,
// before or after the wall where the original tests it on the other.
//
// On TargetMap ([DXCapture.ProxyWallConsole], 01_NYC_UNATCOIsland by
// default), the player out of the world, the HUD hidden, every pawn gone,
// conversations and datalinks stopped, 3 s in: of the level's path nodes,
// up to Pairs (6) pairs, the first of each the eye (its location plus the
// player's eye height), the other 150 to 600 units away with the line to it
// blocked at three heights, as AugVisionConsole picks them. For each pair:
// the line from the far node toward the eye meets the wall's far face; bare
// meshes (CrateUnbreakableSmall's, collision off, unlit) at 24, 40, 64 and
// 96 units off that face on the far node's side. The view from the eye
// toward the face's hit, turned by -22, -11, 0, 11 and 22 degrees; at each,
// 0.6 s held, then "DXPROXYWALL: pair p yaw y depth d drawn L" with each
// crate's LastRendered().
//
// Compare the two logs' lines: those apart are where the proxies' cuts
// differ (the original's under a frame where a piece crosses the wall's
// plane into the eye's side of the walk; DXEngine's under a frame where a
// piece past an ancestor plane is tested early). Then against a build whose
// proxies start at the root, where they should agree but for the one span
// buffer (by choice).
//=============================================================================
class ProxyWallConsole extends Console;

var config string TargetMap;
var config int Pairs;

var float MapTime, StepTime;
var string CurrentMap;
var int Phase;
var bool bOut;
var vector Eye;
var rotator Look, Aim;
var Object Stopped;
var PathNode EyeNode[8];
var PathNode FarNode[8];
var vector Face[8];
var int NumPairs;
var int Pair, YawStep;
var Actor Crates[4];
var float Depths[4];

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

function Hush(PlayerPawn P)
{
	local Actor A;

	foreach P.AllActors(class'Actor', A)
	{
		if (A == P)
			continue;
		if (A.IsA('Pawn') || A.IsA('SecurityCamera') || A.IsA('AutoTurret') || A.IsA('AlarmUnit') || A.IsA('LaserTrigger') || A.IsA('BeamTrigger'))
			A.Destroy();
	}
}

function Silence(PlayerPawn P)
{
	local DeusExPlayer DXP;

	DXP = DeusExPlayer(P);
	if (DXP == None)
		return;
	if (DXP.dataLinkPlay != None)
		DXP.dataLinkPlay.AbortDataLink();
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		DXP.conPlay.TerminateConversation();
	}
}

function Hold(PlayerPawn P)
{
	P.SetLocation(Eye - vect(0,0,1) * P.EyeHeight);
	P.SetRotation(Look);
	P.ViewRotation = Look;
	P.Velocity = vect(0,0,0);
}

function bool Blocked(PlayerPawn P, vector From, vector Spot)
{
	return !P.FastTrace(Spot + vect(0,0,-30), From) && !P.FastTrace(Spot, From) && !P.FastTrace(Spot + vect(0,0,40), From);
}

// Up to Pairs pairs of path nodes with a wall between, as AugVisionConsole's
// PickSpots, a node taken once
function PickPairs(PlayerPawn P)
{
	local PathNode V, N;
	local vector E, HitLocation, HitNormal;
	local Actor HitActor;
	local float D;
	local int Tried;

	if (Pairs <= 0 || Pairs > 8)
		Pairs = 6;
	foreach P.AllActors(class'PathNode', V)
	{
		if (NumPairs >= Pairs || ++Tried > 200)
			break;
		E = V.Location;
		E.Z += P.BaseEyeHeight;
		foreach P.AllActors(class'PathNode', N)
		{
			D = VSize(N.Location - E);
			if (D < 150 || D > 600 || !Blocked(P, E, N.Location))
				continue;
			HitActor = P.Trace(HitLocation, HitNormal, E, N.Location, false);
			if (HitActor == None || Abs(HitNormal.Z) > 0.3)
				continue;
			EyeNode[NumPairs] = V;
			FarNode[NumPairs] = N;
			Face[NumPairs] = HitLocation;
			Log("DXPROXYWALL: pair " $ NumPairs $ " eye " $ V.Name $ " " $ E $ " far " $ N.Name $ " " $ N.Location
			    $ " the wall's far face at " $ HitLocation $ " normal " $ HitNormal);
			NumPairs++;
			break;
		}
	}
}

function Actor Crate(PlayerPawn P, vector Spot)
{
	local Actor C;

	C = P.Spawn(class'Effects',,, Spot, rotator(Eye - Spot));
	if (C == None)
		return None;
	C.SetCollision(false, false, false);
	C.bCollideWorld = false;
	C.SetPhysics(PHYS_None);
	C.LifeSpan = 0;
	C.bHidden = false;
	C.DrawType = DT_Mesh;
	C.Style = STY_Normal;
	C.Mesh = class'CrateUnbreakableSmall'.default.Mesh;
	C.Skin = class'CrateUnbreakableSmall'.default.Skin;
	C.DrawScale = class'CrateUnbreakableSmall'.default.DrawScale;
	C.bUnlit = true;
	return C;
}

// The crates of pair P, off the wall's far face on the far node's side
function SetPair(PlayerPawn P, int K)
{
	local int i;
	local vector Away, Spot;

	for (i = 0; i < 4; i++)
	{
		if (Crates[i] != None)
			Crates[i].Destroy();
		Crates[i] = None;
	}
	Eye = EyeNode[K].Location;
	Eye.Z += P.BaseEyeHeight;
	Away = FarNode[K].Location - Face[K];
	Away.Z = 0;
	Away = Normal(Away);
	for (i = 0; i < 4; i++)
	{
		Spot = Face[K] + Away * Depths[i];
		Crates[i] = Crate(P, Spot);
		if (Crates[i] == None)
			Log("DXPROXYWALL: pair " $ K $ " depth " $ Depths[i] $ ": no crate at " $ Spot);
	}
	Aim = rotator(Face[K] - Eye);
	Aim.Roll = 0;
}

function SetYaw(int Step)
{
	Look = Aim;
	Look.Yaw += (Step - 2) * 2002;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local int i;

	Super.Tick(Delta);
	if (Phase == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	Silence(P);

	if (Phase == 0)
	{
		if (!(CurrentMap ~= Target()))
		{
			if (MapTime > 3.0)
			{
				Log("DXPROXYWALL: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Phase = -1;
			}
			return;
		}
		if (!bOut && DeusExPlayer(P) != None)
		{
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			P.SetPhysics(PHYS_None);
			bOut = true;
		}
		if (MapTime < 3.0 || DeusExPlayer(P) == None)
			return;
		Depths[0] = 24;
		Depths[1] = 40;
		Depths[2] = 64;
		Depths[3] = 96;
		Hush(P);
		DeusExPlayer(P).ShowHud(False);
		PickPairs(P);
		if (NumPairs == 0)
		{
			Log("DXPROXYWALL: no pair of nodes with a wall between, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		Pair = 0;
		YawStep = 0;
		SetPair(P, Pair);
		SetYaw(YawStep);
		Hold(P);
		StepTime = 0;
		Phase = 1;
		return;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= Target())
			Phase = 0;
		return;
	}

	Hold(P);
	if (Phase == 1 && StepTime > 0.6)
	{
		for (i = 0; i < 4; i++)
		{
			if (Crates[i] != None)
				Log("DXPROXYWALL: pair " $ Pair $ " yaw " $ ((YawStep - 2) * 11) $ " depth " $ Depths[i] $ " drawn " $ Crates[i].LastRendered());
		}
		YawStep++;
		if (YawStep > 4)
		{
			YawStep = 0;
			Pair++;
			if (Pair >= NumPairs)
			{
				Log("DXPROXYWALL: done, exiting");
				P.ConsoleCommand("exit");
				Phase = 99;
				return;
			}
			SetPair(P, Pair);
		}
		SetYaw(YawStep);
		Hold(P);
		StepTime = 0;
	}
}
