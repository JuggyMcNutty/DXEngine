//=============================================================================
// DrawCountWindow: a window of the package's own that counts the DrawWindow
// and PostDrawWindow events it is sent, until its console clears them.
// ClipDrawConsole puts them in and out of a clip window's view.
//=============================================================================
class DrawCountWindow extends Window;

var int Draws, PostDraws;

event DrawWindow(GC gc)
{
	Draws++;
}

event PostDrawWindow(GC gc)
{
	PostDraws++;
}

function ClearCounts()
{
	Draws = 0;
	PostDraws = 0;
}
