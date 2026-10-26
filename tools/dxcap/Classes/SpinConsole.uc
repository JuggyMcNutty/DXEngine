//=============================================================================
// SpinConsole: an actor's turn by its RotationRate, logged with "DXSPIN:"
// (docs/DEVELOPMENT.md, scripted runs). From wherever the run starts, 2 s
// in: four CapSpinners spawned over the player, in PHYS_Rotating -- one
// with a fixed direction alone, spinning at 3000 yaw units a second from
// 60000 (it wraps); one turning to a desired rotation the shortest way at
// 20000 a second, from yaw 1000 to 50000 (back through 0), its pitch from
// 0 to 3000 at 1000 a second; one with a fixed direction to a desired
// rotation, at 20000 a second from 1000 to 50000 (the long way, forward);
// one with a rate of 65536 a second, a whole turn, which the original
// drops (its rate's low 16 bits 0). Six frames are held half a second each,
// so their ticks are 0.4 s: each tick, every spinner's rotation, and the
// EndedRotation events. What is compared between the engines: every line.
//=============================================================================
class SpinConsole extends Console;

var float RunTime;
var int Phase, Holds;
var CapSpinner Spinners[4];

function Say(string S)
{
	Log("DXSPIN: " $ S);
}

// The frame held Seconds by line traces timed in short spans.
function Hold(PlayerPawn P, float Seconds)
{
	local vector HitLocation, HitNormal;
	local float Stalled;
	local int c, i, Spans;

	while (Stalled < Seconds && Spans < 4000)
	{
		c = 0;
		clock(c);
		for (i = 0; i < 1000; i++)
		{
			P.Trace(HitLocation, HitNormal, P.Location + vect(0,0,-2000), P.Location, false);
			P.Trace(HitLocation, HitNormal, P.Location + vect(0,0,-2000), P.Location, false);
			P.Trace(HitLocation, HitNormal, P.Location + vect(0,0,-2000), P.Location, false);
			P.Trace(HitLocation, HitNormal, P.Location + vect(0,0,-2000), P.Location, false);
		}
		unclock(c);
		Stalled += CyclesToSeconds(c);
		Spans++;
	}
}

function bool MakeSpinners(PlayerPawn P)
{
	local int i;

	for (i = 0; i < 4; i++)
	{
		Spinners[i] = P.Spawn(class'CapSpinner',,, P.Location + vect(0, 0, 100) + vect(40, 0, 0) * i);
		if (Spinners[i] == None)
		{
			Say("no spinner " $ i);
			return False;
		}
	}
	Spinners[0].bFixedRotationDir = True;
	Spinners[0].RotationRate = rot(0, 3000, 0);
	Spinners[0].SetRotation(rot(0, 60000, 0));

	Spinners[1].bRotateToDesired = True;
	Spinners[1].RotationRate = rot(1000, 20000, 0);
	Spinners[1].DesiredRotation = rot(3000, 50000, 0);
	Spinners[1].SetRotation(rot(0, 1000, 0));

	Spinners[2].bRotateToDesired = True;
	Spinners[2].bFixedRotationDir = True;
	Spinners[2].RotationRate = rot(0, 20000, 0);
	Spinners[2].DesiredRotation = rot(0, 50000, 0);
	Spinners[2].SetRotation(rot(0, 1000, 0));

	Spinners[3].bFixedRotationDir = True;
	Spinners[3].RotationRate = rot(0, 65536, 0);
	Spinners[3].SetRotation(rot(0, 100, 0));
	Say("spinners " $ Spinners[0].Name $ " " $ Spinners[1].Name $ " " $ Spinners[2].Name $ " " $ Spinners[3].Name $ " set");
	return True;
}

function Report()
{
	Say("rotations " $ Spinners[0].Rotation $ " | " $ Spinners[1].Rotation $ " | " $ Spinners[2].Rotation $ " | " $ Spinners[3].Rotation);
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
		Hold(P, 0.5);
		Phase = 1;
	}
	else if (Phase == 1)
	{
		if (!MakeSpinners(P))
		{
			Say("exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		Hold(P, 0.5);
		Holds = 1;
		Phase = 2;
	}
	else if (Phase == 2)
	{
		Report();
		Hold(P, 0.5);
		Holds++;
		if (Holds >= 6)
			Phase = 3;
	}
	else if (Phase == 3)
	{
		Report();
		Say("done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
