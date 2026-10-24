//=============================================================================
// ProjectionWindow: a window of the package's own that logs, with
// "DXFROB:", what ConvertVectorToCoordinates gives it for the points its
// console sets, each place and whether it was returned in front, once each
// time asked. It asks from its own DrawWindow, as the frob highlight does:
// the original's root has the scene's frame only while its windows draw
// (Extension.dll XRootWindow::PostRender), and asserts without it.
// FrobConsole puts one on the root window.
//=============================================================================
class ProjectionWindow extends Window;

var vector Points[12];
var string Labels[12];
var int NumPoints;
var bool bLogNext;

function AddPoint(string Label, vector V)
{
	if (NumPoints >= 12)
		return;
	Labels[NumPoints] = Label;
	Points[NumPoints] = V;
	NumPoints++;
}

event DrawWindow(GC gc)
{
	local int i;
	local float X, Y;
	local bool bFront;

	if (!bLogNext)
		return;
	bLogNext = false;
	for (i = 0; i < NumPoints; i++)
	{
		bFront = ConvertVectorToCoordinates(Points[i], X, Y);
		Log("DXFROB: projected " $ Labels[i] $ " at " $ X $ "," $ Y $ ", in front " $ bFront);
	}
	NumPoints = 0;
}
