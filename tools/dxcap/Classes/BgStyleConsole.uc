//=============================================================================
// BgStyleConsole: the style a window's own drawing takes from its
// background style, shot in both engines (docs/DEVELOPMENT.md, scripted
// runs; bugs.md EXT.28). The original's window draws through a GC of its own
// that SetBackgroundStyle and SetBackgroundSmoothing set (Extension.dll
// XWindow::SetBackgroundStyle 0x1004d670, SetBackgroundSmoothing
// 0x1004d700), so a button's texture (XButtonWindow::Draw 0x100082c0, a
// DrawPattern in the GC as it is) draws in the button's background style,
// as the stock MenuUIScrollAreaWindow's arrows (DSTY_Masked) and
// PersonaScrollAreaWindow's (DSTY_Translucent) and MenuUITabButtonWindow
// (DSTY_Masked) have it, none of them given a background texture.
//
// On the menu map, 3 s in: on the root, a grey box (Texture'Solid', 128,
// 128, 128) at 100, 100, 400 by 100; in it four buttons 64 by 64 at 16, 18,
// 112, 18, 208, 18 and 304, 18, every state's texture
// PersonaScrollUpButton_Normal (16 by 16, a grey frame and arrow, colour 1,
// on colour 0, black), white: B0 DSTY_Normal (the control), B1 DSTY_Masked,
// B2 DSTY_Translucent, B3 DSTY_Masked with SetBackground(None) as well
// (DXEngine's background path, the second control). 1 s later a shot,
// marked for the original's grabber, and an exit. Each button's place is
// logged with "DXBGSTYLE:" in front.
//
// Read, at DXCAP_WINDOW=1024x768 (a scale of 1, the root at 0, 0): in each
// button the pixel 13, 13 in (texel 13, 13: colour 0) and 0, 0 in (texel
// 0, 0: colour 1). The original: B0 black and 100; B1 grey (128, the box)
// and 100; B2 grey (128) and 228; B3 grey and 100. DXEngine before a fix:
// B1 and B2 black and 100, as B0.
//=============================================================================
class BgStyleConsole extends Console;

var float RunTime;
var int Phase;
var bool bMarking;
var Window Box;
var ButtonWindow Buttons[4];

// Style: 0 DSTY_Normal, 1 DSTY_Masked, 2 DSTY_Translucent (EDrawStyle is
// Extension's, not a Console's to name).
function ButtonWindow MakeButton(int i, int Style, bool bWithBackground)
{
	local ButtonWindow B;
	local Texture T;

	T = Texture'DeusExUI.PersonaScrollUpButton_Normal';
	B = ButtonWindow(Box.NewChild(class'ButtonWindow'));
	B.SetPos(16 + 96 * i, 18);
	B.SetSize(64, 64);
	B.SetButtonTextures(T, T, T, T, T, T);
	if (Style == 1)
		B.SetBackgroundStyle(DSTY_Masked);
	else if (Style == 2)
		B.SetBackgroundStyle(DSTY_Translucent);
	else
		B.SetBackgroundStyle(DSTY_Normal);
	if (bWithBackground)
		B.SetBackground(None);
	B.SetSelectability(False);
	B.Show();
	return B;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;
	local int i;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 3.0)
	{
		Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
		if (Root == None)
			return;
		Box = Root.NewChild(class'Window');
		Box.SetPos(100, 100);
		Box.SetSize(400, 100);
		Box.SetBackground(Texture'Solid');
		Box.SetBackgroundStyle(DSTY_Normal);
		Box.SetTileColorRGB(128, 128, 128);
		Box.Show();
		Buttons[0] = MakeButton(0, 0, false);
		Buttons[1] = MakeButton(1, 1, false);
		Buttons[2] = MakeButton(2, 2, false);
		Buttons[3] = MakeButton(3, 1, true);
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 4.0)
	{
		Log("DXBGSTYLE: root " $ Box.GetParent().width $ "x" $ Box.GetParent().height $ ", box at " $ Box.x $ "," $ Box.y $ " " $ Box.width $ "x" $ Box.height);
		for (i = 0; i < 4; i++)
			Log("DXBGSTYLE: button " $ i $ " at " $ Buttons[i].x $ "," $ Buttons[i].y $ " " $ Buttons[i].width $ "x" $ Buttons[i].height $ ", style " $ Buttons[i].backgroundStyle);
		bMarking = true;
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 5.0)
	{
		Log("DXCAP: shot 0");
		P.ConsoleCommand("shot");
		bMarking = false;
		Phase = 3;
	}
	else if (Phase == 3 && RunTime > 6.0)
	{
		Box.Destroy();
		Log("DXBGSTYLE: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}

// The mark for the original's screen grabber, as TextBlockConsole's: a
// magenta block, then the shot's number (0) in eight black blocks
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
	C.DrawColor.R = 0;
	C.DrawColor.G = 0;
	C.DrawColor.B = 0;
	for (i = 0; i < 8; i++)
	{
		C.SetPos(12 + 12 * i, 0);
		C.DrawRect(Texture'Solid', 12, 12);
	}
}
