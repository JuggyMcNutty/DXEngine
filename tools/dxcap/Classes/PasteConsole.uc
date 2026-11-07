//=============================================================================
// PasteConsole: the clipboard's text -- 3 s into whatever map the run starts
// on, PlayerPawn.PasteFromClipboard, its length and text logged; then the
// game's own copy of "dxcap copy" and three characters, one past Latin-1
// (CopyToClipboard), pasted back, then an exit. The run's clipboard holds
// "dxcap paste" (wl-copy before the run, or the desktop's own). DXEngine's
// X11 window once asked the selection for no data and waited for the rest for
// ever, hanging the game on a paste (bugs.md E16b.11), and asked its own
// selection for Latin-1. The text's characters are logged by their codes too,
// the original's log being 8-bit. Each line starts "DXPASTE:".
//=============================================================================
class PasteConsole extends Console;

var float RunTime;
var int Step;

// Each character's code, the log being 8-bit
function string Codes(string S)
{
	local string R;
	local int i;

	for (i = 0; i < Len(S); i++)
		R = R $ " " $ Asc(Mid(S, i, 1));
	return R;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string T;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (Step == 0 && RunTime > 3.0)
	{
		Log("DXPASTE: asking the clipboard");
		T = P.PasteFromClipboard();
		Log("DXPASTE: " $ Len(T) $ " character(s): '" $ T $ "', codes" $ Codes(T));
		Step = 1;
	}
	else if (Step == 1 && RunTime > 3.5)
	{
		T = "dxcap copy " $ Chr(233) $ " " $ Chr(937) $ " " $ Chr(8364);
		P.CopyToClipboard(T);
		Log("DXPASTE: copied " $ Len(T) $ " character(s), codes" $ Codes(T));
		Step = 2;
	}
	else if (Step == 2 && RunTime > 4.0)
	{
		T = P.PasteFromClipboard();
		Log("DXPASTE: own copy back, " $ Len(T) $ " character(s), codes" $ Codes(T));
		Step = 3;
	}
	else if (Step == 3 && RunTime > 5.0)
	{
		Log("DXPASTE: exiting");
		P.ConsoleCommand("exit");
		Step = 4;
	}
}
