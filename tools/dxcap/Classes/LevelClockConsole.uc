//=============================================================================
// LevelClockConsole: the level's clock and a write to its float, logged
// with "DXCLOCK:".
//
// On whatever map the run starts on, 3 s in, Level.TimeSeconds set to
// 1,048,576, where the float's step is 0.125 s and a frame shorter than
// 0.0625 s is rounded away by a float sum; then for 2 s of the console's
// own ticks, every quarter second, the real time since and the level's time
// since. Then an exit.
// Both engines alike: the float is stored each tick from a clock of the
// level's own, so a tick after the write it is the level's few seconds
// again, the time since some -1,048,573, and goes on with the real time. A
// sum kept in the float itself would stand at the value set, the time
// since 0 throughout.
//=============================================================================
class LevelClockConsole extends Console;

var float RunTime, Since, NextLog;
var int Phase;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (Phase == 0)
	{
		if (RunTime < 3.0)
			return;
		P.Level.TimeSeconds = 1048576.0;
		Since = 0;
		NextLog = 0.25;
		Log("DXCLOCK: set " $ P.Level.TimeSeconds $ " dilation " $ P.Level.TimeDilation);
		Phase = 1;
		return;
	}
	if (Phase == 1)
	{
		Since += Delta;
		if (Since >= NextLog)
		{
			Log("DXCLOCK: real " $ Since $ " level " $ (P.Level.TimeSeconds - 1048576.0));
			NextLog += 0.25;
		}
		if (Since >= 2.0)
		{
			Log("DXCLOCK: done");
			P.ConsoleCommand("exit");
			Phase = 2;
		}
	}
}
