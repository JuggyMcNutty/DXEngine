//=============================================================================
// StrafeConsole: a strafe's move timer, logged with "DXSTRAFE:"
// (docs/DEVELOPMENT.md, scripted runs).
//
// From wherever the run starts, 2 s in: two CapStrafers, plain pawns,
// spawned 300 units over the player, one left there with no physics
// (aloft), one 120 units aside set falling, to land and walk (walker); 2 s
// after, each strafes 200 units along X, its MoveTimer, DesiredSpeed and
// physics logged at its first three ticks after, and the time the strafe
// took as it ends; 6 s later an exit. What is compared between the engines:
// each MoveTimer at the strafe's start, within a tick -- the walker's 1 +
// 1.3 x 200 / (320 x its DesiredSpeed, all of MaxDesiredSpeed), counting
// down, the aloft one's 1 + 1.3 x 200 / 200, which does not count, its
// strafe never over; and that the walker's strafe ends.
//=============================================================================
class StrafeConsole extends Console;

var float RunTime;
var int Phase;
var CapStrafer Aloft, Walker;

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (Phase == 2 || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		Aloft = Viewport.Actor.Spawn(class'CapStrafer',,, Viewport.Actor.Location + vect(0, 0, 300));
		Walker = Viewport.Actor.Spawn(class'CapStrafer',,, Viewport.Actor.Location + vect(120, 0, 300));
		if (Aloft == None || Walker == None)
		{
			Log("DXSTRAFE: a pawn did not spawn");
			Phase = 2;
			Viewport.Actor.ConsoleCommand("exit");
			return;
		}
		Aloft.Who = "aloft";
		Walker.Who = "walker";
		Walker.SetPhysics(PHYS_Falling);
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 8.0)
	{
		Log("DXSTRAFE: done, exiting");
		Aloft.Destroy();
		Walker.Destroy();
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 2;
	}
}
