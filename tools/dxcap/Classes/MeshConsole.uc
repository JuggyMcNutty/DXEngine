//=============================================================================
// MeshConsole: meshes lit, the same in both engines (docs/DEVELOPMENT.md,
// scripted runs). At Liberty Island's start, level and facing the start's
// way, the HUD hidden: Paul Denton, who stands still, moved 220 units ahead,
// and a large crate, a barrel and a large box placed around him on the pier;
// each one's place and look logged, then two shots 2 s apart once they have
// settled; then exit. LODBias ([DXCapture.MeshConsole], 0 by default: as the
// map has it) is given to all four, a mesh's detail falling with it as with
// distance (dx-reverse-info render-dll.md, mesh detail). With NoDetail the
// detail textures are turned off first, as the Display menu turns them off
// (set ini:Engine.Engine.GameRenderDevice DetailTextures False). With Enviro
// all four are drawn environment-mapped (bMeshEnviroMap), in the map their
// zone or the level gives, or their last slot's texture. With Hold, Paul
// is held at HoldAnim's HoldFrame (his sequence as it is with none named),
// not playing, his blend slots emptied, from the tick before each draw:
// one pose in both engines, whichever idle and head turn his script picks,
// each look logging the sequences and frames. With Extra (a class, as
// DeusEx.Plant1) one more is placed 140 units ahead and 50 to the right,
// before the crate, turned ExtraYaw from the view's way, and logged with
// the rest; with ExtraAhead, that far ahead at the eye's height, still, and
// at ExtraScale (else its own), lit as any actor there. With Plain (a mesh, as DeusExDeco.DXText) one more actor, a bare
// Effects, is placed 120 units ahead at the eye's height, its mesh that one
// alone, no skin, turned PlainYaw from the view's way, drawn in PlainStyle
// (1 normal, 2 masked, 3 translucent, 4 modulated; else normal) at
// PlainScale (else 1), unlit and still, logged with the rest: a plain
// mesh's faces culled or not as the renderer culls them (dx-reverse-info
// render-dll.md, meshes). DXText, a one-sided quad, shows its back at a
// PlainYaw of 16384 and its front at -16384. The pier's
// floor, compared in NATIVES.md (brightness), is two bands of a 1024x768
// shot's planks, either side of Paul's feet: x 180 to 460 and 580 to 750,
// y 640 to 760.
// Each line is logged with "DXMESH:" in front.
//=============================================================================
class MeshConsole extends Console;

var config float LODBias;
var config bool NoDetail;
var config bool Enviro;
var config bool Hold;
var config name HoldAnim;
var config float HoldFrame;
var config string Extra;
var config int ExtraYaw;
var config float ExtraAhead;
var config float ExtraScale;
var config string Plain;
var config int PlainYaw;
var config int PlainStyle;
var config float PlainScale;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var Actor Placed[6];

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

function LogLook(string Label, Actor A)
{
	Log("DXMESH: " $ Label $ " " $ A.Name $ " at " $ A.Location $ " rotation " $ A.Rotation $ " mesh " $ A.Mesh
		$ " style " $ int(A.Style) $ " unlit " $ A.bUnlit $ " glow " $ A.ScaleGlow $ " ambient " $ A.AmbientGlow
		$ " zone " $ A.Region.Zone.Name $ " zone ambient " $ A.Region.Zone.AmbientBrightness $ " LOD bias " $ A.LODBias
		$ " anim " $ A.AnimSequence $ " frame " $ A.AnimFrame
		$ " blends " $ A.BlendAnimSequence[0] $ " " $ A.BlendAnimFrame[0] $ ", " $ A.BlendAnimSequence[1] $ " " $ A.BlendAnimFrame[1]
		$ ", " $ A.BlendAnimSequence[2] $ " " $ A.BlendAnimFrame[2] $ ", " $ A.BlendAnimSequence[3] $ " " $ A.BlendAnimFrame[3]);
}

function Actor Place(PlayerPawn P, class<Actor> C, vector Where)
{
	local Actor A;
	A = P.Spawn(C,,, Where, P.Rotation);
	if (A == None)
		Log("DXMESH: no room for " $ C $ " at " $ Where);
	return A;
}

// Plain's mesh on a bare actor, R the view's way, X ahead
function Actor PlacePlain(PlayerPawn P, rotator R, vector X)
{
	local Actor A;
	local vector Where;
	local int i;

	Where = P.Location + X * 120 + vect(0,0,1) * P.BaseEyeHeight;
	R.Yaw += PlainYaw;
	A = P.Spawn(class'Effects',,, Where, R);
	if (A == None)
	{
		Log("DXMESH: no room for " $ Plain $ " at " $ Where);
		return None;
	}
	A.SetCollision(false, false, false);
	A.bCollideWorld = false;
	A.SetPhysics(PHYS_None);
	A.LifeSpan = 0;
	A.bHidden = false;
	A.DrawType = DT_Mesh;
	A.Mesh = Mesh(DynamicLoadObject(Plain, class'Mesh'));
	A.Skin = None;
	A.Texture = None;
	for (i = 0; i < 8; i++)
		A.MultiSkins[i] = None;
	if (PlainStyle == 2)
		A.Style = STY_Masked;
	else if (PlainStyle == 3)
		A.Style = STY_Translucent;
	else if (PlainStyle == 4)
		A.Style = STY_Modulated;
	else
		A.Style = STY_Normal;
	if (PlainScale > 0)
		A.DrawScale = PlainScale;
	else
		A.DrawScale = 1.0;
	A.bUnlit = true;
	A.SetRotation(R);
	return A;
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

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local ScriptedPawn SP;
	local vector X, Y, Z;
	local rotator R;
	local int i;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") < 0)
		{
			if (MapTime > 3.0)
			{
				Log("DXMESH: opening 01_NYC_UNATCOIsland");
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
			if (NoDetail)
			{
				P.ConsoleCommand("set ini:Engine.Engine.GameRenderDevice DetailTextures False");
				Log("DXMESH: detail textures " $ P.ConsoleCommand("get ini:Engine.Engine.GameRenderDevice DetailTextures"));
			}
			R = P.ViewRotation;
			R.Pitch = 0;
			P.ViewRotation = R;
			P.SetRotation(R);
			GetAxes(R, X, Y, Z);
			foreach P.AllActors(class'ScriptedPawn', SP)
			{
				if (SP.IsA('PaulDenton'))
				{
					if (SP.SetLocation(P.Location + X * 220))
						Placed[0] = SP;
					else
						Log("DXMESH: Paul cannot stand at " $ (P.Location + X * 220));
					break;
				}
			}
			Placed[1] = Place(P, class'CrateUnbreakableLarge', P.Location + X * 200 + Y * 130);
			Placed[2] = Place(P, class'Barrel1', P.Location + X * 170 - Y * 110);
			Placed[3] = Place(P, class'BoxLarge', P.Location + X * 300 - Y * 40);
			if (Extra != "")
			{
				if (ExtraAhead > 0)
				{
					Placed[4] = Place(P, class<Actor>(DynamicLoadObject(Extra, class'Class')), P.Location + X * ExtraAhead + vect(0,0,1) * P.BaseEyeHeight);
					if (Placed[4] != None)
					{
						Placed[4].SetPhysics(PHYS_None);
						Placed[4].SetLocation(P.Location + X * ExtraAhead + vect(0,0,1) * P.BaseEyeHeight);
					}
				}
				else
					Placed[4] = Place(P, class<Actor>(DynamicLoadObject(Extra, class'Class')), P.Location + X * 140 + Y * 50);
				if (Placed[4] != None && ExtraScale > 0)
					Placed[4].DrawScale = ExtraScale;
				if (Placed[4] != None && ExtraYaw != 0)
				{
					R.Yaw += ExtraYaw;
					Placed[4].SetRotation(R);
					R.Yaw -= ExtraYaw;
				}
			}
			if (Plain != "")
				Placed[5] = PlacePlain(P, R, X);
			if (LODBias > 0)
				for (i = 0; i < 4; i++)
					if (Placed[i] != None)
						Placed[i].LODBias = LODBias;
			if (Enviro)
				for (i = 0; i < 4; i++)
					if (Placed[i] != None)
						Placed[i].bMeshEnviroMap = True;
			// The original's HUD draws over the grabber's mark.
			if (DeusExRootWindow(DXP.rootWindow) != None)
				DeusExRootWindow(DXP.rootWindow).ShowHud(False);
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		if (Hold && Placed[0] != None)
		{
			if (HoldAnim != '')
				Placed[0].AnimSequence = HoldAnim;
			Placed[0].AnimFrame = HoldFrame;
			Placed[0].AnimRate = 0;
			for (i = 0; i < 4; i++)
			{
				Placed[0].BlendAnimSequence[i] = '';
				Placed[0].BlendAnimFrame[i] = 0;
				Placed[0].BlendAnimRate[i] = 0;
			}
		}
		// Settled after 3 s; a shot every 2 s from then, the mark shown for
		// the 0.6 s before each.
		if (StepTime > 3.0 && ShotsTaken < 2)
		{
			if (!bMarking)
			{
				MarkShot = ShotsTaken;
				bMarking = true;
				StepTime = 2.4;
			}
			else
			{
				for (i = 0; i < 6; i++)
					if (Placed[i] != None)
						LogLook("shot " $ ShotsTaken, Placed[i]);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 1.0;
			}
		}
		else if (ShotsTaken >= 2 && StepTime > 2.0)
		{
			Log("DXMESH: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
