//=============================================================================
// WindowPathConsole: a window's outer (bugs.md EXT.38), logged with
// "DXWINPATH:". 3 s into the map the run starts on: the player's root
// window, a new child of it and a child of that child, each by its path,
// its Outer and its Name; the child destroyed, then an exit. The original
// constructs a new window in its parent and the root in the player's pawn,
// each named after its class and a number.
//=============================================================================
class WindowPathConsole extends Console;

var float RunTime;
var bool bDone;

function LogWindow(string What, Window W)
{
	Log("DXWINPATH: " $ What $ " " $ W $ " outer " $ W.Outer $ " name " $ W.Name);
}

event Tick(float Delta)
{
	local PlayerPawnExt P;
	local Window A, B;

	Super.Tick(Delta);
	if (bDone || Viewport == None || PlayerPawnExt(Viewport.Actor) == None)
		return;
	RunTime += Delta;
	if (RunTime < 3.0)
		return;
	bDone = true;
	P = PlayerPawnExt(Viewport.Actor);
	LogWindow("root", P.rootWindow);
	A = P.rootWindow.NewChild(class'Window');
	B = A.NewChild(class'Window');
	LogWindow("child", A);
	LogWindow("grandchild", B);
	A.Destroy();
	P.ConsoleCommand("exit");
}
