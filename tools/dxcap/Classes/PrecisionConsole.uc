//=============================================================================
// PrecisionConsole: the precision the x87 works a script's sums out at, in
// the console's tick and in the level's, logged with "DXPRECISION:"
// (docs/DEVELOPMENT.md, scripted runs). 2 s in, from wherever the run
// starts: 7 *= 3e9 worked out in the console's own Tick, and a
// CapPrecisionProbe spawned, which works it out in its own Tick, in the
// level's tick; 1 s later an exit. The original's product, rounded to a
// float at 24 bits, is -474,836,992 (Core.dll 0x10134280), at 53 or 64
// bits -474,836,480.
//=============================================================================
class PrecisionConsole extends Console;

var float RunTime;
var int Phase;

event Tick(float Delta)
{
	local int I;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		I = 7;
		I *= 3000000000.0;
		Log("DXPRECISION: in the console's tick, 7 *= 3e9 -> " $ I);
		Viewport.Actor.Spawn(class'CapPrecisionProbe');
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		Log("DXPRECISION: done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 9;
	}
}
