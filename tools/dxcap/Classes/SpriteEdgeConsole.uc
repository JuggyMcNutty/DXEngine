//=============================================================================
// SpriteEdgeConsole (bugs.md REN.64, a sprite at the frame's right edge):
// whether a sprite whose left edge lies in the frame's last column counts
// as drawn (LastRendered()). Render.dll's FDynamicSprite::Setup (0x10b23910)
// gives a sprite no rectangle unless its left edge's pixel -- the column
// its screen place less half its size rounds up to -- lies before the
// frame's last (0x10b23c91-0x10b23c9a); DXEngine counted one drawn whenever
// a pixel of it showed.
//
// On Liberty Island, 3 s in, the player out of the world at its start,
// level and facing the start's way, the HUD hidden, every pawn gone: five
// sprites (Effects, DT_Sprite, Texture'Engine.DefaultTexture' at DrawScale
// 0.25, unlit), Depth (200) units ahead at the eye's height, each shown
// alone in turn for 0.6 s, its left edge put Edges[i] pixels from the
// frame's right edge (2.5, 1.75, 1.25, 0.75, 0.25: columns X-2, X-1, X-1,
// X and X by its rounding up), by the view's width the canvas gives and
// the player's FovAngle. Each logged "DXSPRITEEDGE: edge e left x of X
// drawn L now T seen S", L the time since it was last drawn (LastRendered()
// is the time since, not a time).
//
// The original: seen at 2.5 alone, the last column's 1.75 and 1.25 and
// the two past it not; DXEngine as it counted any pixel shown: seen at 2.5,
// 1.75 and 1.25. A spot where 2.5 is seen in neither engine is blocked:
// give another Depth. DXCAP_WINDOW=1024x768 for both, a 4:3 view, the same
// in both.
//=============================================================================
class SpriteEdgeConsole extends Console;

var config float Depth;

var float MapTime, StepTime;
var string CurrentMap;
var int Phase;
var bool bOut;
var vector Eye;
var rotator Look;
var Actor Sprite;
var float Edges[5];
var int Step;
var float ViewWidth;
var float LeftX;

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

event PostRender(canvas C)
{
	Super.PostRender(C);
	ViewWidth = C.ClipX;
}

// The sprite with its left edge E pixels short of the frame's right edge
function Place(PlayerPawn P, float E)
{
	local rotator R;
	local vector Ahead, Side;
	local float Half, Proj, X;

	if (Sprite != None)
		Sprite.Destroy();
	Sprite = P.Spawn(class'Effects',,, Eye, Look);
	if (Sprite == None)
	{
		Log("DXSPRITEEDGE: no sprite");
		return;
	}
	Sprite.SetCollision(false, false, false);
	Sprite.bCollideWorld = false;
	Sprite.SetPhysics(PHYS_None);
	Sprite.LifeSpan = 0;
	Sprite.bHidden = false;
	Sprite.DrawType = DT_Sprite;
	Sprite.Style = STY_Normal;
	Sprite.Texture = Texture'Engine.DefaultTexture';
	Sprite.DrawScale = 0.25;
	Sprite.bUnlit = true;

	// Screen x of a point at the depth: the frame's middle plus x over the
	// depth times FX/2 over the half angle's tangent
	Proj = (ViewWidth * 0.5) / Tan(P.FovAngle * 3.14159265 / 360.0);
	LeftX = ViewWidth - E;
	Half = Sprite.Texture.USize * Sprite.DrawScale * 0.5;
	X = (LeftX - ViewWidth * 0.5) / Proj * Depth + Half;
	Ahead = vector(Look);
	R = Look;
	R.Yaw += 16384;
	Side = vector(R);
	Sprite.SetLocation(Eye + Ahead * Depth + Side * X);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local float Now;

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

	if (Phase == 0)
	{
		if (!(CurrentMap ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = -1;
			}
			return;
		}
		if (MapTime < 3.0 || DeusExPlayer(P) == None || ViewWidth <= 0)
			return;
		if (!bOut)
		{
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			P.SetPhysics(PHYS_None);
			bOut = true;
		}
		if (Depth <= 0)
			Depth = 200;
		Edges[0] = 2.5;
		Edges[1] = 1.75;
		Edges[2] = 1.25;
		Edges[3] = 0.75;
		Edges[4] = 0.25;
		DeusExPlayer(P).ShowHud(False);
		Look = P.Rotation;
		Look.Pitch = 0;
		Look.Roll = 0;
		Eye = P.Location + vect(0,0,1) * P.EyeHeight;
		Hold(P);
		Step = 0;
		Place(P, Edges[Step]);
		StepTime = 0;
		Phase = 1;
		return;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
			Phase = 0;
		return;
	}

	Hold(P);
	if (Phase == 1 && StepTime > 0.6)
	{
		Now = P.Level.TimeSeconds;
		if (Sprite != None)
			Log("DXSPRITEEDGE: edge " $ Edges[Step] $ " left " $ LeftX $ " of " $ ViewWidth $ " fov " $ P.FovAngle
			    $ " drawn " $ Sprite.LastRendered() $ " now " $ Now $ " seen " $ (Sprite.LastRendered() < 0.1));
		Step++;
		if (Step >= 5)
		{
			Log("DXSPRITEEDGE: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		Place(P, Edges[Step]);
		StepTime = 0;
	}
}
