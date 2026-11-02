//=============================================================================
// CapLayoutWindow: a window of the package's own for LayoutConsole that
// places two children itself as it is laid out: its first (bottom) child at
// 40, 30 and its second at 60, 40, each 20 by 20, the first left out while
// bLeaveOutFirst is set. A child's request goes on up to its own parent, as
// a menu's does, so that the holder is laid out again with it.
//=============================================================================
class CapLayoutWindow extends Window;

var bool bLeaveOutFirst;

event ConfigurationChanged()
{
	local Window C;
	local int n;

	for (C = GetBottomChild(False); C != None; C = C.GetHigherSibling(False))
	{
		if (n == 0 && !bLeaveOutFirst)
			C.ConfigureChild(40, 30, 20, 20);
		else if (n == 1)
			C.ConfigureChild(60, 40, 20, 20);
		n++;
	}
}

event bool ChildRequestedReconfiguration(Window childWin)
{
	return False;
}
