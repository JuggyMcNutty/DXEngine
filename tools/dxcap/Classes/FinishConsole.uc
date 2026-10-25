//=============================================================================
// FinishConsole: what FinishAnim waits for, logged with "DXFINISH:"
// (docs/DEVELOPMENT.md, scripted runs).
//
// From wherever the run starts, 2 s in: a CapFinisher spawned over the
// player, whose state code calls FinishAnim after a PlayAnim, after a
// TweenAnim with a time, with nothing animating, after a LoopAnim and once
// more; the level's time, the frame and the rate logged as each returns;
// 6 s later an exit. What is compared between the engines: the time each
// FinishAnim takes -- none with nothing animating, a tween's whole time
// after a TweenAnim.
//=============================================================================
class FinishConsole extends Console;

var float RunTime;
var int Phase;
var CapFinisher F;

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (Phase == 2 || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		F = Viewport.Actor.Spawn(class'CapFinisher',,, Viewport.Actor.Location + vect(0, 0, 300));
		if (F == None)
		{
			Log("DXFINISH: no actor spawned");
			Phase = 2;
			Viewport.Actor.ConsoleCommand("exit");
			return;
		}
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 8.0)
	{
		Log("DXFINISH: done, exiting");
		F.Destroy();
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 2;
	}
}
