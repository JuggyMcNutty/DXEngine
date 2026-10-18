//=============================================================================
// CanisterConsole: an augmentation canister seen from near and far, shot in
// both engines (docs/ENGINE.md, rendering). The effect inside its shell, the
// mesh's second texture, is drawn translucent; the shell hides it at any
// distance. On Liberty Island, 6 s in: a canister set on the pier 160 units
// ahead of the start, still, and a scorch mark on the pier's planks 40 units
// to its left, as an explosion leaves one (a decal: drawn modulated, flat on
// the floor, which the view meets at a slant); the player out of the
// world's collision, held
// in the air behind it at each of Distances ([DXCapture.CanisterConsole]:
// 120, 250, 400, 700 and 1200 units by default) from it, a quarter of that
// above it, looking at it at the field of view the game gives; a view is
// held 1.5 s, then marked for the original's grabber MarkTime s (0.6 by
// default) and shot. Then exit. The HUD is hidden, and conversations and
// datalinks are stopped as they start. Each line is logged with "DXCAN:" in
// front.
//=============================================================================
class CanisterConsole extends Console;

var config float Distances[8];
var config float MarkTime;

var string CurrentMap;
var float MapTime;
var float StepTime;
var int Phase;
var int ViewIndex;
var int ShotsTaken;
var AugmentationCannister Can;
var ScorchMark Scorch;
var vector Ahead;
var vector Eye;
var rotator Facing;
var Object Stopped;

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

function float Mark()
{
	if (MarkTime <= 0)
		return 0.6;
	return MarkTime;
}

// The distance of view i, 0 past the last
function float Distance(int i)
{
	local float Defaults[5];

	if (Distances[0] > 0)
	{
		if (i < ArrayCount(Distances))
			return Distances[i];
		return 0;
	}
	Defaults[0] = 120;
	Defaults[1] = 250;
	Defaults[2] = 400;
	Defaults[3] = 700;
	Defaults[4] = 1200;
	if (i < ArrayCount(Defaults))
		return Defaults[i];
	return 0;
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

// A conversation's camera would take the view, and a datalink's text the
// frame's foot.
function Silence(DeusExPlayer DXP)
{
	if (DXP.dataLinkPlay != None && DXP.dataLinkPlay.con != None && DXP.dataLinkPlay.con != Stopped)
	{
		Stopped = DXP.dataLinkPlay.con;
		Log("DXCAN: datalink " $ Stopped.Name $ " stopped");
		DXP.dataLinkPlay.AbortDataLink();
	}
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		Log("DXCAN: conversation " $ Stopped.Name $ " stopped");
		DXP.conPlay.TerminateConversation();
	}
}

event PostRender(canvas C)
{
	local int i;
	Super.PostRender(C);
	if (!bMarking)
		return;
	C.Style = 1;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		if ((MarkShot & (1 << i)) != 0)
		{
			C.DrawColor.R = 255;
			C.DrawColor.G = 255;
			C.DrawColor.B = 255;
		}
		else
		{
			C.DrawColor.R = 0;
			C.DrawColor.G = 0;
			C.DrawColor.B = 0;
		}
		C.SetPos(12 + 12 * i, 0);
		C.DrawRect(Texture'Solid', 12, 12);
	}
}

function vector Ground(PlayerPawn P, vector S)
{
	local vector HitLocation, HitNormal;
	if (P.Trace(HitLocation, HitNormal, S - vect(0,0,512), S, false) != None)
		return HitLocation;
	return S;
}

// The canister on the pier ahead of the start, still
function SetCanister(PlayerPawn P)
{
	local rotator R;
	local vector X, Y, Z;

	R = P.ViewRotation;
	R.Pitch = 0;
	R.Roll = 0;
	GetAxes(R, X, Y, Z);
	Ahead = X;
	Can = P.Spawn(class'AugmentationCannister',,, Ground(P, P.Location + X * 160)
		+ vect(0,0,1) * class'AugmentationCannister'.default.CollisionHeight, R);
	if (Can == None)
	{
		Log("DXCAN: no room for the canister");
		return;
	}
	Can.SetPhysics(PHYS_None);
	Log("DXCAN: canister " $ Can.Name $ " at " $ Can.Location $ " rotation " $ Can.Rotation $ " mesh " $ Can.Mesh
		$ " style " $ int(Can.Style) $ " zone " $ Can.Region.Zone.Name);
	// As an explosion leaves one (DeusExDecoration.Explode)
	Scorch = P.Spawn(class'ScorchMark',,, Ground(P, Can.Location - Y * 40) + vect(0,0,1), rot(16384,0,0));
	if (Scorch == None)
		Log("DXCAN: no scorch mark");
	else
	{
		// Its class's texture, not the one its BeginPlay picks half the time
		Scorch.Texture = Scorch.default.Texture;
		Scorch.DrawScale = 1.5;
		Scorch.ReattachDecal();
		Log("DXCAN: scorch mark " $ Scorch.Name $ " at " $ Scorch.Location $ " texture " $ Scorch.Texture);
	}
}

// The eye at the view's distance behind the canister, a quarter of it above
function BeginView(PlayerPawn P)
{
	local float D;

	D = Distance(ViewIndex);
	Eye = Can.Location - Ahead * D + vect(0,0,1) * (D * 0.25);
	Facing = rotator(Can.Location - Eye);
	HoldView(P);
	Log("DXCAN: view " $ ViewIndex $ " distance " $ D $ " eye " $ Eye $ " rotation " $ Facing
		$ " clear " $ P.FastTrace(Can.Location, Eye));
}

function HoldView(PlayerPawn P)
{
	local rotator Turn;

	P.SetPhysics(PHYS_None);
	P.Velocity = vect(0,0,0);
	P.Acceleration = vect(0,0,0);
	P.EyeHeight = P.BaseEyeHeight;
	P.SetLocation(Eye - vect(0,0,1) * P.BaseEyeHeight);
	P.ViewRotation = Facing;
	Turn.Yaw = Facing.Yaw;
	P.SetRotation(Turn);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local string M;

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
	if (DXP != None && (M ~= "01_NYC_UNATCOIsland"))
		Silence(DXP);

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				Log("DXCAN: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = -1;
			}
		}
		else if (MapTime > 6.0 && DXP != None)
		{
			SetCanister(P);
			if (Can == None)
			{
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			DXP.KillShadow();
			P.bDetectable = false;
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			// The original's HUD draws over the grabber's mark.
			if (DeusExRootWindow(DXP.rootWindow) != None)
				DeusExRootWindow(DXP.rootWindow).ShowHud(False);
			ViewIndex = 0;
			BeginView(P);
			StepTime = 0;
			Phase = 1;
		}
	}
	else if (Phase == -1 && (M ~= "01_NYC_UNATCOIsland"))
		Phase = 0;
	else if (Phase == 1)
	{
		HoldView(P);
		if (!bMarking && StepTime > 1.5)
		{
			MarkShot = ShotsTaken;
			bMarking = true;
			StepTime = 0;
		}
		else if (bMarking && StepTime > Mark())
		{
			Log("DXCAN: shot " $ ShotsTaken $ " distance " $ Distance(ViewIndex) $ " eye "
				$ (P.Location + vect(0,0,1) * P.EyeHeight) $ " rotation " $ P.ViewRotation $ " FOV " $ P.FOVAngle
				$ " scorch mark " $ (Scorch != None && !Scorch.bDeleteMe));
			P.ConsoleCommand("shot");
			bMarking = false;
			ShotsTaken++;
			ViewIndex++;
			StepTime = 0;
			if (Distance(ViewIndex) <= 0)
				Phase = 2;
			else
				BeginView(P);
		}
	}
	else if (Phase == 2 && StepTime > 1.0)
	{
		Log("DXCAN: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
