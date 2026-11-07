//=============================================================================
// IniEdgeConsole: a config file's edges (bugs.md PKG.25), IniEdgeProbe's
// DXCapEdge.ini written by hand into the game's System folder before the
// run: from the menu map, what the class was given logged, "DXEDGE: Word
// |<word>| Count <n>", its Ratio times 1e7 and its subclass's Count; then
// its Word in capitals, Count + 1 and a Ratio of 0.1234567 saved
// (StaticSaveConfig), "DXEDGE: saved Count <n>", the three again as the
// defaults hold them after the save, then an exit. The original's
// SetString keeps a value that differs in case alone (GOG DeusEx.exe
// 0x10918030), so a Word of jc stays jc; its reader ends a line at a CR or
// an LF and the text at a NUL, and keeps a key with no name, =x
// (0x10916780). Its SaveConfig then reads the config again into the
// defaults of every class under the topmost config class, each its base's
// section first (Core.dll 0x10151540; PKG.27): Word jc again, the Ratio
// the file's six decimals, 1234570, and IniEdgeProbeSub's Count the one
// saved. Run twice on each file: the second run's lines, and the file
// after it (docs/DEVELOPMENT.md, scripted runs).
//=============================================================================
class IniEdgeConsole extends Console;

var float RunTime;
var bool bDone;

function LogDefaults(string When)
{
	Log("DXEDGE: " $ When $ "Word |" $ class'IniEdgeProbe'.default.Word $ "| Count " $ class'IniEdgeProbe'.default.Count
		$ " Ratio x 1e7 " $ (class'IniEdgeProbe'.default.Ratio * 10000000.0) $ " the subclass's Count " $ class'IniEdgeProbeSub'.default.Count);
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || bDone)
		return;
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	bDone = true;
	LogDefaults("");
	class'IniEdgeProbe'.default.Word = Caps(class'IniEdgeProbe'.default.Word);
	class'IniEdgeProbe'.default.Count = class'IniEdgeProbe'.default.Count + 1;
	class'IniEdgeProbe'.default.Ratio = 0.1234567;
	class'IniEdgeProbe'.static.StaticSaveConfig();
	Log("DXEDGE: saved Count " $ class'IniEdgeProbe'.default.Count);
	LogDefaults("after the save ");
	Viewport.Actor.ConsoleCommand("exit");
}
