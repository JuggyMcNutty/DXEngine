//=============================================================================
// BackFaceConsole: a one-sided level surface seen from behind, shot before
// and after its texture is made two-sided, in both engines (bugs.md REN.28;
// docs/NATIVES.md, draw order and movers). On TargetMap
// ([DXCapture.BackFaceConsole], Liberty Island by default), the top of the
// statue's pedestal: its room's south opening, x 2840 to 3112 and z 2472 to
// 2664, holds a chain-link sheet, the level's surface 2267 (node 7877,
// Brush1280), CoreTexMetal.Metal.ClenChainlink_B, its flags 0x80000A
// (masked, not solid, high shadow detail; not two-sided), its plane y = -248
// facing +y, into the room: from the walkway outside it is seen from behind,
// and neither engine draws it. Two views: A on the south walkway at x 2976,
// y -384, 136 units behind the sheet's plane, facing north through the
// opening, which at 4:3 fills the frame but for its last rows (the sheet's
// frame, below); B in the room at x 2976, y -100, facing south, 148 units
// before the sheet, which fills the frame. Each puts the player's feet on
// the ground under its spot, still, its collision off, unseen by the NPCs,
// the HUD hidden, conversations and datalinks stopped as they start, and the
// NPCs within 1600 units of the walkway taken out of the world (LeaveWorld):
// the room's commander stands in view, and nothing then moves in the frame
// (KeepPawns: left in it). A view is held 1.5 s, then marked for the
// original's grabber MarkTime s (2 by default: D3DDrv's window reaches the
// grabber only every 0.2 to 1.4 s) and shot. Shot 0 is A and shot 1 B with
// the texture as it is; then the texture's bTwoSided is set (TextureName:
// another texture), shot 2 A and shot 3 B; then set back, shot 4 A. Then
// views within a unit and more than a unit behind a one-sided surface: the
// original's walk passes over one more than a unit behind the eye and
// passes on one within a unit, which its frame shows none of either. Shot 5
// C and shot 6 D, 0.9 and 1.1 units behind the sheet's plane on the walkway
// at x 2976, facing east turned 11.25 degrees from it, so that the sheet
// would lie along the frame's right, from 1.6 units on; shot 7 E and shot 8
// F, the eye 0.9 and 1.1 units behind a door's face, inside the door:
// DoorName's (DeusExMover12 by default, a door 8 units thick, its box x 568
// to 576, y -16 to 48, z -136 to -8), its +x face at x 576, the eye at y 16,
// z -72, facing +y turned 11.25 degrees from the face, which would lie along
// the frame's left. The NPCs within 1600 units of the door are taken out of
// the world too. Then exit. Each line is logged with "DXBACKFACE:" in
// front.
//=============================================================================
class BackFaceConsole extends Console;

var config string TargetMap;
var config string TextureName;
var config float MarkTime;
var config bool KeepPawns;
var config string DoorName;

var string CurrentMap;
var float MapTime;
var float StepTime;
var int Phase;
var int Step;
var int ShotsTaken;
var Object Stopped;
var Texture Tex;
var bool bStockTwoSided;
var vector Spots[6];
var rotator Facings[6];
var string Names[6];
var Mover Door;
var vector Stand;
var int ViewIndex;

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

function string Target()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOIsland";
	return TargetMap;
}

function string TextureOf()
{
	if (TextureName == "")
		return "CoreTexMetal.Metal.ClenChainlink_B";
	return TextureName;
}

function float Mark()
{
	if (MarkTime <= 0)
		return 2.0;
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
		Log("DXBACKFACE: datalink " $ Stopped.Name $ " stopped");
		DXP.dataLinkPlay.AbortDataLink();
	}
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		Log("DXBACKFACE: conversation " $ Stopped.Name $ " stopped");
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

function string DoorOf()
{
	if (DoorName == "")
		return "DeusExMover12";
	return DoorName;
}

// Each view's spot over the ground and its turn: A behind the sheet, B
// before it, C and D just behind it; E and F the eye itself, in the door
function SetupViews()
{
	Names[0] = "A, behind the sheet, from the walkway";
	Spots[0] = Spot(2976, -384, 2600);
	Facings[0] = Facing(0, 16384);
	Names[1] = "B, before the sheet, from the room";
	Spots[1] = Spot(2976, -100, 2600);
	Facings[1] = Facing(0, 49152);
	Names[2] = "C, 0.9 behind the sheet, along it";
	Spots[2] = Spot(2976, -248.9, 2600);
	Facings[2] = Facing(0, 63488);
	Names[3] = "D, 1.1 behind the sheet, along it";
	Spots[3] = Spot(2976, -249.1, 2600);
	Facings[3] = Facing(0, 63488);
	Names[4] = "E, 0.9 behind the door's face, in the door";
	Spots[4] = Spot(575.1, 16, -72);
	Facings[4] = Facing(0, 18432);
	Names[5] = "F, 1.1 behind the door's face, in the door";
	Spots[5] = Spot(574.9, 16, -72);
	Facings[5] = Facing(0, 18432);
}

// The view of each shot, and whether the texture is two-sided for it
function int ViewOf(int S)
{
	if (S == 1 || S == 3)
		return 1;
	if (S >= 5)
		return S - 3;
	return 0;
}

function bool TwoSidedAt(int S)
{
	return S == 2 || S == 3;
}

// The ground under a spot, or the spot where none is near
function vector Ground(PlayerPawn P, vector S)
{
	local vector HitLocation, HitNormal;
	if (P.Trace(HitLocation, HitNormal, S - vect(0,0,512), S, false) != None)
		return HitLocation;
	return S;
}

function string TextureFlags(Texture T)
{
	return "bTwoSided " $ T.bTwoSided $ " bMasked " $ T.bMasked $ " bTransparent " $ T.bTransparent $ " bModulate " $ T.bModulate
		$ " bInvisible " $ T.bInvisible $ " bNotSolid " $ T.bNotSolid;
}

function bool FindTexture()
{
	Tex = Texture(DynamicLoadObject(TextureOf(), class'Texture', true));
	if (Tex == None)
	{
		Log("DXBACKFACE: no texture " $ TextureOf());
		return false;
	}
	bStockTwoSided = Tex.bTwoSided;
	Log("DXBACKFACE: texture " $ Tex.Name $ " in " $ Tex.Outer.Name $ " in " $ Tex.Outer.Outer.Name $ " (" $ Tex $ "), "
		$ Tex.USize $ "x" $ Tex.VSize $ ", as loaded " $ TextureFlags(Tex));
	return true;
}

// The door of views E and F, its place logged: its +x face at its x
function FindDoor(PlayerPawn P)
{
	local Mover M;

	foreach P.AllActors(class'Mover', M)
	{
		if (string(M.Name) ~= DoorOf())
			Door = M;
	}
	if (Door == None)
		Log("DXBACKFACE: no door " $ DoorOf());
	else
		Log("DXBACKFACE: door " $ Door.Name $ " at " $ Door.Location $ " rotation " $ Door.Rotation $ " MainScale "
			$ Door.MainScale.Scale $ " PostScale " $ Door.PostScale.Scale $ " PrePivot " $ Door.PrePivot);
}

// How far before the surface a view's eye is: the sheet's plane is
// y = -248, its front +y; the door's face x 576, its front +x
function string Before(vector Eye)
{
	if (ViewIndex >= 4)
		return (Eye.X - 576) $ " before the door's face";
	return (Eye.Y + 248) $ " before the sheet's plane";
}

// The NPCs near the walkway and the door out of the world: the room's
// commander stands in view through the opening
function ClearPawns(PlayerPawn P)
{
	local ScriptedPawn SP;

	foreach P.AllActors(class'ScriptedPawn', SP)
	{
		if (VSize(SP.Location - Spots[0]) < 1600 || VSize(SP.Location - Spots[4]) < 1600)
		{
			Log("DXBACKFACE: " $ SP.Name $ " at " $ SP.Location $ " taken out of the world");
			SP.LeaveWorld();
		}
	}
}

// What a trace meets along view A's line: the sheet, being not solid, is
// none of it; the invisible collision slab behind its plane is
function TraceThrough(PlayerPawn P, vector Eye)
{
	local Actor Hit;
	local name TexName, TexGroup;
	local vector HitLoc, HitNorm, End;
	local int TexFlags, Count;
	local string S;

	End = Eye + 600 * vector(Facings[0]);
	foreach P.TraceTexture(class'Actor', Hit, TexName, TexGroup, TexFlags, HitLoc, HitNorm, End, Eye)
	{
		S = S $ " [" $ Hit.Name $ " " $ TexName $ " " $ TexGroup $ " " $ TexFlags $ " at " $ HitLoc $ "]";
		if (++Count >= 8)
			break;
	}
	Log("DXBACKFACE: TraceTexture along view A:" $ S);
}

function BeginStep(PlayerPawn P)
{
	ViewIndex = ViewOf(Step);
	if (TwoSidedAt(Step))
		Tex.bTwoSided = true;
	else
		Tex.bTwoSided = bStockTwoSided;
	if (ViewIndex >= 4)
		Stand = Spots[ViewIndex] - vect(0,0,1) * P.BaseEyeHeight;
	else
		Stand = Ground(P, Spots[ViewIndex]) + vect(0,0,1) * (P.CollisionHeight + 1);
	HoldView(P);
	Log("DXBACKFACE: step " $ Step $ " view " $ Names[ViewIndex] $ " standing at " $ Stand $ " zone " $ P.Region.Zone.Name
		$ ", the texture's " $ TextureFlags(Tex));
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
	local vector Eye;

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
				Log("DXBACKFACE: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Phase = -1;
			}
		}
		else if (MapTime > 6.0 && DXP != None)
		{
			SetupViews();
			if (!FindTexture())
			{
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			Log("DXBACKFACE: the sheet: the level's surface 2267 (node 7877, Brush1280), flags 0x80000A (masked, not solid, high shadow detail),"
				$ " plane y = -248 facing +y, x 2840 to 3112, z 2472 to 2664");
			FindDoor(P);
			DXP.KillShadow();
			P.bDetectable = false;
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			// The original's HUD draws over the grabber's mark.
			if (DeusExRootWindow(DXP.rootWindow) != None)
				DeusExRootWindow(DXP.rootWindow).ShowHud(False);
			if (!KeepPawns)
				ClearPawns(P);
			Step = 0;
			BeginStep(P);
			TraceThrough(P, P.Location + vect(0,0,1) * P.EyeHeight);
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
			Eye = P.Location + vect(0,0,1) * P.EyeHeight;
			Log("DXBACKFACE: shot " $ ShotsTaken $ " view " $ Names[ViewIndex] $ " eye " $ Eye $ " rotation " $ P.ViewRotation
				$ " FOV " $ P.FOVAngle $ ", " $ Before(Eye) $ ", the texture's bTwoSided " $ Tex.bTwoSided);
			P.ConsoleCommand("shot");
			bMarking = false;
			ShotsTaken++;
			Step++;
			StepTime = 0;
			if (Step >= 9)
			{
				Tex.bTwoSided = bStockTwoSided;
				Phase = 2;
			}
			else
				BeginStep(P);
		}
	}
	else if (Phase == 2 && StepTime > 1.0)
	{
		Log("DXBACKFACE: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
