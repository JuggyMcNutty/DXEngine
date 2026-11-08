//=============================================================================
// DecalLiftConsole (bugs.md REN.61): where a decal is drawn off
// its surface.
//
// Render.dll's ClipDecal (0x10b1bda0) sets each corner of the decal 0.75
// units off its surface along the surface's unit normal (vNormal, the 0.75
// at 0x10b2b304), and clips it by the planes through the eye and each edge
// of the surface's drawn polygon, so it lies over the surface where the
// surface lies on screen, a fixed 0.75 over it in the world. DXEngine's
// VisibleDecal moves each corner a unit toward the eye along the view's axis
// (-ViewRotation.XAxis), after a clip in the surface's plane: the decal
// slides on its surface as the view turns, and wherever the view's axis
// lies along the surface it lies in the surface's plane, its depth equal to
// the surface's.
//
// On TargetMap ([DXCapture.DecalLiftConsole], 01_NYC_UNATCOIsland by
// default), the player out of the world, the HUD hidden, conversations and
// datalinks stopped, Decals on, 3 s in: a ScorchMark at DrawScale Scale
// (0.25) laid with a fixed direction on the floor Ahead units (64) in front
// of the start; three marked shots:
//
// - 0: the eye 48 over the floor, Ahead units back from the mark, facing its
//   middle (the mark at the view's middle);
// - 1: the same eye turned 25 degrees right (the mark left of the middle);
// - 2: the eye 48 over the floor and 300 back, the view level (pitch 0):
//   the floor's plane along the view's axis.
//
// Compare shots.py --region over the mark at DXCAP_WINDOW=1024x768, the
// original with DXCAP_RENDERER=D3D. Expected: in shot 0 the original's mark
// some 5 pixels higher than DXEngine's (its 0.75 over the floor seen from
// 37 degrees above), in shot 1 apart by more; in shot 2 the original's mark
// whole, DXEngine's broken up by the depth test where it lies in the
// floor's plane.
//=============================================================================
class DecalLiftConsole extends Console;

var config string TargetMap;
var config float Scale;
var config float Ahead;

var float MapTime, StepTime;
var string CurrentMap;
var int Phase;
var int MarkShot;
var bool bMarking;
var bool bOut;
var vector Eye, Start, MarkSpot, Forward;
var rotator Look, StartLook;
var Object Stopped;
var Decal Mark;

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

function bool PutMark(PlayerPawn P)
{
	local vector Ground, HitLocation, HitNormal;
	local Actor HitActor;

	if (Scale == 0)
		Scale = 0.25;
	if (Ahead == 0)
		Ahead = 64;
	Forward = vector(StartLook);
	Ground = Start + Forward * Ahead;
	HitActor = P.Trace(HitLocation, HitNormal, Ground - vect(0,0,400), Ground, false);
	Log("DXDECALLIFT: floor under " $ Ground $ " met " $ HitActor $ " at " $ HitLocation $ " normal " $ HitNormal);
	if (HitActor == None || HitNormal.Z < 0.99)
		return false;
	MarkSpot = HitLocation;
	Mark = P.Spawn(class'ScorchMark',,, HitLocation + HitNormal, rotator(HitNormal));
	if (Mark == None)
		return false;
	Mark.DrawScale = Scale;
	DeusExDecal(Mark).ReattachDecal(vect(1,0,0));
	Log("DXDECALLIFT: mark " $ Mark.Name $ " at " $ HitLocation $ " DrawScale " $ Scale $ " attached " $ DeusExDecal(Mark).bAttached);
	return true;
}

// The view for shot N
function SetView(int N)
{
	local rotator R;

	if (N == 0 || N == 1)
	{
		Eye = MarkSpot - Forward * Ahead + vect(0,0,48);
		Look = rotator(MarkSpot - Eye);
		if (N == 1)
			Look.Yaw += 4551;
	}
	else
	{
		Eye = MarkSpot - Forward * 300 + vect(0,0,48);
		R = rotator(MarkSpot - Eye);
		R.Pitch = 0;
		Look = R;
	}
	Look.Roll = 0;
	Log("DXDECALLIFT: shot " $ N $ " from " $ Eye $ " facing " $ Look);
}

event Tick(float Delta)
{
	local PlayerPawn P;

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
				Log("DXDECALLIFT: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Phase = -1;
			}
			return;
		}
		if (!bOut && DeusExPlayer(P) != None)
		{
			Start = P.Location;
			StartLook = P.Rotation;
			StartLook.Pitch = 0;
			StartLook.Roll = 0;
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			P.SetPhysics(PHYS_None);
			bOut = true;
		}
		if (MapTime < 3.0 || DeusExPlayer(P) == None)
			return;
		P.ConsoleCommand("set ini:Engine.Engine.ViewportManager Decals True");
		DeusExPlayer(P).ShowHud(False);
		if (!PutMark(P))
		{
			Log("DXDECALLIFT: no flat floor ahead, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		MarkShot = 0;
		SetView(0);
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
	// Each shot: 1 s held, marked for 0.8 s, shot
	if (Phase == 1 && StepTime > 1.0)
	{
		bMarking = true;
		Phase = 2;
	}
	else if (Phase == 2 && StepTime > 1.8)
	{
		Log("DXCAP: shot " $ MarkShot);
		P.ConsoleCommand("shot");
		bMarking = false;
		if (MarkShot == 2)
		{
			Log("DXDECALLIFT: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		MarkShot++;
		SetView(MarkShot);
		Hold(P);
		StepTime = 0;
		Phase = 1;
	}
}

// The mark for the original's screen grabber, as CaptureConsole's
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
