//=============================================================================
// SimRoleConsole: a pawn made a simulated proxy in single player, logged with
// "DXSIMROLE:" (bugs.md NET.16; docs/DEVELOPMENT.md, scripted runs).
//
// From wherever the run starts, 2 s in: a CapSimPawn spawned over the player
// and made a simulated proxy, with a velocity of 40 along X, a life span of
// 1.5 s, a timer every 0.5 s and state code that logs every 0.5 s; 3 s later
// how far it moved, its Ticks, Timers and state code passes, or that it is
// gone; then an exit. What is compared between the engines: the original's
// pawn moves 120 units and runs its Tick alone, whatever the net mode.
//=============================================================================
class SimRoleConsole extends Console;

var float RunTime;
var int Phase;
var CapSimPawn P;

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (Phase == 2 || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		P = Viewport.Actor.Spawn(class'CapSimPawn',,, Viewport.Actor.Location + vect(0, 0, 300));
		if (P == None)
		{
			Log("DXSIMROLE: no pawn spawned");
			Phase = 2;
			Viewport.Actor.ConsoleCommand("exit");
			return;
		}
		P.Simulate(vect(40, 0, 0), 1.5);
		Log("DXSIMROLE: " $ P.Name $ " a simulated proxy: role " $ P.Role $ ", remote role " $ P.RemoteRole $ ", net mode " $ P.Level.NetMode $ ", physics " $ P.Physics);
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 5.0)
	{
		if (P == None)
			Log("DXSIMROLE: 3 s on, the pawn is gone");
		else if (P.bDeleteMe)
			Log("DXSIMROLE: 3 s on, the pawn is gone, after " $ P.Ticks $ " Ticks, " $ P.Timers $ " Timers, " $ P.Passes $ " state code passes");
		else
		{
			Log("DXSIMROLE: 3 s on, moved " $ (P.Location.X - P.StartedAt.X) $ " along X, " $ P.Ticks $ " Ticks, " $ P.Timers $ " Timers, " $ P.Passes $ " state code passes, role " $ P.Role);
			P.Destroy();
		}
		Log("DXSIMROLE: done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 2;
	}
}
