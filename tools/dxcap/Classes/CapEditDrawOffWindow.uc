//=============================================================================
// CapEditDrawOffWindow: an edit window of the package's own for
// EditDrawOffConsole whose script DrawWindow, with bDrawOff, turns the GC's
// drawing off. The original's XEditWindow::Draw (Extension.dll 0x10020d00),
// editable and with the focus, draws its rows after the script's DrawWindow
// through the GC as the script left it -- none of them with drawing off,
// XGC::DrawTextLine (0x10028470) and DrawTextChar drawing nothing then --
// and then its insertion point after a SetStyle(DSTY_Masked), which turns
// drawing on again. Without the focus it draws as a large text window, its
// rows alone. Its text MMMMMMMM in green (FontMenuSmall), its insertion
// point at the text's end, a white bar 4 wide that stays lit for 1,000 s
// after each move.
//=============================================================================
class CapEditDrawOffWindow extends EditWindow;

var bool bDrawOff;

function Setup(bool bOff)
{
	local Color White;

	bDrawOff = bOff;
	White.R = 255;
	White.G = 255;
	White.B = 255;
	SetFont(Font'DeusExUI.FontMenuSmall');
	SetTextColorRGB(0, 255, 0);
	SetInsertionPointTexture(Texture'Solid', White);
	SetInsertionPointType(INSTYPE_Insert, 4);
	SetInsertionPointBlinkRate(1000.0, 1.0);
	SetText("MMMMMMMM");
	SetInsertionPoint(8);
}

event DrawWindow(GC gc)
{
	if (bDrawOff)
		gc.EnableDrawing(False);
}
