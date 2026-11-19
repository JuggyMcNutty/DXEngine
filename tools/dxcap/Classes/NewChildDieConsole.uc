//=============================================================================
// NewChildDieConsole (bugs.md EXT.45): a window that destroys itself while
// NewChild makes it, logged with "DXDIE:" in front, in both engines. On the
// menu map, 3 s in, a plain window on the root, then a case every half
// second, each a CapW6Die made in it by NewChild, shown or not:
//   1 the window destroys itself in its InitWindow;
//   2 in its first VisibilityChanged(True) (made hidden, it is then shown
//     by Show);
//   3 in its first ConfigurationChanged (made hidden, it is destroyed by
//     the console).
// For each: the events the window gets, in order, whether its maker got a
// window or None, and how many children the parent then has. Then exits.
//=============================================================================
class NewChildDieConsole extends Console;

var float RunTime;
var float NextStep;
var int Step;
var Window Box;

function int Children(Window W)
{
	local Window C;
	local int n;

	for (C = W.GetTopChild(False); C != None; C = C.GetLowerSibling(False))
		n++;
	return n;
}

function Try(int Die, bool bShow)
{
	local Window W;

	class'CapW6Die'.default.Die = Die;
	Log("DXDIE: case " $ Die $ ", shown " $ bShow $ ": begins");
	W = Box.NewChild(class'CapW6Die', bShow);
	Log("DXDIE: case " $ Die $ ", shown " $ bShow $ ": the maker got a window " $ (W != None)
		$ ", the parent's children " $ Children(Box));
	if (W != None && Die == 2)
	{
		W.Show();
		Log("DXDIE: case " $ Die $ ", shown " $ bShow $ ": after Show, the parent's children " $ Children(Box));
	}
	else if (W != None)
	{
		W.Destroy();
		Log("DXDIE: case " $ Die $ ", shown " $ bShow $ ": destroyed by its maker, the parent's children "
			$ Children(Box));
	}
}

event Tick(float Delta)
{
	local PlayerPawn Pawn;
	local DeusExRootWindow Root;

	Super.Tick(Delta);
	if (Step == 99 || Viewport == None || Viewport.Actor == None)
		return;
	Pawn = Viewport.Actor;
	RunTime += Delta;
	if (RunTime < 3.0 || RunTime < NextStep || DeusExPlayer(Pawn) == None)
		return;
	Root = DeusExRootWindow(DeusExPlayer(Pawn).rootWindow);
	if (Root == None)
		return;
	NextStep = RunTime + 0.5;
	switch (Step)
	{
	case 0:
		Box = Root.NewChild(class'Window');
		Box.SetPos(100, 100);
		Box.SetSize(300, 200);
		break;
	case 1: Try(1, True); break;
	case 2: Try(1, False); break;
	case 3: Try(2, True); break;
	case 4: Try(2, False); break;
	case 5: Try(3, True); break;
	case 6: Try(3, False); break;
	default:
		Log("DXDIE: done, exiting");
		Pawn.ConsoleCommand("exit");
		Step = 99;
		return;
	}
	Step++;
}
