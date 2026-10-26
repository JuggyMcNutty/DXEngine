//=============================================================================
// ConfigConsole: a class's own config file, ConfigProbe's DXCapConfig.ini,
// read and written back: logs "DXCONFIG: Runs <n>" as the class was given
// it, saves n + 1 (StaticSaveConfig), then an exit, from the menu map. The
// engine writes the file as it exits: the game's System folder, or with
// DXEngine's --userdir (DXCAP_USERDIR) that folder, which a read takes first.
// Then an object of the class, its Runs set to 1000, calls StaticSaveConfig
// itself: the original writes the class's defaults again, n + 1, never the
// object's own value (bugs.md E4a.9), so the file keeps n + 1.
//=============================================================================
class ConfigConsole extends Console;

var float RunTime;
var bool bDone;

event Tick(float Delta)
{
	local ConfigProbe O;

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
	O = new class'ConfigProbe';
	O.Runs = 1000;
	O.StaticSaveConfig();
	Log("DXCONFIG: an object's StaticSaveConfig with Runs 1000; the file keeps " $ class'ConfigProbe'.default.Runs);
	Viewport.Actor.ConsoleCommand("exit");
}
