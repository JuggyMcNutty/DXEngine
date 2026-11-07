//=============================================================================
// LingerConsole: how a TcpLink's connection ends (bugs.md NET.23), logged
// with "DXLINGER:" -- the original's BindPort makes its socket's close
// abortive (SO_LINGER on, 0 s; IpDrv.dll 0x10706a22), and its scripted close
// shuts the connection both ways (shutdown's SD_BOTH, 0x1070808e), so the
// tick's check finds it ended and closes it at once. 2 s into whatever map
// the run starts on, two CapLingerLinks opened to tools/dxcap/lingerpeer.py
// on 127.0.0.1:7879 (started beside the run), each sending a line once
// opened: A "abort", destroyed a second later, its socket closed with the
// actor; B "close", which Closes a second later. The peer prints what each
// connection brought and how it ended, and after an end of file what came of
// a "bye" sent back a second later. The original's: A's peer reads "abort"
// then a reset; B closed a tick after its Close, its peer reading "close"
// then the end, its "bye" refused. The run exits at 8 s.
//=============================================================================
class LingerConsole extends Console;

var float RunTime;
var int Step;
var CapLingerLink A, B;
var bool bAGone, bBClosed;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Step == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (Step == 0 && RunTime > 2.0)
	{
		A = P.Spawn(class'CapLingerLink');
		A.Start("A", "abort");
		B = P.Spawn(class'CapLingerLink');
		B.Start("B", "close");
		Step = 1;
	}
	else if (Step == 1)
	{
		if (!bAGone && A.bOpened && P.Level.TimeSeconds - A.OpenedAt > 1.0)
		{
			Log("DXLINGER: A destroyed while connected");
			A.Destroy();
			bAGone = true;
		}
		if (!bBClosed && B.bOpened && P.Level.TimeSeconds - B.OpenedAt > 1.0)
		{
			Log("DXLINGER: B Close " $ B.Close() $ ", state " $ B.LinkState $ "," $ B.Since());
			bBClosed = true;
		}
		if (RunTime > 8.0)
		{
			Log("DXLINGER: exiting, B's state " $ B.LinkState);
			P.ConsoleCommand("exit");
			Step = 9;
		}
	}
}
