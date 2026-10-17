//=============================================================================
// ConfigConsole: a class's own config file, ConfigProbe's DXCapConfig.ini,
// read and written back: logs "DXCONFIG: Runs <n>" as the class was given
// it, saves n + 1 (StaticSaveConfig), then an exit, from the menu map. The
// engine writes the file as it exits: the game's System folder, or with the
// fork's --userdir (DXCAP_USERDIR) that folder, which a read takes first.
//=============================================================================
class ConfigConsole extends Console;

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
	Viewport.Actor.ConsoleCommand("exit");
}
