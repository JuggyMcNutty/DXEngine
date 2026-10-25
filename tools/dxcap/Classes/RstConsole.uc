//=============================================================================
// RstConsole: a TCP link whose peer resets the connection -- 2 s into
// whatever map the run starts on, a CapRstLink spawned and opened to
// 127.0.0.1:7877 (the harness's rstserver.py, which resets the connection a
// second after it opens); the link sends a line as it opens and two more, a
// second apart, after the reset; an exit at 8 s. Without MSG_NOSIGNAL a send
// on the reset connection raised SIGPIPE and ended the engine where nothing
// masks the signal (the X11 and Wayland windows, a dedicated server), so the
// run ends with no "DXRST: exiting" at 59dd1e4 (bugs.md E8.15); the
// original's Windows sockets return an error. Each line starts "DXRST:".
//=============================================================================
class RstConsole extends Console;

var float RunTime;
var int Step;

event Tick(float Delta)
{
	local PlayerPawn P;
	local CapRstLink L;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (Step == 0 && RunTime > 2.0)
	{
		L = P.Spawn(class'CapRstLink');
		L.Start();
		Step = 1;
	}
	else if (Step == 1 && RunTime > 8.0)
	{
		Log("DXRST: exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
}
