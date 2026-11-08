//=============================================================================
// DecalSeenConsole (bugs.md REN.62): which decals count as
// drawn -- the Decal's LastRenderedTime, which DeusExDecal.Timer reads to
// keep a decal or destroy it.
//
// Render.dll's DrawFrame stamps a decal whenever ClipDecal (0x10b1bda0)
// leaves some of it inside a saved polygon of its surface: the polygon of a
// node with any span showing, clipped to the view's sides, no span buffer
// asked (DrawFrame's decal loop, 0x10b1b1d0-0x10b1b2aa; OccludeBsp files the
// saved polygons). So a decal hidden behind nearer geometry, on a node of
// which some part shows, is stamped. DXEngine stamps one only where its own
// polygon finds a span before its surface fills them
// (VisibleFrame::ProcessNodeSurface, Clipper.IsPolygonVisible).
//
// On TargetMap ([DXCapture.DecalSeenConsole], 01_NYC_UNATCOIsland by
// default), the player out of the world at the start's eye, the HUD hidden,
// conversations and datalinks stopped, Decals on, 3 s in: the view turned
// 11 degrees down; on the ground under a fan of 9 directions (yaw from -27
// to +27 degrees of the start's) and 7 distances (200 to 1400), a
// ScorchMark at DrawScale Scale (0.5) laid with a fixed direction where the
// line down meets a floor (normal Z over 0.7). Each logs whether the eye's
// line to 8 units over it is blocked by the level ("DXDECALSEEN: mark i ...
// blocked B"). At 2 s and 4 s each mark's age, the level's time less its
// LastRenderedTime, is logged; a marked shot at 2.5 s.
//
// Expected: the marks not blocked, in the view: age under a frame in both.
// Blocked ones: the original's under a frame for those whose node shows
// elsewhere, DXEngine's the time since they were laid (2 and 4) for each.
//=============================================================================
class DecalSeenConsole extends Console;

var config string TargetMap;
var config float Scale;

var float MapTime, StepTime;
var string CurrentMap;
var int Phase;
var int MarkShot;
var bool bMarking;
var bool bOut;
var vector Eye;
var rotator Look;
var Object Stopped;
var Decal Marks[64];
var int Blocked[64];
var int NumMarks;
var float LaidAt;

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

// The fan of marks on the ground in front of the eye
function LayMarks(PlayerPawn P)
{
	local int i, j;
	local rotator R;
	local vector Ground, HitLocation, HitNormal, Spot;
	local Actor HitActor;
	local Decal D;

	if (Scale == 0)
		Scale = 0.5;
	for (i = 0; i < 9; i++)
	{
		for (j = 0; j < 7; j++)
		{
			if (NumMarks >= 64)
				return;
			R = Look;
			R.Pitch = 0;
			R.Roll = 0;
			R.Yaw += (i - 4) * 1250;
			Ground = Eye + vector(R) * (200 + 200 * j);
			HitActor = P.Trace(HitLocation, HitNormal, Ground - vect(0,0,1000), Ground, false);
			if (HitActor == None || HitNormal.Z < 0.7)
			{
				Log("DXDECALSEEN: no floor under yaw step " $ i $ " distance " $ (200 + 200 * j));
				continue;
			}
			D = P.Spawn(class'ScorchMark',,, HitLocation + HitNormal, rotator(HitNormal));
			if (D == None)
			{
				Log("DXDECALSEEN: no mark at " $ HitLocation);
				continue;
			}
			D.DrawScale = Scale;
			DeusExDecal(D).ReattachDecal(vect(1,0,0));
			Spot = HitLocation + HitNormal * 8;
			Marks[NumMarks] = D;
			if (P.FastTrace(Spot, Eye))
				Blocked[NumMarks] = 0;
			else
				Blocked[NumMarks] = 1;
			Log("DXDECALSEEN: mark " $ NumMarks $ " " $ D.Name $ " yaw step " $ i $ " distance " $ (200 + 200 * j)
			    $ " at " $ HitLocation $ " blocked " $ Blocked[NumMarks] $ " attached " $ DeusExDecal(D).bAttached);
			NumMarks++;
		}
	}
}

function LogMarks(PlayerPawn P, string What)
{
	local int i;

	for (i = 0; i < NumMarks; i++)
	{
		if (Marks[i] == None || Marks[i].bDeleteMe)
			Log("DXDECALSEEN: " $ What $ " mark " $ i $ " gone");
		else
			Log("DXDECALSEEN: " $ What $ " mark " $ i $ " blocked " $ Blocked[i] $ " age " $ (P.Level.TimeSeconds - Marks[i].LastRenderedTime));
	}
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
				Log("DXDECALSEEN: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Phase = -1;
			}
			return;
		}
		if (!bOut && DeusExPlayer(P) != None)
		{
			Eye = P.Location + vect(0,0,1) * P.EyeHeight;
			Look = P.Rotation;
			Look.Pitch = -2000;
			Look.Roll = 0;
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			P.SetPhysics(PHYS_None);
			bOut = true;
		}
		if (MapTime < 3.0 || DeusExPlayer(P) == None)
			return;
		P.ConsoleCommand("set ini:Engine.Engine.ViewportManager Decals True");
		DeusExPlayer(P).ShowHud(False);
		Hold(P);
		Log("DXDECALSEEN: the eye at " $ Eye $ " looking " $ Look);
		LayMarks(P);
		LaidAt = P.Level.TimeSeconds;
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
	if (Phase == 1 && StepTime > 2.0)
	{
		LogMarks(P, "at 2 s");
		MarkShot = 0;
		bMarking = true;
		Phase = 2;
	}
	else if (Phase == 2 && StepTime > 2.8)
	{
		Log("DXCAP: shot 0");
		P.ConsoleCommand("shot");
		bMarking = false;
		Phase = 3;
	}
	else if (Phase == 3 && StepTime > 4.0)
	{
		LogMarks(P, "at 4 s");
		Log("DXDECALSEEN: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
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
