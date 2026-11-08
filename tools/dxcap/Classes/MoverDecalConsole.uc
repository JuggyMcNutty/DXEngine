//=============================================================================
// MoverDecalConsole (bugs.md REN.45, REN.47): where a decal attaches, shot in
// both engines. On TargetMap ([DXCapture.MoverDecalConsole],
// 01_NYC_UNATCOIsland by default), the player out of the world, the HUD
// hidden, conversations and datalinks stopped; 3 s in:
//
// - the first door in name order (a DeusExMover with bIsDoor, no glass), or
//   the mover named DoorName (a BreakableGlass too), the view Ahead units
//   (120 by default) from its box's middle along its thinnest side, facing
//   it. A line from the eye to the middle finds the face; a BulletHole is
//   spawned there owned by the mover and turned along the face's normal, as
//   DeusExWeapon.SpawnEffects spawns one (for glass, the crack texture and
//   a ReattachDecal, as SpawnEffects does). "DXMOVERDECAL: hole" logs
//   whether the spawn kept it (its BeginPlay's AttachDecal(32) true) and
//   shot 0 shows it. Render.dll/Engine.dll attach a decal to a mover's
//   faces the brush tracker filed into the level's BSP (execAttachDecal
//   0x103e7250: the level model's LineCheck, the hit surface's Actor a
//   mover -> SetBase); DXEngine's TraceDecal walks the level's own nodes.
//
// - then, back at the player's start, a ScorchMark at DrawScale Scale (8 by
//   default) laid on the floor 200 units ahead with a fixed direction
//   (ReattachDecal(vect(1,0,0))), the view 300 units over it looking down:
//   shot 1. The original lays it on every node of the floor's surface it
//   covers and on the other surfaces a 4 x 4 grid of lines finds
//   (MultiDecalLevel 4); DXEngine on the hit node's coplanar chain alone.
//
// - then the mover opened (DoOpen) and, 4 s on, the place the hole was laid,
//   carried by the door's turn, seen Ahead units out along the turned face:
//   shot 2, "DXMOVERDECAL: opened" logging the
//   hole's place and base. Render.dll's ClipDecal (0x10b1bda0) turns a
//   decal on a mover's face by the mover's rotation less the one it was
//   laid at (Deus Ex's MoverRotation), about the face's base, which the
//   tracker's filing moves with the mover: the hole stays where it was on
//   the door's face.
//
// Each shot is marked for the original's grabber; each line is logged with
// "DXMOVERDECAL:" in front. Compare: the two logs' "hole" and "opened"
// lines; shots.py --region over the hole (shots 0 and 2) and over the
// scorch mark (shot 1).
//=============================================================================
class MoverDecalConsole extends Console;

var config string TargetMap;
var config string DoorName;
var config float Ahead;
var config float Scale;

var float MapTime;
var string CurrentMap;
var int Phase;
var int MarkShot;
var bool bMarking;
var bool bOut;
var DeusExMover Door;
var vector Dir, Middle, Eye, Start;
var rotator Look, StartLook;
var Object Stopped;
var Decal Hole, Mark;
var vector HoleAt, HoleNormal, DoorAt;
var rotator DoorTurn;

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
		else if (!M.IsA('BreakableGlass') && M.bIsDoor && (Door == None || Caps(string(M.Name)) < Caps(string(Door.Name))))
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

function Hold(PlayerPawn P)
{
	P.SetLocation(Eye - vect(0,0,1) * P.EyeHeight);
	P.SetRotation(Look);
	P.ViewRotation = Look;
	P.Velocity = vect(0,0,0);
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

function Shoot(PlayerPawn P, int N, string What)
{
	Log("DXMOVERDECAL: shot " $ N $ ", " $ What $ ", from " $ Eye $ " facing " $ Look);
	Log("DXCAP: shot " $ N);
	P.ConsoleCommand("shot");
	bMarking = false;
}

// The bullet hole on the mover's face, as DeusExWeapon.SpawnEffects puts one
function PutHole(PlayerPawn P)
{
	local vector HitLocation, HitNormal;
	local Actor HitActor;

	HitActor = P.Trace(HitLocation, HitNormal, Middle, Eye, true);
	Log("DXMOVERDECAL: line to " $ Door.Name $ " met " $ HitActor $ " at " $ HitLocation $ " normal " $ HitNormal);
	if (HitActor == None)
	{
		HitLocation = Middle + Dir * 1;
		HitNormal = Dir;
	}
	HoleAt = HitLocation;
	HoleNormal = HitNormal;
	DoorAt = Door.Location;
	DoorTurn = Door.Rotation;
	Hole = P.Spawn(class'BulletHole', Door,, HitLocation, rotator(HitNormal));
	if (Hole != None && Door.IsA('BreakableGlass'))
	{
		Hole.Texture = Texture'FlatFXTex29';
		Hole.DrawScale = 0.1;
		DeusExDecal(Hole).ReattachDecal();
	}
	if (Hole == None)
		Log("DXMOVERDECAL: hole on " $ Door.Name $ ": None (its AttachDecal refused it)");
	else
		Log("DXMOVERDECAL: hole on " $ Door.Name $ ": " $ Hole.Name $ " base " $ Hole.Base $ " deleted " $ Hole.bDeleteMe);
}

// The big scorch mark on the floor ahead of the start
function PutMark(PlayerPawn P)
{
	local vector HitLocation, HitNormal, Ground;
	local Actor HitActor;

	Ground = Start + vector(StartLook) * 200;
	Ground.Z = Start.Z;
	HitActor = P.Trace(HitLocation, HitNormal, Ground - vect(0,0,400), Ground, false);
	Log("DXMOVERDECAL: floor under " $ Ground $ " met " $ HitActor $ " at " $ HitLocation $ " normal " $ HitNormal);
	Mark = P.Spawn(class'ScorchMark',,, HitLocation + HitNormal, rotator(HitNormal));
	if (Mark != None)
	{
		if (Scale == 0)
			Scale = 8;
		Mark.DrawScale = Scale;
		DeusExDecal(Mark).ReattachDecal(vect(1,0,0));
	}
	Log("DXMOVERDECAL: scorch " $ Mark $ " at DrawScale " $ Scale);
	Eye = HitLocation + vect(0,0,300);
	Look = rot(-16384,0,0);
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
	P.ReducedDamageType = 'All';
	Silence(P);

	if (Phase == 0)
	{
		if (!(CurrentMap ~= Target()))
		{
			if (MapTime > 3.0)
			{
				Log("DXMOVERDECAL: opening " $ Target());
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
		if (!Choose(P))
		{
			Log("DXMOVERDECAL: no mover, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		if (Ahead == 0)
			Ahead = 120;
		Eye = Middle + Dir * Ahead;
		if (!P.FastTrace(Middle, Eye))
		{
			Dir = -Dir;
			Eye = Middle + Dir * Ahead;
		}
		Log("DXMOVERDECAL: mover " $ Door.Name $ " at " $ Door.Location $ ", its middle " $ Middle $ ", the view at " $ Eye);
		DeusExPlayer(P).ShowHud(False);
		Look = rotator(-Dir);
		Hold(P);
		PutHole(P);
		Phase = 1;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= Target())
			Phase = 0;
		return;
	}

	Hold(P);
	if (Phase == 1 && MapTime > 4.5)
	{
		MarkShot = 0;
		bMarking = true;
		Phase = 2;
	}
	else if (Phase == 2 && MapTime > 5.3)
	{
		Shoot(P, 0, "the hole on the mover");
		Phase = 3;
	}
	else if (Phase == 3 && MapTime > 5.5)
	{
		PutMark(P);
		Phase = 4;
	}
	else if (Phase == 4 && MapTime > 6.5)
	{
		MarkShot = 1;
		bMarking = true;
		Phase = 5;
	}
	else if (Phase == 5 && MapTime > 7.3)
	{
		Shoot(P, 1, "the scorch mark on the floor");
		Phase = 6;
	}
	else if (Phase == 6 && MapTime > 8.0)
	{
		Log("DXMOVERDECAL: opening " $ Door.Name);
		Door.DoOpen();
		Phase = 7;
	}
	else if (Phase == 7 && MapTime > 12.0)
	{
		ViewHole(P);
		Log("DXMOVERDECAL: the hole's place on the turned door " $ Middle $ ", the view at " $ Eye);
		if (Hole == None)
			Log("DXMOVERDECAL: opened " $ Door.Name $ " at " $ Door.Location $ " rotation " $ Door.Rotation $ ", no hole");
		else
			Log("DXMOVERDECAL: opened " $ Door.Name $ " at " $ Door.Location $ " rotation " $ Door.Rotation $ ", the hole " $ Hole.Name $ " at " $ Hole.Location $ " base " $ Hole.Base);
		MarkShot = 2;
		bMarking = true;
		Phase = 8;
	}
	else if (Phase == 8 && MapTime > 12.8)
	{
		Shoot(P, 2, "the hole on the opened mover");
		Phase = 9;
	}
	else if (Phase == 9 && MapTime > 13.5)
	{
		Log("DXMOVERDECAL: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
	}
}

// The view Ahead units out along the face the hole was laid on, at the
// place the hole was laid, both carried by the door's turn since
function ViewHole(PlayerPawn P)
{
	local vector N;

	Middle = Door.Location + (((HoleAt - DoorAt) << DoorTurn) >> Door.Rotation);
	N = (HoleNormal << DoorTurn) >> Door.Rotation;
	Dir = N;
	Eye = Middle + N * Ahead;
	Look = rotator(-N);
	Hold(P);
}

// The view Ahead units from the mover's box's middle along its thinnest side,
// facing it, on the side its line to the middle is clear
function ViewDoor(PlayerPawn P)
{
	local vector Min, Max, E;

	if (!Door.GetBoundingBox(Min, Max))
		return;
	Middle = (Min + Max) * 0.5;
	E = (Max - Min) * 0.5;
	if (E.X <= E.Y && E.X <= E.Z)
		Dir = vect(1,0,0);
	else if (E.Y <= E.Z)
		Dir = vect(0,1,0);
	else
		Dir = vect(0,0,1);
	Eye = Middle + Dir * Ahead;
	if (!P.FastTrace(Middle, Eye))
	{
		Dir = -Dir;
		Eye = Middle + Dir * Ahead;
	}
	Look = rotator(-Dir);
	Hold(P);
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
