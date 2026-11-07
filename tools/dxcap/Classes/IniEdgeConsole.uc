//=============================================================================
// IniEdgeConsole: a config file's edges (bugs.md PKG.25), IniEdgeProbe's
// DXCapEdge.ini written by hand into the game's System folder before the
// run: from the menu map, what the class was given logged, "DXEDGE: Word
// |<word>| Count <n>", then its Word in capitals and Count + 1 saved
// (StaticSaveConfig), "DXEDGE: saved Count <n>", then an exit. The
// original's SetString keeps a value that differs in case alone (GOG
// DeusEx.exe 0x10918030), so a Word of jc stays jc; its reader ends a line
// at a CR or an LF and the text at a NUL, and keeps a key with no name, =x
// (0x10916780). Run twice on each file: the second run's lines, and the
// file after it (docs/DEVELOPMENT.md, scripted runs).
//=============================================================================
class IniEdgeConsole extends Console;

var float RunTime;
var bool bDone;

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || bDone)
		return;
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	bDone = true;
	Log("DXEDGE: Word |" $ class'IniEdgeProbe'.default.Word $ "| Count " $ class'IniEdgeProbe'.default.Count);
	class'IniEdgeProbe'.default.Word = Caps(class'IniEdgeProbe'.default.Word);
	class'IniEdgeProbe'.default.Count = class'IniEdgeProbe'.default.Count + 1;
	class'IniEdgeProbe'.static.StaticSaveConfig();
	Log("DXEDGE: saved Count " $ class'IniEdgeProbe'.default.Count);
	Viewport.Actor.ConsoleCommand("exit");
}
