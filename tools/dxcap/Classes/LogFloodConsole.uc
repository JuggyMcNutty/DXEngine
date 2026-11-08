//=============================================================================
// LogFloodConsole: the engine's log past what it keeps, in DXEngine (bugs.md
// ENG.33). On the map the run starts on, 3 s in, Lines lines (120,000 by
// default) logged in one tick, each "DXFLOOD: <n>", then an exit. The
// engine's last-run log (DXE-Log-LastRun.txt in its folder) keeps the
// session's first 1,000 lines and its last 100,000, a line between them
// saying how many it did not keep; stderr, the run's engine.log, has every
// line.
//=============================================================================
class LogFloodConsole extends Console;

var config int Lines;
var float RunTime;
var bool bDone;

event Tick(float Delta)
{
	local int i, n;

	Super.Tick(Delta);
	RunTime += Delta;
	if (bDone || RunTime < 3.0 || Viewport == None || Viewport.Actor == None)
		return;
	bDone = true;
	n = Lines;
	if (n <= 0)
		n = 120000;
	for (i = 0; i < n; i++)
		Log("DXFLOOD: " $ i);
	Log("DXFLOOD: done, exiting");
	Viewport.Actor.ConsoleCommand("exit");
}
