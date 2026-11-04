//=============================================================================
// MipConsole: the mip a surface is drawn from, and whether a point-sampled one
// takes one texel at a slant (docs/DEVELOPMENT.md, scripted runs; bugs.md
// D.40, D.41). At Liberty Island's start, 6 s in, the player is lifted Lift
// units (400) off the pier, no collision, no physics, the view level and
// the start's way, the HUD hidden. A CapQuad is laid flat Height units (16)
// under the eye, Span units (512) across, its near edge 5 units ahead, with
// tools/dxcap/testtextures.py's CapMips for its skin, 16 times across. The
// view's lower half is then a floor whose texels shrink from some 0.7 of a
// pixel at the screen's bottom to 256 a pixel at the horizon, the screen's
// middle row. A pixel's colour names its mip, and a colour between that
// mip's two shades means it took more than one texel. Two marked shots are
// taken, as CaptureConsole marks them: the quad smooth (1), then bNoSmooth
// (2), point-sampled in both engines (D3DDrv's PF_NoSmooth). Then exit.
// Each line starts "DXMIP:"; tools/dxcap/mips.py reads the shots. Read at
// DXCAP_WINDOW=1024x768.
//=============================================================================
class MipConsole extends Console;

// CapMips is DXCapture's own: ScriptedConsole's #exec loads CapTextures.utx
// into the package.

var config float Lift, Height, Span;

var float MapTime, StepTime;
var int Phase;
var int MarkShot;
var vector Spot;
var rotator View;
var CapQuad Quad;

function Say(string S)
{
	Log("DXMIP: " $ S);
}

function Hold(PlayerPawn P)
{
	P.SetLocation(Spot);
	P.Velocity = vect(0, 0, 0);
	P.Acceleration = vect(0, 0, 0);
	P.ViewRotation = View;
}

function bool LayQuad(PlayerPawn P)
{
	local vector HitLocation, HitNormal, Eye, Ahead;
	local Actor HitActor;
	local rotator Flat;
	local Texture Mips;
	local float Up;

	Mips = Texture(DynamicLoadObject("DXCapture.CapMips", class'Texture'));
	if (Mips == None)
	{
		Say("no DXCapture.CapMips");
		return False;
	}
	// Lifted as far as Lift, short of anything over the start
	Up = Lift;
	HitActor = P.Trace(HitLocation, HitNormal, P.Location + vect(0, 0, 1) * (Lift + 100), P.Location, False);
	if (HitActor != None)
		Up = FMax(0, HitLocation.Z - P.Location.Z - 100);
	Spot = P.Location + vect(0, 0, 1) * Up;
	View = P.ViewRotation;
	View.Pitch = 0;
	View.Roll = 0;
	P.SetCollision(False, False, False);
	P.bCollideWorld = False;
	P.SetPhysics(PHYS_None);
	Hold(P);

	Eye = Spot + vect(0, 0, 1) * P.EyeHeight;
	Flat = View;
	Ahead = vector(Flat);
	Quad = P.Spawn(class'CapQuad',,, Eye - vect(0, 0, 1) * Height + Ahead * (Span / 2 + 5), Flat);
	if (Quad == None)
	{
		Say("no CapQuad spawned");
		return False;
	}
	Quad.SetRotation(Flat);
	Quad.DrawScale = Span / 1920;
	Quad.Skin = Mips;
	Quad.bNoSmooth = False;
	Say("lifted " $ Up $ " to " $ Spot $ ", the eye at " $ Eye $ " (eye height " $ P.EyeHeight $ "), view " $ View);
	Say("CapQuad at " $ Quad.Location $ ", rotation " $ Quad.Rotation $ ", draw scale " $ Quad.DrawScale
		$ ", skin " $ Quad.Skin $ " " $ Mips.USize $ " by " $ Mips.VSize);
	return True;
}

function Shoot(PlayerPawn P, string What)
{
	Say("shot " $ MarkShot $ ": " $ What $ ", the eye at " $ (P.Location + vect(0, 0, 1) * P.EyeHeight)
		$ ", view " $ P.ViewRotation);
	P.ConsoleCommand("shot");
	MarkShot = 0;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	if (Phase >= 2 && Phase < 9)
		Hold(P);

	if (Phase == 0)
	{
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") < 0)
		{
			if (MapTime > 3.0)
			{
				Say("opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				MapTime = 0;
				Phase = 1;
			}
		}
		else
		{
			MapTime = 0;
			Phase = 1;
		}
	}
	else if (Phase == 1)
	{
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") >= 0 && MapTime > 6.0 && DXP != None)
		{
			if (DeusExRootWindow(DXP.rootWindow) != None)
				DeusExRootWindow(DXP.rootWindow).ShowHud(False);
			if (!LayQuad(P))
			{
				Say("exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2 && StepTime > 0.6)
	{
		// Settled: the smooth quad marked for the grabber, then shot
		MarkShot = 1;
		StepTime = 0;
		Phase = 3;
	}
	else if (Phase == 3 && StepTime > 0.6)
	{
		Shoot(P, "smooth");
		StepTime = 0;
		Phase = 4;
	}
	else if (Phase == 4 && StepTime > 0.2)
	{
		// The mark gone from the frames before the quad changes
		Quad.bNoSmooth = True;
		Say("bNoSmooth " $ Quad.bNoSmooth);
		StepTime = 0;
		Phase = 5;
	}
	else if (Phase == 5 && StepTime > 0.4)
	{
		MarkShot = 2;
		StepTime = 0;
		Phase = 6;
	}
	else if (Phase == 6 && StepTime > 0.6)
	{
		Shoot(P, "NoSmooth");
		StepTime = 0;
		Phase = 7;
	}
	else if (Phase == 7 && StepTime > 0.4)
	{
		Say("2 shots; exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}

event PostRender(canvas C)
{
	local int i;

	Super.PostRender(C);
	if (MarkShot == 0)
		return;
	// The original's grabber keeps the frames marked as CaptureConsole marks
	// them: a magenta block, then the shot's number in eight blocks.
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

defaultproperties
{
	Lift=400
	Height=16
	Span=512
}
