//=============================================================================
// ForwardConsole: a second launch's command line, handed to the running game
// (dx-reverse-info launch-flow.md, single-instance forwarding). From the
// menu map, each second the URL of the level the player is in, for 60 s;
// then an exit. A second DeusEx.exe started meanwhile with a map (by hand,
// with the run's INI= and USERINI=) should open that map here and exit at
// once. Each line starts "DXFWD:".
//=============================================================================
class ForwardConsole extends Console;

var float RunTime, Logged;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (RunTime - Logged >= 1.0)
	{
		Logged = RunTime;
		Log("DXFWD: at " $ int(RunTime) $ " s in " $ P.Level.GetLocalURL());
	}
	if (RunTime > 60.0)
	{
		Log("DXFWD: exiting");
		P.ConsoleCommand("exit");
		RunTime = -1000;
	}
}
