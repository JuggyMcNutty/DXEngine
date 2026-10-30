//=============================================================================
// CapActorWindow: a window of the package's own that draws its Target with
// GC.DrawActor from its DrawWindow, constrained to the window or not as
// bConstrain says, unlit, over the scene's depth (ConstrainConsole).
//=============================================================================
class CapActorWindow extends Window;

var Actor Target;
var bool bConstrain;

event DrawWindow(GC gc)
{
	if (Target != None)
		gc.DrawActor(Target, false, bConstrain, true, 1.0, 1.0, None);
}
