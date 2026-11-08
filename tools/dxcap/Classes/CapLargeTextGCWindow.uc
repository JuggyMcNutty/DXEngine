//=============================================================================
// CapLargeTextGCWindow: a large text window, centred both ways and not word
// wrapped, whose script DrawWindow logs ("DXEDIT:"), once, the alignments and
// the word wrap of the GC it is handed. UndoConsole puts one on the main menu.
// The original's XLargeTextWindow::Draw (Extension.dll 0x1002e500) calls the
// script with the window's own GC as XGC's constructor (0x10026ad0) starts it,
// Left, Top and word wrap on (0, 0, True), where XTextWindow::Draw (0x10046930)
// sets the window's first; DXEngine's ULargeTextWindow::DrawWindow sets the
// window's (1, 1, False).
//=============================================================================
class CapLargeTextGCWindow extends LargeTextWindow;

var bool bLogged;

event InitWindow()
{
	Super.InitWindow();
	SetTextAlignments(HALIGN_Center, VALIGN_Center);
	SetWordWrap(False);
}

event DrawWindow(GC gc)
{
	if (bLogged)
		return;
	bLogged = True;
	Log("DXEDIT: a large text window's DrawWindow: the GC's alignments " $ int(gc.GetHorizontalAlignment())
		$ ", " $ int(gc.GetVerticalAlignment()) $ ", word wrap " $ gc.IsWordWrapEnabled());
}
