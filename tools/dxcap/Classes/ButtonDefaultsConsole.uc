//=============================================================================
// ButtonDefaultsConsole: a button's colours and textures when a call to
// SetTextColors, SetButtonColors or SetButtonTextures leaves slots out,
// logged with "DXBUTTON:".
//
// The original's execs (Extension.dll 0x10008940, 0x100087b0, 0x10008690)
// give every slot left out its default -- a text colour green, a tile colour
// white, a texture None -- and set all six.
//
// From the menu map, 2 s in: a button made on the root window, its six text
// colours, tile colours and textures each set apart, then each call made
// again with its first slot alone; the button made insensitive, so it draws
// its insensitive slots, and a frame later its curTextColor, curTileColor and
// curTexture logged; then sensitive again, its normal slots, logged a frame
// later. Then an exit.
//=============================================================================
class ButtonDefaultsConsole extends Console;

var float RunTime;
var int Phase;
var ButtonWindow Button;

function string ColorText(color C)
{
	return C.R $ "," $ C.G $ "," $ C.B;
}

function color MakeColor(byte R, byte G, byte B)
{
	local color C;
	C.R = R;
	C.G = G;
	C.B = B;
	return C;
}

function Report(string Where)
{
	Log("DXBUTTON: " $ Where $ ": text " $ ColorText(Button.curTextColor) $ ", tile " $ ColorText(Button.curTileColor) $ ", texture " $ Button.curTexture);
}

event Tick(float Delta)
{
	local DeusExPlayer DXP;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	DXP = DeusExPlayer(Viewport.Actor);
	if (DXP == None || DXP.rootWindow == None)
		return;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		Button = ButtonWindow(DXP.rootWindow.NewChild(class'ButtonWindow'));
		Button.SetPos(0, 0);
		Button.SetSize(32, 16);
		Button.SetTextColors(MakeColor(10, 0, 0), MakeColor(20, 0, 0), MakeColor(30, 0, 0), MakeColor(40, 0, 0), MakeColor(50, 0, 0), MakeColor(60, 0, 0));
		Button.SetButtonColors(MakeColor(0, 10, 0), MakeColor(0, 20, 0), MakeColor(0, 30, 0), MakeColor(0, 40, 0), MakeColor(0, 50, 0), MakeColor(0, 60, 0));
		Button.SetButtonTextures(Texture'Engine.S_Actor', Texture'Engine.S_Camera', Texture'Engine.S_Flag', Texture'Engine.S_Inventory', Texture'Engine.S_Keypoint', Texture'Engine.S_Light');
		Button.SetTextColors(MakeColor(255, 0, 0));
		Button.SetButtonColors(MakeColor(0, 0, 255));
		Button.SetButtonTextures(Texture'Engine.S_Pawn');
		Button.SetSensitivity(False);
		Log("DXBUTTON: made, each call again with its first slot alone, made insensitive");
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 2.5)
	{
		Report("insensitive");
		Button.SetSensitivity(True);
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 3.0)
	{
		Report("sensitive");
		Button.Destroy();
		Log("DXBUTTON: done");
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 3;
	}
}
