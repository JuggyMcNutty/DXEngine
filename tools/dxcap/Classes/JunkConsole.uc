//=============================================================================
// JunkConsole: SetPropertyText and the console's SET with text that is no
// value of the property, logged with "DXJUNK:" (docs/DEVELOPMENT.md,
// scripted runs).
//
// From wherever the run starts, 2 s in: a CapJunk spawned over the player,
// each of its properties -- an int, a float, a bool, a byte, a string and a
// name, each set to a value first -- given "abc", then "", then "12abc" and
// "-7" by SetPropertyText, and the values logged after each; then the
// console's `set CapJunk I abc` and `set CapJunk F abc`, the values logged;
// and an exit. What is compared between the engines: every line, exactly.
//=============================================================================
class JunkConsole extends Console;

var float RunTime;
var bool bDone;

function Say(string S)
{
	Log("DXJUNK: " $ S);
}

function SetAll(CapJunk J, string Text)
{
	J.SetPropertyText("I", Text);
	J.SetPropertyText("F", Text);
	J.SetPropertyText("B", Text);
	J.SetPropertyText("Bt", Text);
	J.SetPropertyText("S", Text);
	J.SetPropertyText("N", Text);
	Say("after [" $ Text $ "]: " $ J.Values());
}

function Probes()
{
	local CapJunk J;

	J = Viewport.Actor.Spawn(class'CapJunk',,, Viewport.Actor.Location + vect(0, 0, 200));
	if (J == None)
	{
		Say("no actor spawned");
		return;
	}
	Say("before: " $ J.Values());
	SetAll(J, "abc");
	SetAll(J, "");
	SetAll(J, "12abc");
	SetAll(J, "-7");
	SetAll(J, "True");
	SetAll(J, "0");
	Viewport.Actor.ConsoleCommand("set CapJunk I abc");
	Viewport.Actor.ConsoleCommand("set CapJunk F abc");
	Viewport.Actor.ConsoleCommand("set CapJunk B maybe");
	Say("after set abc: " $ J.Values());
	Viewport.Actor.ConsoleCommand("set CapJunk I 42");
	Viewport.Actor.ConsoleCommand("set CapJunk F 2.5");
	Viewport.Actor.ConsoleCommand("set CapJunk B false");
	Say("after set values: " $ J.Values());
	J.Destroy();
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
