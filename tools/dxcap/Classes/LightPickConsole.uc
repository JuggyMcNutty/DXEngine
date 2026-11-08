//=============================================================================
// LightPickConsole (bugs.md D.52, REN.49): a mesh's pick of its lights and
// their fades, frame by frame, for tools/dxcap/burst.py (DXCAP_BURST, at
// DXCAP_FPS=60). Render.dll's SetupForActor (0x10b08c70) takes as a
// candidate every light kept under the actor, every static light reaching
// into its leaf and every moving light in it -- one off or of no
// brightness too, though AddLight (0x10b08b30) lights nothing by it -- and
// gives a cylinder light three quarters of its strength and radius, its
// reach tested at the whole radius; a light new to the actor from the
// leaf's moving lights starts at 255 and lit, one from its static lights at
// 0 and dark until its line is run.
//
// On TargetMap ([DXCapture.LightPickConsole], 01_NYC_UNATCOIsland by
// default), 3 s in, the player out of the world at its start, level and
// facing the start's way, the HUD hidden, every pawn gone, conversations
// and datalinks stopped: a bare mesh (CrateUnbreakableSmall's, collision
// off, lit, still) Ahead (100) units ahead at the eye's height, filling the
// view's middle, and per Scene:
//
// - Off (D.52's first): a white CapLamp 30 units over the crate and a red
//   one (hue 0, saturation 0, brightness 64) 150 units to its right, both
//   radius 8 (225 units); the red one's strength at the crate is under an
//   eighth of the white one's, so neither engine lights it: as it falls
//   out of the pass it fades out over some 20 frames, and as a moving light
//   of the leaf new to the crate once its fade is gone it comes back at
//   255, a sawtooth of red on the crate. At the burst's frame 20 the white
//   lamp is switched off (LightType LT_None). The original: it still
//   counts, lighting nothing, and the red sawtooth goes on; DXEngine as it
//   left an off light out: the red lamp lit from that frame, its fade up to
//   255 and held.
// - Cylinder (D.52's second): a white CapLamp of LE_Cylinder, radius 8, 110
//   units to the crate's right, moved at frame 20 to 190 units, past three
//   quarters of its radius (169) and within it. The original: its strength
//   there below 0, its light fades out over a third of a second, some 20
//   frames, and as a moving light comes back at 255 to fade again, a
//   sawtooth (a static cylinder light, of the leaf's static lights, fades
//   out once); DXEngine as it dropped it: out at once and for good.
// - Late (REN.49): no lamp until frame 20, then a white CapLamp made 60
//   units over the crate and given bMovable False, a moving light of the
//   leaf that is not movable, its line run only every 16 frames. The
//   original: the crate lit full from the lamp's first frame; DXEngine as it
//   started such a light at 0: dark until its line is run, then fading up
//   over a third of a second.
//
// Then Burst (90) frames in a row each carry the mark with its number, as
// LightConsole's Burst marks them, each logged ("DXPICK: frame n time t")
// with the lamps' state; then exit. Compare burst.csv's frames 15 to 60.
//=============================================================================
class LightPickConsole extends Console;

var config string TargetMap;
var config string Scene;
var config float Ahead;
var config int Burst;

var float MapTime, StepTime;
var string CurrentMap;
var int Phase;
var bool bOut;
var vector Eye, Spot, Side;
var rotator Look;
var Object Stopped;
var Actor Crate;
var Light White, Red;

var bool bBursting;
var int BurstFrame;
var int MarkShot;
var bool bMarking;

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

function Light Lamp(PlayerPawn P, vector Where)
{
	local Light L;

	L = P.Spawn(class'CapLamp',,, Where);
	if (L == None)
		Log("DXPICK: no lamp at " $ Where);
	return L;
}

function Setup(PlayerPawn P)
{
	local rotator R;

	Look = P.Rotation;
	Look.Pitch = 0;
	Look.Roll = 0;
	Eye = P.Location + vect(0,0,1) * P.EyeHeight;
	if (Ahead <= 0)
		Ahead = 100;
	Spot = Eye + vector(Look) * Ahead;
	R = Look;
	R.Yaw += 16384;
	Side = vector(R);

	Crate = P.Spawn(class'Effects',,, Spot, Look);
	if (Crate != None)
	{
		Crate.SetCollision(false, false, false);
		Crate.bCollideWorld = false;
		Crate.SetPhysics(PHYS_None);
		Crate.LifeSpan = 0;
		Crate.bHidden = false;
		Crate.DrawType = DT_Mesh;
		Crate.Style = STY_Normal;
		Crate.Mesh = class'CrateUnbreakableSmall'.default.Mesh;
		Crate.Skin = class'CrateUnbreakableSmall'.default.Skin;
		Crate.DrawScale = class'CrateUnbreakableSmall'.default.DrawScale;
		Crate.bUnlit = false;
		Crate.ScaleGlow = 1.0;
		Crate.AmbientGlow = 0;
	}
	else
		Log("DXPICK: no crate at " $ Spot);

	if (Scene ~= "Off")
	{
		White = Lamp(P, Spot + vect(0,0,30));
		Red = Lamp(P, Spot + Side * 150);
		if (Red != None)
		{
			Red.LightHue = 0;
			Red.LightSaturation = 0;
			Red.LightBrightness = 64;
		}
	}
	else if (Scene ~= "Cylinder")
	{
		White = Lamp(P, Spot + Side * 110);
		if (White != None)
			White.LightEffect = LE_Cylinder;
	}
	Log("DXPICK: scene " $ Scene $ " eye " $ Eye $ " crate " $ Spot $ " zone " $ Crate.Region.Zone.Name);
}

// The burst's change, at its frame 20
function Change(PlayerPawn P)
{
	if (Scene ~= "Off")
	{
		if (White != None)
			White.LightType = LT_None;
	}
	else if (Scene ~= "Cylinder")
	{
		if (White != None)
			White.SetLocation(Spot + Side * 190);
	}
	else if (Scene ~= "Late")
	{
		White = Lamp(P, Spot + vect(0,0,60));
		if (White != None)
			White.bMovable = false;
	}
}

function MarkFrame()
{
	local PlayerPawn P;

	if (BurstFrame >= Burst)
	{
		bBursting = false;
		bMarking = false;
		return;
	}
	P = Viewport.Actor;
	if (BurstFrame == 20)
		Change(P);
	MarkShot = BurstFrame & 255;
	if (White != None)
		Log("DXPICK: frame " $ BurstFrame $ " time " $ P.Level.TimeSeconds $ " white type " $ int(White.LightType)
		    $ " at " $ VSize(White.Location - Spot) $ " movable " $ White.bMovable);
	else
		Log("DXPICK: frame " $ BurstFrame $ " time " $ P.Level.TimeSeconds $ " no white lamp");
	BurstFrame++;
}

event PostRender(canvas C)
{
	local int i;
	Super.PostRender(C);
	if (bBursting)
		MarkFrame();
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
				Log("DXPICK: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Phase = -1;
			}
			return;
		}
		if (MapTime < 3.0 || DeusExPlayer(P) == None)
			return;
		if (!bOut)
		{
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			P.SetPhysics(PHYS_None);
			bOut = true;
		}
		if (Burst <= 0)
			Burst = 90;
		Hush(P);
		DeusExPlayer(P).ShowHud(False);
		Setup(P);
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
	if (Phase == 1 && StepTime > 3.0)
	{
		Log("DXPICK: burst of " $ Burst $ " frames");
		bBursting = true;
		bMarking = true;
		Phase = 2;
	}
	else if (Phase == 2 && !bBursting && StepTime > 3.0)
	{
		Log("DXPICK: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
	}
}
