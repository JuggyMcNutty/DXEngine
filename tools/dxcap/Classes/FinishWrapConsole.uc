//=============================================================================
// FinishWrapConsole: a FinishAnim whose animation changes while it waits,
// logged with "DXWRAP:" (bugs.md AI.15; docs/DEVELOPMENT.md, scripted runs).
//
// From wherever the run starts, 2 s in: a CapWrapFinisher spawned over the
// player, whose state code waits in FinishAnim while its Timer loops the
// animation, then while its AnimEnd plays it again; 8 s later an exit. What
// is compared between the engines: bAnimFinished as the loop's AnimEnd and
// the wait's end see it, and the time the second wait takes -- the
// original's poll waits on through the animation its AnimEnd started.
//=============================================================================
class FinishWrapConsole extends Console;

var float RunTime;
var int Phase;
var CapWrapFinisher F;

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (Phase == 2 || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		F = Viewport.Actor.Spawn(class'CapWrapFinisher',,, Viewport.Actor.Location + vect(0, 0, 300));
		if (F == None)
		{
			Log("DXWRAP: no actor spawned");
			Phase = 2;
			Viewport.Actor.ConsoleCommand("exit");
			return;
		}
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 10.0)
	{
		Log("DXWRAP: done, exiting");
		F.Destroy();
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 2;
	}
}
