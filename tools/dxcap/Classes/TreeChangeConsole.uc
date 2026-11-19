//=============================================================================
// TreeChangeConsole (bugs.md EXT.44, EXT.42): the window tree changed by a
// window's own script from inside the native walks, logged with "DXWIN6:"
// in front, in both engines. On the menu map, 3 s in, a step every half
// second:
//  1. the draw walk: a plain window P on the root holding A, B (destroys
//     itself in its first DrawWindow) and C, then a window Q over P, all
//     full tiles of their colours. Look for: is C drawn in the frame B
//     dies (DXEngine follows B's nextSibling after B's draw: none); is Q
//     drawn whole that frame (DXEngine pushes B's clip and pops it only
//     while B still has a parent). Shots 0 and 1: the frame of the destroy
//     is the one to compare.
//  2. the draw walk again, a window that raises itself in its DrawWindow
//     (D, E raises, F): F drawn in that frame?
//  3. the layout walk: a TabGroupWindow sized to its parent's (children
//     sized to it) holding G, H (destroys itself in ConfigurationChanged)
//     and I; then the group resized. Look for: which of G and I are
//     configured (DXEngine's UTabGroupWindow::ConfigurationChanged walks
//     prevSibling live).
//  4. the tick walk: J holding K (destroys J in its Tick) and L: L ticks?
//  5. the grabs: the main menu up, the counts; a CapW6Modal made that
//     destroys itself as it shows; the counts again (expect them
//     unchanged), then the menu's keys: Esc closes it?
//  6. timers: AddTimer(0, True), AddTimer(-1, False) and AddTimer(0.0001,
//     True) on a window, their invocations over 5 ticks.
// Then exits.
//=============================================================================
class TreeChangeConsole extends Console;

// Skip: the steps left out, their numbers as below less 1 ("0" leaves out
// the window that destroys itself in its draw, at which the original stops)
var config string Skip;

var float RunTime;
var float NextStep;
var int Step;
var Window P, Q, Grp, J, T;
var CapW6Modal M;

function CapW6Win Make(Window Parent, string Label, int Mode, byte R, byte G, byte B, float X, float Y, float W, float H)
{
	local CapW6Win Win;
	local Color C;

	C.R = R; C.G = G; C.B = B;
	Win = CapW6Win(Parent.NewChild(class'CapW6Win'));
	Win.Setup(Label, Mode, C);
	Win.SetPos(X, Y);
	Win.SetSize(W, H);
	return Win;
}

function Note(DeusExRootWindow Root, string What)
{
	Log("DXWIN6: " $ What $ ": keyboard " $ Root.handleKeyboardRef $ ", mouse " $ Root.handleMouseRef
		$ ", focus " $ Root.GetFocusWindow() $ " at " $ RunTime);
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

	if (Step < 7 && InStr(Skip, string(Step)) >= 0)
	{
		Log("DXWIN6: step " $ Step $ " left out");
		Step++;
		return;
	}
	switch (Step)
	{
	case 0:
		Root.ClearWindowStack();
		Note(Root, "start");
		P = Make(Root, "P", 0, 40, 40, 40, 100, 100, 300, 200);
		Make(P, "A", 0, 255, 0, 0, 10, 10, 80, 80);
		Make(P, "B", 1, 0, 255, 0, 110, 10, 80, 80);
		Make(P, "C", 0, 0, 0, 255, 210, 10, 80, 80);
		Q = Make(Root, "Q", 0, 255, 255, 0, 50, 250, 500, 150);
		Log("DXCAP: shot 0");
		Pawn.ConsoleCommand("shot");
		break;
	case 1:
		Log("DXCAP: shot 1");
		Pawn.ConsoleCommand("shot");
		if (P != None)
			P.Destroy();
		if (Q != None)
			Q.Destroy();
		P = Make(Root, "P2", 0, 40, 40, 40, 100, 100, 300, 200);
		Make(P, "D", 0, 255, 0, 0, 10, 10, 80, 80);
		Make(P, "E", 4, 0, 255, 0, 110, 10, 80, 80);
		Make(P, "F", 0, 0, 0, 255, 210, 10, 80, 80);
		break;
	case 2:
		if (P != None)
			P.Destroy();
		Grp = Root.NewChild(class'TabGroupWindow');
		TabGroupWindow(Grp).bSizeChildrenToParent = True;
		Grp.SetPos(100, 100);
		Make(Grp, "G", 0, 255, 0, 0, 0, 0, 10, 10);
		Make(Grp, "H", 2, 0, 255, 0, 0, 0, 10, 10);
		Make(Grp, "I", 0, 0, 0, 255, 0, 0, 10, 10);
		Log("DXWIN6: the group resized");
		Grp.SetSize(200, 120);
		break;
	case 3:
		if (Grp != None)
			Grp.Destroy();
		J = Make(Root, "J", 0, 40, 40, 40, 100, 100, 300, 200);
		Make(J, "L", 0, 255, 0, 0, 10, 10, 80, 80);
		Make(J, "K", 3, 0, 255, 0, 110, 10, 80, 80);
		break;
	case 4:
		Root.InvokeMenu(class'MenuMain');
		Note(Root, "the main menu up");
		M = CapW6Modal(Root.NewChild(class'CapW6Modal', False));
		M.bDie = True;
		M.Show();
		Note(Root, "a modal destroyed as it showed");
		break;
	case 5:
		Root.ClearWindowStack();
		Note(Root, "the stack cleared");
		T = Make(Root, "T", 0, 40, 40, 40, 0, 0, 10, 10);
		T.AddTimer(0.0, True, 1, 'W6Timer');
		T.AddTimer(-1.0, False, 2, 'W6Timer');
		T.AddTimer(0.0001, True, 3, 'W6Timer');
		break;
	case 6:
		if (T != None)
			T.Destroy();
		break;
	default:
		Log("DXWIN6: done, exiting");
		Pawn.ConsoleCommand("exit");
		Step = 99;
		return;
	}
	Step++;
}
