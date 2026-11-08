//=============================================================================
// WindowWordsConsole: what the local player's ConsoleCommand gives for the
// window's words that WordOrderConsole left to exec functions, a missing
// exec file and the viewport's net words, with no exec function of their
// names (bugs.md WIN.23). From the menu map, 2 s in, each word
// of Words through Viewport.Actor.ConsoleCommand -- the original's viewport
// (APlayerPawn::execConsoleCommand, Engine.dll 0x103ba200; UViewport::Exec
// 0x10373a60; UWindowsViewport::Exec, WinDrv.dll 0x11106d90) -- logged with
// "DXWINWORDS:" and what it gave, then an exit. Run the original with
// DXCAP_RENDERER=D3D, in a window.
// The original's: GETCOLORDEPTHS "16 32"; GETCURRENTCOLORDEPTH 8
// times the viewport's bytes a pixel, "32" in a window on a 32-bit desktop;
// GETCURRENTRENDERDEVICE the device class's path name,
// "D3DDrv.D3DRenderDevice"; EXEC of a file not there Core's FileNotFound on
// the command's output (UViewport::ExecMacro 0x10374700), "Can't find file
// 'DxNoSuchFile.txt'"; GETPING "0" (0x10373aee); GETLOSS "" (no taker).
// DXEngine 6d100090: "32 16", "32", "" (Unknown command), "",
// "0", "0".
//=============================================================================
class WindowWordsConsole extends Console;

var float RunTime;
var bool bDone;
var string Words[8];

event Tick(float Delta)
{
	local int i;

	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	bDone = true;
	for (i = 0; i < ArrayCount(Words); i++)
	{
		if (Words[i] != "")
			Log("DXWINWORDS: '" $ Words[i] $ "' gave '" $ Viewport.Actor.ConsoleCommand(Words[i]) $ "'");
	}
	Log("DXWINWORDS: exiting");
	Viewport.Actor.ConsoleCommand("exit");
}

defaultproperties
{
	Words(0)="GETCOLORDEPTHS"
	Words(1)="GETCURRENTCOLORDEPTH"
	Words(2)="GETCURRENTRENDERDEVICE"
	Words(3)="EXEC DxNoSuchFile.txt"
	Words(4)="GETPING"
	Words(5)="GETLOSS"
}
