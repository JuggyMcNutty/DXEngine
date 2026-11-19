//=============================================================================
// FrameOrderConsole: what the first frame drawn after a window an event
// makes shows of it (bugs.md ENG.57), logged with "DXORDER:". At each draw
// the root's top child: when it changes, that frame and the two after it
// are logged with their numbers, the top child's class and the root's focus
// window; at 20 s an exit. Run with Esc pressed in play some 6 s in (the
// main menu opened by the key's event): DXEngine by a timeline
// (tools/dxcap/timelines/esc-menu.txt), the original by the desktop's
// keyboard. The original takes its window's messages at a frame's end, so
// the menu ticks before its first draw (dx-reverse-info launch-flow.md, the
// main loop): its first frame drawn with the menu has the menu's focus.
// Compared: the three lines after the change, in both engines.
//=============================================================================
class FrameOrderConsole extends Console;

var float RunTime;
var int Frames;
var int After;
var Window LastTop;
var bool bDone;

event Tick(float Delta)
{
	Super.Tick(Delta);
	RunTime += Delta;
	if (!bDone && RunTime > 20.0 && Viewport != None && Viewport.Actor != None)
	{
		bDone = true;
		Viewport.Actor.ConsoleCommand("exit");
	}
}

event PostRender(canvas C)
{
	local DeusExRootWindow Root;
	local Window Top;

	Super.PostRender(C);
	Frames++;
	if (Viewport == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	Root = DeusExRootWindow(DeusExPlayer(Viewport.Actor).rootWindow);
	if (Root == None)
		return;
	Top = Root.GetTopChild();
	if (Top != LastTop)
	{
		LastTop = Top;
		After = 3;
	}
	if (After > 0)
	{
		After--;
		Log("DXORDER: frame " $ Frames $ " drawn at " $ RunTime $ " s: top " $ Top $ ", focus " $ Root.GetFocusWindow());
	}
}
