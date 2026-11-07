//=============================================================================
// HandshakeConsole: the joining side of the handshake tests -- from the menu
// map it opens 127.0.0.1:7790 as Shake, where tools/dxcap/handshakeserver.py
// answers as no stock server does: in its upgrade mode as a server of
// another version (UPGRADE), in its values mode with values past blanks and
// a FAILURE past tabs (bugs.md NET.19). Each pair of progress lines the
// player is shown is logged as it is set, with the seconds it shows for, a
// pair set again too, so a join's Connecting shown anew is seen; 15 s after
// the open the join is cancelled, as F10 cancels it, and 4 s later the run
// exits. Each line starts "DXSHAKE:".
//=============================================================================
class HandshakeConsole extends Console;

var float RunTime, OpenTime, LastTimeOut;
var int Step;

// A time out's seconds, rounded either way from 0: -1 as -1
function int Seconds(float S)
{
	if (S < 0)
		return int(S - 0.5);
	return int(S + 0.5);
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (P.ProgressTimeOut != LastTimeOut)
	{
		LastTimeOut = P.ProgressTimeOut;
		Log("DXSHAKE: t=" $ int(RunTime) $ " progress '" $ P.ProgressMessage[0] $ "' '" $ P.ProgressMessage[1] $ "' for " $ Seconds(P.ProgressTimeOut - P.Level.TimeSeconds) $ " s");
	}

	if (Step == 0 && RunTime > 3.0)
	{
		Log("DXSHAKE: t=" $ int(RunTime) $ " opening 127.0.0.1:7790?Name=Shake");
		P.ConsoleCommand("open 127.0.0.1:7790?Name=Shake");
		OpenTime = RunTime;
		Step = 1;
	}
	else if (Step == 1 && RunTime - OpenTime > 15.0)
	{
		Log("DXSHAKE: t=" $ int(RunTime) $ " cancel");
		P.ConsoleCommand("cancel");
		Step = 2;
	}
	else if (Step == 2 && RunTime - OpenTime > 19.0)
	{
		Log("DXSHAKE: exiting");
		P.ConsoleCommand("exit");
		Step = 3;
	}
}
