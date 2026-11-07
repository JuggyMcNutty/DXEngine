//=============================================================================
// PendingTouchConsole: an actor's pending touch with no physics, and
// AutonomousPhysics (bugs.md NET.25; docs/DEVELOPMENT.md, scripted runs),
// logged with "DXPTOUCH:". The original sends the pending touch's PostTouch
// at the end of performPhysics (Engine.dll 0x103c9917), which its actor
// tick calls only for an actor with physics (0x103a275c), as its
// AutonomousPhysics does (0x103c8fd4) after cutting the acceleration toward
// zero to a tenth, whatever the physics (0x103c8f60-0x103c8fce).
//
// From wherever the run starts, 2 s in, three CapTouchProbes over the
// player, none with physics: First, its PendingTouch the toucher; 1 s later
// what came, and First given rotating physics, which turns nothing; 1 s
// later what came, then Second, its PendingTouch the toucher and its
// acceleration (1.23456, -2.34567, 0.05), its AutonomousPhysics called for
// 0.02 s with no physics, what came and its acceleration after, and again
// with rotating physics, what came. Half a second on, an exit.
//
// The original's, so DXEngine's once fixed: no PostTouch while First has no
// physics, one from First once it has; none from Second's AutonomousPhysics
// with no physics, its acceleration 1.2,-2.3,0; one with rotating physics.
//=============================================================================
class PendingTouchConsole extends Console;

var float RunTime, StepTime;
var int Phase;
var CapTouchProbe Toucher, First, Second;

function string Came()
{
	return Toucher.PostTouches $ " PostTouch so far";
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (Phase == 0 && RunTime > 2.0)
	{
		Toucher = P.Spawn(class'CapTouchProbe',, 'Toucher', P.Location + vect(0, 0, 300));
		First = P.Spawn(class'CapTouchProbe',, 'First', P.Location + vect(0, 40, 300));
		Second = P.Spawn(class'CapTouchProbe',, 'Second', P.Location + vect(0, 80, 300));
		if (Toucher == None || First == None || Second == None)
		{
			Log("DXPTOUCH: probes not spawned");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		First.PendingTouch = Toucher;
		Log("DXPTOUCH: First, physics " $ First.Physics $ ", its pending touch the toucher");
		StepTime = RunTime;
		Phase = 1;
	}
	else if (Phase == 1 && RunTime - StepTime >= 1.0)
	{
		Log("DXPTOUCH: 1 s on, " $ Came() $ ", First's pending touch left " $ (First.PendingTouch != None));
		First.SetPhysics(PHYS_Rotating);
		Log("DXPTOUCH: First given physics " $ First.Physics);
		StepTime = RunTime;
		Phase = 2;
	}
	else if (Phase == 2 && RunTime - StepTime >= 1.0)
	{
		Log("DXPTOUCH: 1 s on, " $ Came() $ ", First's pending touch left " $ (First.PendingTouch != None));
		Second.PendingTouch = Toucher;
		Second.Acceleration = vect(1.23456, -2.34567, 0.05);
		Second.AutonomousPhysics(0.02);
		Log("DXPTOUCH: Second's AutonomousPhysics, physics " $ Second.Physics $ ": " $ Came() $ ", its acceleration " $ Second.Acceleration);
		Second.SetPhysics(PHYS_Rotating);
		Second.AutonomousPhysics(0.02);
		Log("DXPTOUCH: Second's AutonomousPhysics, physics " $ Second.Physics $ ": " $ Came());
		StepTime = RunTime;
		Phase = 3;
	}
	else if (Phase == 3 && RunTime - StepTime >= 0.5)
	{
		Log("DXPTOUCH: done, " $ Came() $ ", exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
