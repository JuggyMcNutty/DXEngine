//=============================================================================
// NameConsole: the names spawned actors get, logged with "DXNAME:"
// (docs/DEVELOPMENT.md, scripted runs).
//
// From wherever the run starts, 2 s in: a Lamp1, a Lamp2, a Lamp3 and a
// second Lamp1 -- classes of one name stem, each counting its own objects --
// then an Image15_Area51_Sector3 and an Image15_Area51_Sector4, as
// DeusExPlayer.AllImages spawns them, each spawned 200 over the player and
// its name logged, then destroyed; and an exit. What is compared between
// the engines: every line, exactly.
//=============================================================================
class NameConsole extends Console;

var float RunTime;
var bool bDone;

function Say(string S)
{
	Log("DXNAME: " $ S);
}

function Actor One(class<Actor> C)
{
	local Actor A;

	A = Viewport.Actor.Spawn(C,,, Viewport.Actor.Location + vect(0, 0, 200));
	if (A == None)
		Say(C.Name $ " spawned None");
	else
		Say(C.Name $ " spawned as " $ A.Name);
	return A;
}

function Probes()
{
	local Actor A[6];
	local int i;

	A[0] = One(class'Lamp1');
	A[1] = One(class'Lamp2');
	A[2] = One(class'Lamp3');
	A[3] = One(class'Lamp1');
	A[4] = One(class'Image15_Area51_Sector3');
	A[5] = One(class'Image15_Area51_Sector4');
	for (i = 0; i < 6; i++)
		if (A[i] != None)
			A[i].Destroy();
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime > 2.0)
	{
		Probes();
		Say("done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		bDone = true;
	}
}
