//=============================================================================
// ExecLineConsole: an exec file's lines as the original's ExecMacro cuts
// them, by ParseLine into 256 characters (Engine.dll 0x10374700, Core.dll
// 0x10147e60; bugs.md PKG.27): a // outside double quotes drops the rest
// of its line, a | outside them -- in a comment too -- cuts the next
// command, a line past 255 characters runs its rest as the next; and a
// command word as ParseCommand takes it (0x10147a20), exec"x" EXEC. From
// the menu map: "exec PKG27.txt", then exec"PKG27b.txt" with no blank,
// each marked before it runs; the files' lines call this console's Mark,
// which logs "DXEXECL: mark |<its text>|". Then the player's default URL
// options, GetDefaultURL("") and GetDefaultURL("Name"), and the menu map's
// URL, for a run whose User.ini's [DefaultPlayer] has a line =x
// (DXCAP_USERINI): the original's URL takes it as the option =x
// (FURL::LoadURLConfig 0x103f9bc0); then an exit. In the game's System
// folder beforehand: tools/dxcap/execfiles' PKG27.txt and PKG27b.txt.
// Compared: every line (docs/DEVELOPMENT.md, scripted runs).
//=============================================================================
class ExecLineConsole extends Console;

var float RunTime;
var bool bDone;

exec function Mark(string S)
{
	Log("DXEXECL: mark |" $ S $ "|");
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || bDone)
		return;
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	bDone = true;
	P = Viewport.Actor;
	Log("DXEXECL: exec PKG27.txt");
	P.ConsoleCommand("exec PKG27.txt");
	Log("DXEXECL: exec\"PKG27b.txt\"");
	P.ConsoleCommand("exec\"PKG27b.txt\"");
	Log("DXEXECL: GetDefaultURL(\"\") |" $ P.GetDefaultURL("") $ "| Name |" $ P.GetDefaultURL("Name") $ "|");
	Log("DXEXECL: the level's URL " $ P.Level.GetLocalURL());
	Log("DXEXECL: done");
	P.ConsoleCommand("exit");
}
