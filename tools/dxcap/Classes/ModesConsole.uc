//=============================================================================
// ModesConsole: the sizes the Display menu offers (GETRES) and the window's
// own (GETCURRENTRES) as the window's display has them, in a window and full
// screen (bugs.md WIN.14). On the map the run starts on: the sizes logged at
// 3 s; MoveTime seconds (15) later, once whatever moves the window has moved
// it (xdotool's windowmove; under Wine a Windows program's SetWindowPos),
// logged again, then togglefullscreen; 4 s later logged full screen, then
// togglefullscreen; 4 s later logged in the window again; an exit 2 s
// after. Each with the root window's size. DXEngine alone: each line starts
// "DXMODES:".
//=============================================================================
class ModesConsole extends Console;

var config float MoveTime;

var float MapTime, StepTime;
var int Phase;

function Report(PlayerPawn P, string What)
{
	local DeusExRootWindow Root;
	local string Size;

	Size = "";
	if (DeusExPlayer(P) != None)
	{
		Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
		if (Root != None)
			Size = ", root " $ Root.width $ "x" $ Root.height;
	}
	Log("DXMODES: " $ What $ ": current " $ P.ConsoleCommand("GetCurrentRes") $ Size);
	Log("DXMODES: " $ What $ ": offered " $ P.ConsoleCommand("GetRes"));
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	P.ReducedDamageType = 'All';
	MapTime += Delta;
	StepTime += Delta;

	if (Phase == 0 && MapTime > 3.0)
	{
		Report(P, "at the start");
		StepTime = 0;
		Phase = 1;
	}
	else if (Phase == 1 && StepTime > MoveTime)
	{
		Report(P, "before full screen");
		P.ConsoleCommand("togglefullscreen");
		StepTime = 0;
		Phase = 2;
	}
	else if (Phase == 2 && StepTime > 4.0)
	{
		Report(P, "full screen");
		P.ConsoleCommand("togglefullscreen");
		StepTime = 0;
		Phase = 3;
	}
	else if (Phase == 3 && StepTime > 4.0)
	{
		Report(P, "in the window again");
		StepTime = 0;
		Phase = 4;
	}
	else if (Phase == 4 && StepTime > 2.0)
	{
		Log("DXMODES: exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}

defaultproperties
{
	MoveTime=15.0
}
