//=============================================================================
// WordOrderConsole: exec functions of the console's own named as the
// window's words, each asked through ConsoleCommand (bugs.md WIN.19),
// logged with "DXWORDS:". On the map the run starts on, 3 s in: the root's
// size, then each of TOGGLEFULLSCREEN, ENDFULLSCREEN, GETCURRENTRES,
// GETCURRENTCOLORDEPTH, GETCOLORDEPTHS, GETCURRENTRENDERDEVICE, SETRES
// 800x600 and GETRES asked, what it gives logged, each exec function
// logging its call; 3 s later the root's size again, then an exit. The
// original's viewport takes the input's words, the render device's
// (D3DDrv's GETRES, D3DDrv.dll 0x100061b0), the viewport's own, then the
// exec functions, the console's first, then the engine's words
// (UViewport::Exec, Engine.dll 0x10373a60); its window's own words only
// after all of those (UWindowsViewport::Exec, WinDrv.dll 0x11106d90). So
// every exec function here is called but GetRes, whose word D3DDrv's list
// answers, and no window word is taken: no full screen, no new size, the
// root's size the same after. The original with DXCAP_RENDERER=D3D
// (OpenGLDrv's own words are not read). Compared: the lines
// (docs/DEVELOPMENT.md, scripted runs).
//=============================================================================
class WordOrderConsole extends Console;

var float RunTime;
var int Phase;

exec function ToggleFullscreen()
{
	Log("DXWORDS: exec function ToggleFullscreen");
}

exec function EndFullscreen()
{
	Log("DXWORDS: exec function EndFullscreen");
}

exec function GetCurrentRes()
{
	Log("DXWORDS: exec function GetCurrentRes");
}

exec function GetCurrentColorDepth()
{
	Log("DXWORDS: exec function GetCurrentColorDepth");
}

exec function GetColorDepths()
{
	Log("DXWORDS: exec function GetColorDepths");
}

exec function GetCurrentRenderDevice()
{
	Log("DXWORDS: exec function GetCurrentRenderDevice");
}

exec function SetRes(string S)
{
	Log("DXWORDS: exec function SetRes " $ S);
}

exec function GetRes()
{
	Log("DXWORDS: exec function GetRes");
}

function RootSize(PlayerPawn P, string When)
{
	local DeusExRootWindow Root;

	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
	if (Root != None)
		Log("DXWORDS: the root " $ When $ int(Root.width) $ " x " $ int(Root.height));
}

function Ask(PlayerPawn P, string Command)
{
	Log("DXWORDS: " $ Command $ " gives |" $ P.ConsoleCommand(Command) $ "|");
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (Phase == 0 && RunTime > 3.0)
	{
		RootSize(P, "");
		Ask(P, "togglefullscreen");
		Ask(P, "endfullscreen");
		Ask(P, "getcurrentres");
		Ask(P, "getcurrentcolordepth");
		Ask(P, "getcolordepths");
		Ask(P, "getcurrentrenderdevice");
		Ask(P, "setres 800x600");
		Ask(P, "getres");
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 6.0)
	{
		RootSize(P, "after ");
		P.ConsoleCommand("exit");
		Phase = 2;
	}
}
