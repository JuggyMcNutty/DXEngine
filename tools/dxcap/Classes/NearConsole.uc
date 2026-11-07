//=============================================================================
// NearConsole: what is drawn nearer than a unit before the eye
// (docs/DEVELOPMENT.md, scripted runs; bugs.md REN.37). At Liberty Island's
// start, 6 s in, the player is lifted Lift units (400) off the pier, no
// collision, no physics, the view level and the start's way, the HUD hidden.
// A CapQuad with the level's DefaultTexture for its skin stands across the
// view, facing the eye, at each of Distances ([DXCapture.NearConsole]: 0.5,
// 1.5 and 4 units by default) before the eye, its points half a tile off the
// view's axis, so that every point near the eye lies outside the view's four
// sides and the original clips its faces there (render-dll.md, which faces
// are drawn); then, unless NoSlant, tilted back about the point a unit
// ahead, so that it crosses view depth 1 on the screen's middle row, nearer
// below it (some 0.6 units at the bottom of a 4:3 view) and farther above.
// Each view is held 0.6 s, marked for the original's grabber as
// CaptureConsole marks them, then shot. Then exit. Each line starts
// "DXNEAR:". Read at DXCAP_WINDOW=1024x768, the original with
// DXCAP_RENDERER=D3D.
//=============================================================================
class NearConsole extends Console;

var config float Lift;
var config float Distances[8];
var config bool NoSlant;

var float MapTime, StepTime;
var int Phase;
var int ViewIndex;
var int ShotsTaken;
var int MarkShot;
var bool Slanted;
var vector Spot;
var rotator View;
var CapQuad Quad;

function Say(string S)
{
	Log("DXNEAR: " $ S);
}

function Hold(PlayerPawn P)
{
	P.SetLocation(Spot);
	P.Velocity = vect(0, 0, 0);
	P.Acceleration = vect(0, 0, 0);
	P.ViewRotation = View;
}

// The distance of view i, 0 past the last
function float Distance(int i)
{
	local float Defaults[3];

	if (Distances[0] > 0)
	{
		if (i < ArrayCount(Distances))
			return Distances[i];
		return 0;
	}
	Defaults[0] = 0.5;
	Defaults[1] = 1.5;
	Defaults[2] = 4;
	if (i < ArrayCount(Defaults))
		return Defaults[i];
	return 0;
}

function bool Lifted(PlayerPawn P)
{
	local vector HitLocation, HitNormal;
	local Actor HitActor;
	local Texture Skin;
	local float Up;

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

	Quad = P.Spawn(class'CapQuad',,, Spot + vector(View) * 64, View);
	if (Quad == None)
	{
		Say("no CapQuad spawned");
		return False;
	}
	Skin = P.Level.DefaultTexture;
	if (Skin == None)
		Skin = Texture(DynamicLoadObject("Engine.DefaultTexture", class'Texture'));
	Quad.Skin = Skin;
	Quad.DrawScale = 1;
	Say("lifted " $ Up $ " to " $ Spot $ ", the eye at " $ (Spot + vect(0, 0, 1) * P.EyeHeight) $ ", view " $ View
		$ ", fov " $ P.FovAngle $ ", skin " $ Skin);
	return True;
}

// The quad across the view: upright, facing the eye, Ahead units before it;
// or slanted, its rise along the view 1 for each 1.2 units farther, through
// the point a unit ahead. Its points lie 120 units apart from its centre:
// moved half a tile along both of its sides, none comes near the view's axis.
function PlaceQuad(PlayerPawn P, float Ahead, bool Slant)
{
	local vector Eye, X, Y, Z;
	local rotator R;

	Eye = Spot + vect(0, 0, 1) * P.EyeHeight;
	R = View;
	if (Slant)
		R.Pitch = 7245; // atan(1 / 1.2): 39.8 degrees up
	else
		R.Pitch = 16384;
	GetAxes(R, X, Y, Z);
	Quad.SetRotation(R);
	Quad.SetLocation(Eye + vector(View) * Ahead + X * 60 + Y * 60);
	Say("view " $ ViewIndex $ ": the eye at " $ Eye $ ", the quad at " $ Quad.Location $ ", rotation " $ Quad.Rotation
		$ ", " $ Ahead $ " units ahead on the view's axis, slanted " $ Slant);
}

function Shoot(PlayerPawn P)
{
	ShotsTaken++;
	Say("shot " $ ShotsTaken $ ": view " $ ViewIndex);
	P.ConsoleCommand("shot");
	MarkShot = 0;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local float Ahead;

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
			if (!Lifted(P))
			{
				Say("exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			ViewIndex = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		Ahead = Distance(ViewIndex);
		if (Ahead > 0)
			PlaceQuad(P, Ahead, False);
		else if (!NoSlant && !Slanted)
		{
			Slanted = True;
			PlaceQuad(P, 1, True);
		}
		else
		{
			Say(ShotsTaken $ " shots; exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		StepTime = 0;
		Phase = 3;
	}
	else if (Phase == 3 && StepTime > 0.6)
	{
		// Settled: marked for the grabber, then shot
		MarkShot = ShotsTaken + 1;
		StepTime = 0;
		Phase = 4;
	}
	else if (Phase == 4 && StepTime > 0.6)
	{
		Shoot(P);
		StepTime = 0;
		Phase = 5;
	}
	else if (Phase == 5 && StepTime > 0.4)
	{
		// The mark gone from the frames before the quad moves
		ViewIndex++;
		Phase = 2;
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
}
