//=============================================================================
// CapGCStateWindow: a window of the package's own for GCStateConsole that
// logs the GC it is handed, with "DXGCSTATE:" in front, at its first draws
// (Logged of them): in DrawWindow, the GC's style, smoothing, masking,
// translucency, modulation and drawing as they come; then EnableTranslucency
// (True) and the style after it; then SetStyle(DSTY_Modulated) and the font
// set to FontMenuSmall, which the GC keeps into PostDrawWindow in the
// original (XWindow::DrawTree 0x10051490 pushes the window's GC once round
// its Draw, its children and its PostDraw); in PostDrawWindow, the style,
// modulation and the normal font as they come.
//=============================================================================
class CapGCStateWindow extends Window;

var string Label;
var int Logged;

function string FontOf(GC gc)
{
	local Font N, B;

	gc.GetFonts(N, B);
	if (N == None)
		return "None";
	return string(N.Name);
}

event DrawWindow(GC gc)
{
	if (Logged >= 2)
		return;
	Log("DXGCSTATE: " $ Label $ " draw: style " $ gc.GetStyle() $ ", smoothing " $ gc.IsSmoothingEnabled()
		$ ", masking " $ gc.IsMaskingEnabled() $ ", translucency " $ gc.IsTranslucencyEnabled()
		$ ", modulation " $ gc.IsModulationEnabled() $ ", drawing " $ gc.IsDrawingEnabled()
		$ ", font " $ FontOf(gc));
	gc.EnableTranslucency(True);
	Log("DXGCSTATE: " $ Label $ " after EnableTranslucency: style " $ gc.GetStyle());
	gc.SetStyle(DSTY_Modulated);
	gc.SetFont(Font'DeusExUI.FontMenuSmall');
}

event PostDrawWindow(GC gc)
{
	if (Logged >= 2)
		return;
	Logged++;
	Log("DXGCSTATE: " $ Label $ " post: style " $ gc.GetStyle() $ ", modulation " $ gc.IsModulationEnabled()
		$ ", font " $ FontOf(gc));
}
