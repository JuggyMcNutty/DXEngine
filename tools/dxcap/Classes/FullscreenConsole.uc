//=============================================================================
// FullscreenConsole: the window full screen and back -- on the map the run
// starts on, 4 s in, a shot in the window, then togglefullscreen and a shot
// 3 s later, then togglefullscreen again and a shot 3 s later, each with
// the root's size logged (DXEngine's shots are the device's frame at its
// size). Exits 2 s after the last. Each line is logged with "DXFULL:".
//=============================================================================
class FullscreenConsole extends Console;

var float StepTime;
var int Phase;

function Report(PlayerPawn P, string What)
{
	local DeusExRootWindow Root;

	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
	if (Root != None)
		Log("DXFULL: " $ What $ ": root " $ Root.width $ " x " $ Root.height);
	else
		Log("DXFULL: " $ What);
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	P.ReducedDamageType = 'All';
	StepTime += Delta;

	if (Phase == 0 && StepTime > 4.0)
	{
		Report(P, "shot 0 in the window");
		P.ConsoleCommand("shot");
		P.ConsoleCommand("togglefullscreen");
		StepTime = 0;
		Phase = 1;
	}
	else if (Phase == 1 && StepTime > 3.0)
	{
		Report(P, "shot 1 full screen");
		P.ConsoleCommand("shot");
		P.ConsoleCommand("togglefullscreen");
		StepTime = 0;
		Phase = 2;
	}
	else if (Phase == 2 && StepTime > 3.0)
	{
		Report(P, "shot 2 in the window again");
		P.ConsoleCommand("shot");
		StepTime = 0;
		Phase = 3;
	}
	else if (Phase == 3 && StepTime > 2.0)
	{
		Log("DXFULL: exiting");
		P.ConsoleCommand("exit");
		Phase = 4;
	}
}
