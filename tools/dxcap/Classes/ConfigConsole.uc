//=============================================================================
// ConfigConsole: a class's own config file, ConfigProbe's DXCapConfig.ini,
// read and written back: logs "DXCONFIG: Runs <n>" as the class was given
// it, saves n + 1 (StaticSaveConfig), then an exit, from the menu map. The
// engine writes the file as it exits: the game's System folder, or with
// DXEngine's --userdir (DXCAP_USERDIR) that folder, which a read takes first.
// Then the console itself, an object of a config class, sets its own
// ObjectRuns to 1000 and calls StaticSaveConfig: the original writes the
// class's defaults, ObjectRuns 0, never the object's own value (bugs.md
// E4a.9), so the game ini's [DXCapture.ConfigConsole] holds ObjectRuns=0
// (an object of a config class made by new hangs the original at exit: the
// console serves instead).
//=============================================================================
class ConfigConsole extends Console;

var config int ObjectRuns;
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
	Log("DXCONFIG: Runs " $ class'ConfigProbe'.default.Runs);
	class'ConfigProbe'.default.Runs = class'ConfigProbe'.default.Runs + 1;
	class'ConfigProbe'.static.StaticSaveConfig();
	Log("DXCONFIG: saved Runs " $ class'ConfigProbe'.default.Runs);
	ObjectRuns = 1000;
	StaticSaveConfig();
	Log("DXCONFIG: the console's own StaticSaveConfig with ObjectRuns 1000; the ini keeps " $ default.ObjectRuns);
	Viewport.Actor.ConsoleCommand("exit");
}
