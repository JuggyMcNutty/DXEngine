//=============================================================================
// PrefsConsole: the PREFERENCES word's window, the original's Advanced
// Options (bugs.md D.81), opened from the menu map for a script beside the
// run to capture, walk and edit. 2 s in, PREFERENCES by the player's
// ConsoleCommand -- the original's viewport takes the window out of full
// screen and DeusEx.exe's GLaunchExec opens WConfigProperties
// (dx-reverse-info launch-flow.md) -- logged with "DXPREFS:" and what it gave;
// again at Reopen s (0, by default, never). Each second, each of Gets (a GET's
// words with a comma for the space, DXCAP_CONFIG's values having none: a
// class's default, "WinDrv.WindowsClient,ScaleXYZ", or a live object's,
// "ini:Engine.Engine.ViewportManager,ScaleXYZ") logged when its answer
// changes, so an edit made in the window shows where it lands. The run held
// Seconds (60 by default), then an exit.
//=============================================================================
class PrefsConsole extends Console;

var config float Seconds;
var config float Reopen;
var config string Gets[8];
var string Last[8];
var float RunTime, NextGet;
var int Phase;

function LogGets()
{
	local int i, c;
	local string g, v;

	for (i = 0; i < 8; i++)
	{
		g = Gets[i];
		if (g == "")
			continue;
		c = InStr(g, ",");
		if (c >= 0)
			g = Left(g, c) $ " " $ Mid(g, c + 1);
		v = Viewport.Actor.ConsoleCommand("GET " $ g);
		if (v != Last[i] || RunTime < 1.5)
			Log("DXPREFS: GET " $ g $ " = '" $ v $ "' at " $ RunTime);
		Last[i] = v;
	}
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime >= NextGet)
	{
		LogGets();
		NextGet = RunTime + 1.0;
	}
	if (Phase == 0 && RunTime > 2.0)
	{
		Log("DXPREFS: 'PREFERENCES' gave '" $ Viewport.Actor.ConsoleCommand("PREFERENCES") $ "' at " $ RunTime);
		Phase = 1;
	}
	else if (Phase == 1 && Reopen > 2.0 && RunTime > Reopen)
	{
		Log("DXPREFS: 'PREFERENCES' again gave '" $ Viewport.Actor.ConsoleCommand("PREFERENCES") $ "' at " $ RunTime);
		Phase = 2;
	}
	else if (Phase < 3 && RunTime > 2.0 + Seconds)
	{
		Log("DXPREFS: exiting at " $ RunTime);
		Phase = 3;
		Viewport.Actor.ConsoleCommand("exit");
	}
}

defaultproperties
{
	Seconds=60
}
