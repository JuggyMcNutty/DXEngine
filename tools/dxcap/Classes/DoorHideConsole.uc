//=============================================================================
// DoorHideConsole (bugs.md REN.60): whether a closed door hides
// what lies behind it from what counts as drawn (LastRendered()).
//
// Render.dll's OccludeBsp (0x10b173d0) walks the polygons the brush tracker
// filed for a mover as surfaces of the level's own: an opaque one fills its
// zone's span buffer as any wall does, so a sprite behind it finds no span
// and is not kept (render-dll.md: which actors are drawn, what occludes).
// DXEngine's clipper takes the level's own surfaces alone
// (VisibleFrame::ProcessNodeSurface's CheckSurface); a mover is drawn as an
// actor and fills nothing.
//
// On TargetMap ([DXCapture.DoorHideConsole], 01_NYC_UNATCOIsland by
// default), the player out of the world, the HUD hidden, every pawn but the
// player gone, conversations and datalinks stopped, 3 s in:
//
// - the first door in name order (a DeusExMover with bIsDoor, no glass), or
//   the mover named DoorName; the eye Ahead units (120) from its box's middle
//   along its thinnest side, on the side its line from the middle is clear,
//   facing the middle;
// - a bare mesh, CrateUnbreakableSmall's, collision off and unlit, Behind
//   units (40) past the middle on the door's other side ("behind"), and one
//   beside the line, 50 units nearer the eye and 70 to its side ("beside"),
//   in plain view.
//
// Every half second for 4 s "DXDOORHIDE: closed ..." logs both crates'
// LastRendered() and the door's KeyNum; then the door is opened (DoOpen) and
// 6 s more are logged ("DXDOORHIDE: opening ..."). A marked shot of each
// phase for the original's grabber.
//
// Expected: closed, the original's "behind" grows by 0.5 each line (hidden
// by the door), DXEngine's stays under a frame (counted drawn); "beside"
// under a frame in both; once the door is open, both under a frame.
//=============================================================================
class DoorHideConsole extends Console;

var config string TargetMap;
var config string DoorName;
var config float Ahead;
var config float Behind;

var float MapTime, StepTime, LogTime;
var string CurrentMap;
var int Phase;
var int MarkShot;
var bool bMarking;
var bool bOut;
var DeusExMover Door;
var vector Dir, Middle, Eye;
var rotator Look;
var Object Stopped;
var Actor CrateBehind, CrateBeside;

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

function bool Choose(PlayerPawn P)
{
	local DeusExMover M;
	local vector Min, Max, E;

	foreach P.AllActors(class'DeusExMover', M)
	{
		if (DoorName != "")
		{
			if (string(M.Name) ~= DoorName)
				Door = M;
		}
		else if (!M.IsA('BreakableGlass') && M.bIsDoor && M.KeyNum == 0 && (Door == None || Caps(string(M.Name)) < Caps(string(Door.Name))))
			Door = M;
	}
	if (Door == None || !Door.GetBoundingBox(Min, Max))
		return false;
	Middle = (Min + Max) * 0.5;
	E = (Max - Min) * 0.5;
	if (E.X <= E.Y && E.X <= E.Z)
		Dir = vect(1,0,0);
	else if (E.Y <= E.Z)
		Dir = vect(0,1,0);
	else
		Dir = vect(0,0,1);
	return true;
}

// Every pawn but the player goes, destroyed, not killed, so no script hears
// of a death; and every watcher, as AugVisionConsole's Hush
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

// A bare mesh as GlassConsole's StandCrate makes one: no collision, no
// physics, unlit, so nothing but the occlusion decides its render time
function Actor Crate(PlayerPawn P, vector Spot, string What)
{
	local Actor C;

	C = P.Spawn(class'Effects',,, Spot, rotator(Eye - Spot));
	if (C == None)
	{
		Log("DXDOORHIDE: no crate " $ What $ " at " $ Spot);
		return None;
	}
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
	Log("DXDOORHIDE: crate " $ What $ " " $ C.Name $ " at " $ C.Location $ ", " $ VSize(C.Location - Eye)
	    $ " from the eye, zone " $ C.Region.Zone.Name $ ", line clear " $ P.FastTrace(C.Location, Eye));
	return C;
}

function LogCrates(PlayerPawn P, string What)
{
	local string S;

	S = "DXDOORHIDE: " $ What $ " at " $ MapTime $ ":";
	if (CrateBehind != None)
		S = S $ " behind " $ CrateBehind.LastRendered();
	if (CrateBeside != None)
		S = S $ " beside " $ CrateBeside.LastRendered();
	S = S $ " door KeyNum " $ Door.KeyNum $ " at " $ Door.Location;
	Log(S);
}

function Mark(int N, string What)
{
	Log("DXDOORHIDE: shot " $ N $ ", " $ What);
	MarkShot = N;
	bMarking = true;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local vector Side;

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
				Log("DXDOORHIDE: opening " $ Target());
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
		if (!Choose(P))
		{
			Log("DXDOORHIDE: no closed door, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		if (Ahead == 0)
			Ahead = 120;
		if (Behind == 0)
			Behind = 40;
		Eye = Middle + Dir * Ahead;
		if (!P.FastTrace(Middle, Eye))
		{
			Dir = -Dir;
			Eye = Middle + Dir * Ahead;
		}
		Look = rotator(Middle - Eye);
		Hush(P);
		DeusExPlayer(P).ShowHud(False);
		Hold(P);
		Log("DXDOORHIDE: door " $ Door.Name $ " at " $ Door.Location $ ", its middle " $ Middle $ ", the eye at " $ Eye);
		CrateBehind = Crate(P, Middle - Dir * Behind, "behind");
		Side = Normal(Dir Cross vect(0,0,1));
		if (VSize(Side) < 0.5)
			Side = vect(1,0,0);
		CrateBeside = Crate(P, Middle + Dir * 50 + Side * 70, "beside");
		StepTime = 0;
		LogTime = 0;
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
	LogTime += Delta;
	if (Phase == 1)
	{
		if (LogTime >= 0.5)
		{
			LogTime = 0;
			LogCrates(P, "closed");
		}
		if (StepTime > 2.0 && !bMarking && MarkShot == 0)
			Mark(0, "the door closed");
		if (StepTime > 2.8 && bMarking && MarkShot == 0)
		{
			P.ConsoleCommand("shot");
			bMarking = false;
			MarkShot = -1;
		}
		if (StepTime > 4.0)
		{
			Log("DXDOORHIDE: the door opened at " $ MapTime);
			Door.DoOpen();
			StepTime = 0;
			LogTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		if (LogTime >= 0.5)
		{
			LogTime = 0;
			LogCrates(P, "opening");
		}
		if (StepTime > 4.5 && !bMarking && MarkShot == -1)
			Mark(1, "the door open");
		if (StepTime > 5.3 && bMarking && MarkShot == 1)
		{
			P.ConsoleCommand("shot");
			bMarking = false;
			MarkShot = -2;
		}
		if (StepTime > 6.0)
		{
			Log("DXDOORHIDE: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
		}
	}
}

// The mark for the original's screen grabber, as CaptureConsole's: a
// magenta block, then the shot's number in eight blocks
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
