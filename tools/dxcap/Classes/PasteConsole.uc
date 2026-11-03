//=============================================================================
// PasteConsole: the clipboard's text -- 3 s into whatever map the run starts
// on, PlayerPawn.PasteFromClipboard, its length and text logged, then an
// exit. The run's clipboard holds "dxcap paste" (wl-copy before the run, or
// the desktop's own). DXEngine's X11 window once asked the selection for no
// data and waited for the rest for ever, hanging the game on a paste
// (bugs.md E16b.11). The text's characters are logged by their codes too, the
// log being 8-bit. Each line starts "DXPASTE:".
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
	else if (Step == 1 && RunTime > 4.0)
	{
		Log("DXPASTE: exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
}
