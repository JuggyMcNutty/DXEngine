//=============================================================================
// CanvasZConsole (bugs.md REN.72): the canvas's Z for its tiles and its
// text, shot in both engines. TargetMap ([DXCapture.CanvasZConsole], Liberty
// Island by default) opened from wherever the run starts; 3 s into the
// level's time the HUD hidden (ShowHud(False)); from then the console's
// PostRender draws, after its own, each frame, in the normal style:
//
//   A, Texture'Solid' red at Z 1, at 100, 100, 200 by 200 (DrawRect, so
//      DrawTile);
//   B, green at Z 2, at 200, 200, 200 by 200, over A's lower right quarter;
//   C, "WWWWWW" in white in the MedFont at Z 2, at 110, 110, over A;
//   D, blue at Z 2 by DrawTileClipped, at 110, 230, 60 by 60, over A;
//   E, yellow at Z 1, at 230, 110, 50 by 50, over A, the control;
//
// then Z 1 again. 1 s later a marked shot; 1 s on an exit. Logged with
// "DXCANVASZ:" in front.
//
// The original's DrawTile and DrawTileClipped hand the tile the canvas's Z
// (Engine.dll execDrawTile 0x10377ba0, execDrawTileClipped 0x10378450, into
// UCanvas::DrawTile 0x10376160), as its line drawer's glyphs do (0x10377170);
// D3DDrv puts a tile at the depth of a point Z units before the eye, 1/Z
// its w (DrawTile 0x10005ba0), tested less than or equal against the depth
// buffer and an opaque one writing it (SetBlending 0x10008270): A at Z 1,
// depth 0, hides what is drawn at Z 2 where it lies. Read with
// DXCAP_RENDERER=D3D at DXCAP_WINDOW=1280x720: the pixels at 250, 250 (A
// under B), 140, 260 (A under D), 255, 135 (E) and 350, 350 (B alone), and
// the region 110, 110, 110 by 20 (C's glyphs over A). The original: red,
// red, yellow, green, and red alone in the region. DXEngine before a fix
// drew every tile at Z 1: green, blue, yellow, green, and white strokes in
// the region.
//=============================================================================
class CanvasZConsole extends Console;

var config string TargetMap;

var float MapTime;
var float StepTime;
var int Step;
var string CurrentMap;
var bool bDrawing;
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
	i = InStr(U, "\\");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "\\");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;

	Super.Tick(Delta);
	if (Step == 9 || Viewport == None || Viewport.Actor == None)
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

	if (Step == 0 && !(CurrentMap ~= Target()))
	{
		if (MapTime > 3.0)
		{
			Log("DXCANVASZ: opening " $ Target());
			P.ConsoleCommand("open " $ Target());
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= Target()))
		Step = 0;

	if (Step == 0 && P.Level.TimeSeconds > 3.0)
	{
		Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
		if (Root != None)
			Root.ShowHud(False);
		Log("DXCANVASZ: the HUD hidden, the tiles drawn from now, screen " $ P.ConsoleCommand("getcurrentres"));
		bDrawing = true;
		bMarking = true;
		StepTime = 0;
		Step = 1;
	}
	else if (Step == 1 && StepTime > 1.0)
	{
		Log("DXCAP: shot 0");
		P.ConsoleCommand("shot");
		StepTime = 0;
		Step = 2;
	}
	else if (Step == 2 && StepTime > 1.0)
	{
		bDrawing = false;
		bMarking = false;
		Log("DXCANVASZ: done, exiting");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}

function DrawBlock(canvas C, float X, float Y, float W, float H, byte R, byte G, byte B, float Z)
{
	C.Z = Z;
	C.SetPos(X, Y);
	C.DrawColor.R = R;
	C.DrawColor.G = G;
	C.DrawColor.B = B;
	C.DrawRect(Texture'Solid', W, H);
}

event PostRender(canvas C)
{
	local int i;

	Super.PostRender(C);
	if (bDrawing)
	{
		C.Style = 1;
		DrawBlock(C, 100, 100, 200, 200, 255, 0, 0, 1.0);
		DrawBlock(C, 200, 200, 200, 200, 0, 255, 0, 2.0);
		C.Z = 2.0;
		C.Font = C.MedFont;
		C.DrawColor.R = 255;
		C.DrawColor.G = 255;
		C.DrawColor.B = 255;
		C.SetPos(110, 110);
		C.DrawText("WWWWWW");
		C.SetPos(110, 230);
		C.DrawColor.R = 0;
		C.DrawColor.G = 0;
		C.DrawColor.B = 255;
		C.DrawTileClipped(Texture'Solid', 60, 60, 0, 0, Texture'Solid'.USize, Texture'Solid'.VSize);
		DrawBlock(C, 230, 110, 50, 50, 255, 255, 0, 1.0);
		C.Z = 1.0;
	}
	if (!bMarking)
		return;
	// The mark for the original's screen grabber, as CaptureConsole's: a
	// magenta block, then the shot's number in eight blocks, black for a 0
	// bit and white for a 1, the low bit first
	C.Style = 1;
	C.Z = 1.0;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		C.DrawColor.R = 0;
		C.DrawColor.G = 0;
		C.DrawColor.B = 0;
		C.SetPos(12 + 12 * i, 0);
		C.DrawRect(Texture'Solid', 12, 12);
	}
}
