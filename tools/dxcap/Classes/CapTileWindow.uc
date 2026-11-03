//=============================================================================
// CapTileWindow: a window of the package's own that draws TileConsole's
// window tiles and text over a grey box of its own: StaticLine and
// HackInfoBackground DSTY_Modulated, HUDScopeCrosshair's middle DSTY_Normal,
// and "NEW GAME" in the menu's TrueType font and in FontMenuHeaders; once,
// the GC's widths of characters past the font's page logged.
//=============================================================================
class CapTileWindow extends Window;

var Font TrueType;
var bool bWidthsLogged;

event DrawWindow(GC gc)
{
	gc.EnableSmoothing(False);
	gc.SetStyle(DSTY_Normal);
	gc.SetTileColorRGB(160, 160, 160);
	gc.DrawPattern(0, 0, width, height, 0, 0, Texture'Solid');

	gc.SetStyle(DSTY_Modulated);
	gc.DrawStretchedTexture(20, 20, 128, 64, 0, 0, 16, 8, Texture'StaticLine');
	gc.DrawStretchedTexture(180, 20, 64, 64, 0, 0, 16, 16, Texture'HackInfoBackground');

	gc.SetStyle(DSTY_Normal);
	gc.SetTileColorRGB(255, 255, 255);
	gc.DrawTexture(280, 20, 128, 128, 64, 64, Texture'HUDScopeCrosshair');

	gc.EnableTranslucentText(False);
	gc.SetAlignments(HALIGN_Left, VALIGN_Top);
	gc.SetTextColorRGB(255, 255, 255);
	gc.SetFont(TrueType);
	gc.DrawText(20, 160, 300, 40, "NEW GAME");
	gc.SetFont(Font'DeusExUI.FontMenuHeaders');
	gc.DrawText(20, 220, 300, 40, "NEW GAME");
	if (!bWidthsLogged)
		LogWidths(gc);
}

// A character past the font's page in the window's GC: U+0100, U+03A9, and
// U+0100 between two A's, against A alone
function LogWidths(GC gc)
{
	local float W, H;
	local string S;

	bWidthsLogged = True;
	gc.GetTextExtent(0, W, H, "A");
	S = "DXTILE: window widths: A " $ W $ "x" $ H;
	gc.GetTextExtent(0, W, H, Chr(256));
	S = S $ ", U+0100 " $ W $ "x" $ H;
	gc.GetTextExtent(0, W, H, Chr(937));
	S = S $ ", U+03A9 " $ W $ "x" $ H;
	gc.GetTextExtent(0, W, H, "A" $ Chr(256) $ "A");
	Log(S $ ", A U+0100 A " $ W $ "x" $ H);
}
