//=============================================================================
// OverlayConsole: what Canvas.DrawActor and Canvas.DrawClippedActor leave of
// their actor's bHidden, logged with "DXOVERLAY:" from wherever the run
// starts (docs/DEVELOPMENT.md, scripted runs; bugs.md E10a.25). 2 s in, from
// the console's PostRender: the player's pawn, visible, drawn with
// Canvas.DrawActor and its bHidden logged after; hidden, drawn and logged;
// visible again, drawn with Canvas.DrawClippedActor into a 128 by 128 frame
// and logged; its flag put back. Then an exit. The original's natives clear
// the flag for the draw and set it afterwards, whatever it was (Engine.dll
// UCanvas::execDrawActor, execDrawClippedActor), where GC.DrawActor puts it
// back; DXEngine once put it back for all three.
//=============================================================================
class OverlayConsole extends Console;

var float RunTime;
var int Phase;
var bool bProbe;

event PostRender(canvas C)
{
	local PlayerPawn P;
	local bool bBefore;

	Super.PostRender(C);
	if (!bProbe || Viewport == None || Viewport.Actor == None)
		return;
	bProbe = false;
	P = Viewport.Actor;

	bBefore = P.bHidden;
	P.bHidden = false;
	C.DrawActor(P, false);
	Log("DXOVERLAY: DrawActor of the visible pawn: hidden after " $ P.bHidden);

	P.bHidden = true;
	C.DrawActor(P, false);
	Log("DXOVERLAY: DrawActor of the hidden pawn: hidden after " $ P.bHidden);

	P.bHidden = false;
	C.DrawClippedActor(P, false, 128, 128, 0, 0);
	Log("DXOVERLAY: DrawClippedActor of the visible pawn: hidden after " $ P.bHidden);

	P.bHidden = bBefore;
	Phase = 1;
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	P.ReducedDamageType = 'All';
	if (Phase == 0 && RunTime > 2.0)
		bProbe = true;
	else if (Phase == 1 && RunTime > 3.0)
	{
		Log("DXOVERLAY: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
