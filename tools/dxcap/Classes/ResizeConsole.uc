//=============================================================================
// ResizeConsole: a border window resized and moved by its border
// (docs/DEVELOPMENT.md, scripted runs; docs/NATIVES.md, the UI). From the
// menu map, 2 s in: the cheats enabled and the EditFlags cheat screen
// opened, a modal tool window, and in it a border window of the tool
// window's nine textures, 200 by 150 at 20, 60, with resizing on
// (EnableResizing), which no script of the game turns on. Every half second
// till 20 s its place and size are logged, in the tool window and in the
// root, with "DXRESIZE:" in front; then an exit. A timeline drags it
// (tools/dxcap/timelines/resize-drags.txt): the bottom right corner with the
// left button, then the window by its inside with the right one.
//=============================================================================
class ResizeConsole extends Console;

var float RunTime, LogTime;
var int Phase;
var BorderWindow Border;
var Window Tool;

function Report(string What)
{
	local float RootX, RootY;

	Border.ConvertCoordinates(Border, 0, 0, Border.GetRootWindow(), RootX, RootY);
	Log("DXRESIZE: " $ What $ " at " $ Border.X $ "," $ Border.Y $ " size " $ Border.Width $ "," $ Border.Height
		$ " root " $ RootX $ "," $ RootY);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		P.ConsoleCommand("set DeusEx.JCDentonMale bCheatsEnabled True");
		P.ConsoleCommand("set DeusEx.JCDentonFemale bCheatsEnabled True");
		P.ConsoleCommand("editflags");
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
		if (Root != None)
			Tool = Root.GetTopWindow();
		if (Tool == None)
		{
			Log("DXRESIZE: no tool window, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		Border = BorderWindow(Tool.NewChild(Class'BorderWindow'));
		Border.SetBorders(Texture'ToolWindowBorder_TL', Texture'ToolWindowBorder_TR',
			Texture'ToolWindowBorder_BL', Texture'ToolWindowBorder_BR',
			Texture'ToolWindowBorder_L', Texture'ToolWindowBorder_R',
			Texture'ToolWindowBorder_T', Texture'ToolWindowBorder_B');
		Border.SetBorderStyle(DSTY_Normal);
		Border.SetSize(200, 150);
		Border.SetPos(20, 60);
		Border.EnableResizing(True);
		Border.Show();
		Log("DXRESIZE: made in " $ Tool.Class);
		LogTime = RunTime;
		Phase = 2;
	}
	else if (Phase == 2 && RunTime - LogTime >= 0.5)
	{
		LogTime = RunTime;
		Report(string(int(RunTime * 10) / 10.0));
		if (RunTime > 20.0)
		{
			Log("DXRESIZE: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
