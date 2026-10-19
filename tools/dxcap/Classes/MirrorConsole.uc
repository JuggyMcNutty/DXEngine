//=============================================================================
// MirrorConsole: mirrors, shot from fixed views in both engines
// (docs/NATIVES.md, mirrors). On TargetMap ([DXCapture.MirrorConsole],
// Liberty Island by default):
// - Liberty Island: the UNATCO HQ building's lobby, whose marble floor is a
//   mirror -- from nine yaws 5 degrees apart and five places 40 units apart
//   (turns and steps, where a reflection walked from the wrong eye pops in
//   and out), looking down at the player's own reflection, and toward the
//   lobby's chairs and lamp -- then the island's two small wall mirrors,
//   which face -y. A mannequin, a UNATCO troop's mesh, unlit and still,
//   stands on the floor in view.
// - 01_NYC_UNATCOHQ: a glass mirror facing -x (the mirror's matrix along
//   x), a washroom's glass facing -y, the two facing +x, and the marble
//   floor near the entrance.
// - 00_TrainingFinal: the marble floor, a mirror, with a mirror strip over
//   it, which shows in the floor's reflection (a mirror seen in a mirror).
// - 10_Paris_Club: a room whose two glass mirrors face each other across
//   it, past a pillar: one seen straight on, then each from aside.
// Each view puts the player's feet on the ground under a spot, facing as
// given, still and in the world (its reflection drawn), its collision off,
// unseen by the NPCs, the HUD hidden, conversations and datalinks stopped
// as they start. A view is held 1.5 s, then marked for the original's
// grabber MarkTime s (0.6 by default) and shot. Then exit. With ShinyOff
// the render device's ShinySurfaces is set off first, each engine's by its
// class: a translucent mirror is then drawn solid, reflecting nothing.
// With Straddle the lobby's mannequin stands sunk half its height into the
// floor, across the mirror's plane: from the reflected eye its lower half
// lies before the plane, where a device that clips the mirror's frame at
// its plane draws none of it, and one that does not draws it over the
// reflection (docs/NATIVES.md, mirrors).
// Each line is logged with "DXMIRROR:" in front.
//=============================================================================
class MirrorConsole extends Console;

var config string TargetMap;
var config float MarkTime;
var config bool ShinyOff;
var config bool Straddle;

var string CurrentMap;
var float MapTime;
var float StepTime;
var int Phase;
var int Views;
var int ViewIndex;
var int ShotsTaken;
var vector Spots[32];
var rotator Facings[32];
var string Names[32];
var vector Stand;
var vector MannequinSpot;
var Effects Mannequin;
var Object Stopped;

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

function string Target()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOIsland";
	return TargetMap;
}

function float Mark()
{
	if (MarkTime <= 0)
		return 0.6;
	return MarkTime;
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
		Log("DXMIRROR: datalink " $ Stopped.Name $ " stopped");
		DXP.dataLinkPlay.AbortDataLink();
	}
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		Log("DXMIRROR: conversation " $ Stopped.Name $ " stopped");
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

// vect and rot take constants alone
function vector Spot(float X, float Y, float Z)
{
	local vector S;
	S.X = X;
	S.Y = Y;
	S.Z = Z;
	return S;
}

function rotator Facing(int Pitch, int Yaw)
{
	local rotator F;
	F.Pitch = Pitch;
	F.Yaw = Yaw;
	return F;
}

function AddView(string N, vector S, rotator F)
{
	if (Views >= ArrayCount(Spots))
		return;
	Names[Views] = N;
	Spots[Views] = S;
	Facings[Views] = F;
	Views++;
}

// The views of the map: a spot over the ground, and the view's turn
function SetupViews(string M)
{
	local int k;

	Views = 0;
	if (M ~= "01_NYC_UNATCOHQ")
	{
		AddView("glass facing -x", Spot(2364,1968,-48), Facing(0,0));
		AddView("washroom glass facing -y", Spot(256,250,30), Facing(0,16384));
		AddView("glass facing +x at 161", Spot(241,-344,330), Facing(0,32768));
		AddView("glass facing +x at -2094", Spot(-2014,1040,648), Facing(0,32768));
		AddView("marble floor", Spot(-904,340,450), Facing(-5461,16384));
		return;
	}
	if (M ~= "00_TrainingFinal")
	{
		AddView("marble floor, a mirror strip over it", Spot(6576,-5800,60), Facing(-6371,49152));
		return;
	}
	if (M ~= "10_Paris_Club")
	{
		AddView("club glass facing -x", Spot(-1300,-1436,-150), Facing(0,0));
		AddView("club glass facing -x, from aside", Spot(-1100,-1520,-150), Facing(0,7640));
		AddView("club glass facing +x, from aside", Spot(-1490,-1000,-150), Facing(0,40408));
		return;
	}
	// The lobby, from inside its doors, toward the couch: turned, then
	// stepped across
	for (k = -4; k <= 4; k++)
		AddView("lobby turned " $ (k * 5), Spot(-4600,2200,-200), Facing(-2731,49152 + k * 910));
	for (k = -2; k <= 2; k++)
		AddView("lobby stepped " $ (k * 40), Spot(-4600 + k * 40,2200,-200), Facing(-2731,49152));
	AddView("lobby looking down", Spot(-4640,2000,-200), Facing(-10923,49152));
	AddView("lobby toward the chairs", Spot(-4520,1950,-200), Facing(-3641,32768));
	AddView("wall mirrors facing -y", Spot(1758,-1330,-56), Facing(0,16384));
	MannequinSpot = Spot(-4650,1950,-200);
}

// The ground under a spot, or the spot where none is near
function vector Ground(PlayerPawn P, vector S)
{
	local vector HitLocation, HitNormal;
	if (P.Trace(HitLocation, HitNormal, S - vect(0,0,512), S, false) != None)
		return HitLocation;
	return S;
}

function StandMannequin(PlayerPawn P)
{
	local int i;

	if (MannequinSpot == vect(0,0,0))
		return;
	Mannequin = P.Spawn(class'Effects',,, Ground(P, MannequinSpot) + vect(0,0,1) * class'UNATCOTroop'.default.CollisionHeight, rot(0,16384,0));
	if (Mannequin == None)
	{
		Log("DXMIRROR: no mannequin");
		return;
	}
	Mannequin.SetCollision(false, false, false);
	Mannequin.bCollideWorld = false;
	Mannequin.SetPhysics(PHYS_None);
	Mannequin.LifeSpan = 0;
	Mannequin.bHidden = false;
	Mannequin.DrawType = DT_Mesh;
	Mannequin.Style = STY_Normal;
	Mannequin.Mesh = class'UNATCOTroop'.default.Mesh;
	Mannequin.Skin = class'UNATCOTroop'.default.Skin;
	for (i = 0; i < 8; i++)
		Mannequin.MultiSkins[i] = class'UNATCOTroop'.default.MultiSkins[i];
	Mannequin.DrawScale = class'UNATCOTroop'.default.DrawScale;
	// Unlit, so the two engines' lighting does not tell them apart
	Mannequin.bUnlit = true;
	if (Straddle)
		Mannequin.SetLocation(Mannequin.Location - vect(0,0,1) * class'UNATCOTroop'.default.CollisionHeight);
	Log("DXMIRROR: mannequin " $ Mannequin.Name $ " at " $ Mannequin.Location $ " mesh " $ Mannequin.Mesh $ " zone " $ Mannequin.Region.Zone.Name
		$ " straddling " $ Straddle);
}

// The player's feet on the ground under the view's spot
function BeginView(PlayerPawn P)
{
	Stand = Ground(P, Spots[ViewIndex]) + vect(0,0,1) * (P.CollisionHeight + 1);
	HoldView(P);
	Log("DXMIRROR: view " $ ViewIndex $ " " $ Names[ViewIndex] $ " standing at " $ Stand $ " zone " $ P.Region.Zone.Name);
}

function HoldView(PlayerPawn P)
{
	local rotator Turn;

	P.SetPhysics(PHYS_None);
	P.Velocity = vect(0,0,0);
	P.Acceleration = vect(0,0,0);
	P.EyeHeight = P.BaseEyeHeight;
	P.SetLocation(Stand);
	P.ViewRotation = Facings[ViewIndex];
	Turn.Yaw = Facings[ViewIndex].Yaw;
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
	if (DXP != None && (M ~= Target()))
		Silence(DXP);

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (!(M ~= Target()))
		{
			if (MapTime > 3.0)
			{
				Log("DXMIRROR: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Phase = -1;
			}
		}
		else if (MapTime > 6.0 && DXP != None)
		{
			SetupViews(M);
			Log("DXMIRROR: " $ Views $ " views on " $ M);
			DXP.KillShadow();
			P.bDetectable = false;
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			// The original's HUD draws over the grabber's mark.
			if (DeusExRootWindow(DXP.rootWindow) != None)
				DeusExRootWindow(DXP.rootWindow).ShowHud(False);
			StandMannequin(P);
			if (ShinyOff)
			{
				// Each engine takes its own render device's; the others' classes
				// it does not have
				P.ConsoleCommand("set OpenGLDrv.OpenGLRenderDevice ShinySurfaces False");
				P.ConsoleCommand("set D3DDrv.D3DRenderDevice ShinySurfaces False");
				P.ConsoleCommand("set Engine.DXEngineRenderDevice ShinySurfaces False");
				Log("DXMIRROR: ShinySurfaces set off");
			}
			ViewIndex = 0;
			BeginView(P);
			StepTime = 0;
			Phase = 1;
		}
	}
	else if (Phase == -1 && (M ~= Target()))
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
			Log("DXMIRROR: shot " $ ShotsTaken $ " " $ Names[ViewIndex] $ " eye " $ (P.Location + vect(0,0,1) * P.EyeHeight)
				$ " rotation " $ P.ViewRotation $ " FOV " $ P.FOVAngle);
			P.ConsoleCommand("shot");
			bMarking = false;
			ShotsTaken++;
			ViewIndex++;
			StepTime = 0;
			if (ViewIndex >= Views)
				Phase = 2;
			else
				BeginView(P);
		}
	}
	else if (Phase == 2 && StepTime > 1.0)
	{
		Log("DXMIRROR: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
