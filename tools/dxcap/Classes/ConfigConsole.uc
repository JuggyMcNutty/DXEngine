//=============================================================================
// ConfigConsole: a class's own config file, ConfigProbe's DXCapConfig.ini,
// read and written back: logs "DXCONFIG: Runs <n>" as the class was given
// it (with its Spot, Kind and Words), saves n + 1 with a vector, a class and
// two strings (StaticSaveConfig: the file as the original writes those
// types, bugs.md E5a.18), and Wide, U+03A9, U+2122 and e acute (the file
// written UTF-16), its codes as read logged, then an exit, from the menu map. The
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

// Each character's code, the log being 8-bit
function string Codes(string S)
{
	local string R;
	local int i;

	for (i = 0; i < Len(S); i++)
		R = R $ " " $ Asc(Mid(S, i, 1));
	return R;
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
	Log("DXCONFIG: Runs " $ class'ConfigProbe'.default.Runs $ ", Spot " $ class'ConfigProbe'.default.Spot $ ", Kind " $ class'ConfigProbe'.default.Kind $ ", Words " $ class'ConfigProbe'.default.Words[0] $ "|" $ class'ConfigProbe'.default.Words[1]);
	class'ConfigProbe'.default.Runs = class'ConfigProbe'.default.Runs + 1;
	class'ConfigProbe'.default.Spot = vect(1, 2.5, -3);
	class'ConfigProbe'.default.Kind = class'Light';
	class'ConfigProbe'.default.Words[0] = "two words";
	class'ConfigProbe'.default.Words[1] = "";
	Log("DXCONFIG: Wide codes" $ Codes(class'ConfigProbe'.default.Wide));
	class'ConfigProbe'.default.Wide = Chr(937) $ Chr(8482) $ Chr(233);
	class'ConfigProbe'.static.StaticSaveConfig();
	Log("DXCONFIG: saved Runs " $ class'ConfigProbe'.default.Runs);
	ObjectRuns = 1000;
	StaticSaveConfig();
	Log("DXCONFIG: the console's own StaticSaveConfig with ObjectRuns 1000; the ini keeps " $ default.ObjectRuns);
	Viewport.Actor.ConsoleCommand("exit");
}
