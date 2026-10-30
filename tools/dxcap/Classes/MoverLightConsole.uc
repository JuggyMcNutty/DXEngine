//=============================================================================
// MoverLightConsole: a mover lit where it is, shot in both engines
// (docs/DEVELOPMENT.md, scripted runs; docs/NATIVES.md, lighting). On
// TargetMap ([DXCapture.MoverLightConsole], 01_NYC_UNATCOIsland by default),
// the player out of the world, the HUD hidden, conversations and datalinks
// stopped; 3 s in, the first door in name order (a DeusExMover with bIsDoor,
// no glass, no bDynamicLightMover), or the one named DoorName, made not to
// collide and moved Distance units (100 by default) along its box's
// thinnest side, as MoverConsole moves it; the view Ahead units (160 by
// default) further that way from the box's middle, facing it. Shot 0: the
// door lit where it now is, by the lights the editor lit it with. Then a
// CapLamp, a moving light, spawned 48 units in front of the door with a
// radius of 3, reaching 100 units: the door where it is, not where it was
// (shot 1); then an exit. Each shot is marked for the original's grabber; each line
// is logged with "DXMOVERLIGHT:" in front.
//=============================================================================
class MoverLightConsole extends Console;

var config string TargetMap;
var config string DoorName;
var config float Distance;
var config float Ahead;

var float MapTime;
var string CurrentMap;
var int Phase;
var int MarkShot;
var bool bMarking;
var bool bOut;
var DeusExMover Door;
var vector Dir, Middle, Eye;
var rotator Look;
var CapLamp Lamp;
var Object Stopped;

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

// The door, and its box's middle and thinnest side's axis
function bool Choose(PlayerPawn P)
{
	local DeusExMover M;
	local vector Min, Max, E;

	foreach P.AllActors(class'DeusExMover', M)
	{
		if (M.IsA('BreakableGlass') || !M.bIsDoor || M.bDynamicLightMover)
			continue;
		if (DoorName != "")
		{
			if (string(M.Name) ~= DoorName)
				Door = M;
		}
		else if (Door == None || Caps(string(M.Name)) < Caps(string(Door.Name)))
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
	Log("DXMOVERLIGHT: shot " $ N $ ", " $ What $ ", from " $ Eye $ " facing " $ Look);
	Log("DXCAP: shot " $ N);
	P.ConsoleCommand("shot");
	bMarking = false;
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
		// The original starts in its menu map
		if (!(CurrentMap ~= Target()))
		{
			if (MapTime > 3.0)
			{
				Log("DXMOVERLIGHT: opening " $ Target());
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
			Log("DXMOVERLIGHT: no door, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		if (Distance == 0)
			Distance = 100;
		if (Ahead == 0)
			Ahead = 160;
		Log("DXMOVERLIGHT: door " $ Door.Name $ " at " $ Door.Location $ ", key " $ Door.KeyNum $ ", raytrace key " $ Door.BrushRaytraceKey $ ", its middle " $ Middle $ ", moved " $ Distance $ " along " $ Dir);
		Door.SetCollision(false, false, false);
		Door.SetLocation(Door.Location + Dir * Distance);
		Middle += Dir * Distance;
		Eye = Middle + Dir * Ahead;
		Look = rotator(-Dir);
		DeusExPlayer(P).ShowHud(False);
		Hold(P);
		Log("DXMOVERLIGHT: door now at " $ Door.Location $ ", the view at " $ Eye);
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
		Shoot(P, 0, "the door where it now is");
		Phase = 3;
	}
	else if (Phase == 3 && MapTime > 5.5)
	{
		Lamp = P.Spawn(class'CapLamp',,, Middle + Dir * 48);
		if (Lamp == None)
		{
			Log("DXMOVERLIGHT: no lamp spawned, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		Lamp.SetPhysics(PHYS_None);
		Lamp.LightRadius = 3;
		Log("DXMOVERLIGHT: lamp at " $ Lamp.Location $ " radius " $ Lamp.LightRadius);
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
		Shoot(P, 1, "the lamp before the door");
		Phase = 6;
	}
	else if (Phase == 6 && MapTime > 8.0)
	{
		Log("DXMOVERLIGHT: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
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
