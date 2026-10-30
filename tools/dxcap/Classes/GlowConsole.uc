//=============================================================================
// GlowConsole: an actor's pulsing AmbientGlow, shot in both engines
// (docs/DEVELOPMENT.md, scripted runs; docs/NATIVES.md, lighting). On
// TargetMap ([DXCapture.GlowConsole], 01_NYC_UNATCOIsland by default), the
// player out of the world, the HUD hidden, conversations and datalinks
// stopped; 3 s in, a crate (CrateUnbreakableSmall, not to collide) spawned
// Ahead units (96 by default) before the player's eye, its AmbientGlow 255,
// the pulse, and the level's TimeDilation set to Dilation (0.01 by default),
// so the level's time crawls while the viewport's runs on. Then Shots shots
// (6 by default), one every 1.5 s of the console's clock, the console ticking
// by the viewport's time: the pulse, 0.25 + 0.2 sin(8 t) of the viewport's
// time, comes out at whatever phase each shot finds it, where on the level's
// time it would barely move. Each shot is marked for the original's grabber;
// each line is logged with "DXGLOW:" in front.
//=============================================================================
class GlowConsole extends Console;

var config string TargetMap;
var config float Ahead;
var config float Dilation;
var config int Shots;

var float MapTime;
var float NextShot;
var string CurrentMap;
var int Phase;
var int MarkShot;
var int Taken;
var bool bMarking;
var bool bOut;
var DeusExDecoration Crate;
var vector Eye;
var rotator Look;
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
				Log("DXGLOW: opening " $ Target());
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
		if (Ahead == 0)
			Ahead = 96;
		if (Dilation == 0)
			Dilation = 0.01;
		if (Shots == 0)
			Shots = 6;
		Eye = P.Location + vect(0,0,1) * P.EyeHeight;
		Look = P.ViewRotation;
		Look.Pitch = 0;
		Crate = P.Spawn(class'CrateUnbreakableSmall',,, Eye + vector(Look) * Ahead);
		if (Crate == None)
		{
			Log("DXGLOW: no crate spawned, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		Crate.SetCollision(false, false, false);
		Crate.bCollideWorld = false;
		Crate.SetPhysics(PHYS_None);
		Crate.AmbientGlow = 255;
		DeusExPlayer(P).ShowHud(False);
		Hold(P);
		P.Level.TimeDilation = Dilation;
		Log("DXGLOW: crate at " $ Crate.Location $ ", AmbientGlow " $ Crate.AmbientGlow $ ", the view at " $ Eye
			$ " facing " $ Look $ ", TimeDilation " $ P.Level.TimeDilation);
		NextShot = MapTime + 1.0;
		Phase = 1;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= Target())
			Phase = 0;
		return;
	}

	Hold(P);
	if (Phase == 1 && MapTime > NextShot)
	{
		MarkShot = Taken;
		bMarking = true;
		Phase = 2;
	}
	else if (Phase == 2 && MapTime > NextShot + 0.8)
	{
		Log("DXGLOW: shot " $ Taken $ " at " $ MapTime $ " s, the level's time " $ P.Level.TimeSeconds);
		Log("DXCAP: shot " $ Taken);
		P.ConsoleCommand("shot");
		bMarking = false;
		Taken++;
		NextShot += 1.5;
		if (Taken < Shots)
			Phase = 1;
		else
			Phase = 3;
	}
	else if (Phase == 3 && MapTime > NextShot)
	{
		P.Level.TimeDilation = 1.0;
		Log("DXGLOW: done, exiting");
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
