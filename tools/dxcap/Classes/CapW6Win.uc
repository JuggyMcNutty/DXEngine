//=============================================================================
// CapW6Win: a window for TreeChangeConsole (bugs.md EXT.44). Each logs what is
// done to it with "DXWIN6:" in front. Mode says what it does to the tree
// from inside an event, once:
//   0 nothing;
//   1 destroys itself in its first DrawWindow;
//   2 destroys itself in its first ConfigurationChanged;
//   3 destroys its parent in its first script Tick;
//   4 raises itself in its first DrawWindow.
// Draws a full tile of its colour, so a shot shows what a draw skipped or
// what a clip left behind cut away.
//=============================================================================
class CapW6Win extends Window;

var string Label;
var int Mode;
var int Draws;
var int Configs;
var int Ticks;
var bool bDone;

function Setup(string NewLabel, int NewMode, Color C)
{
	Label = NewLabel;
	Mode = NewMode;
	SetBackground(Texture'Solid');
	SetBackgroundStyle(DSTY_Normal);
	SetTileColor(C);
	if (Mode == 3)
		bTickEnabled = True;
}

event DrawWindow(GC gc)
{
	Draws++;
	if (Draws <= 4)
		Log("DXWIN6: draw " $ Label $ " #" $ Draws);
	if (!bDone && Mode == 1)
	{
		bDone = True;
		Log("DXWIN6: " $ Label $ " destroys itself in DrawWindow");
		Destroy();
	}
	else if (!bDone && Mode == 4)
	{
		bDone = True;
		Log("DXWIN6: " $ Label $ " raises itself in DrawWindow");
		Raise();
	}
}

event ConfigurationChanged()
{
	Configs++;
	Log("DXWIN6: configured " $ Label $ " #" $ Configs $ " at " $ int(width) $ "x" $ int(height));
	if (!bDone && Mode == 2)
	{
		bDone = True;
		Log("DXWIN6: " $ Label $ " destroys itself in ConfigurationChanged");
		Destroy();
	}
}

function Tick(float deltaSeconds)
{
	Ticks++;
	if (Ticks <= 3)
		Log("DXWIN6: tick " $ Label $ " #" $ Ticks);
	if (!bDone && Mode == 3)
	{
		bDone = True;
		Log("DXWIN6: " $ Label $ " destroys its parent in Tick");
		GetParent().Destroy();
	}
}

function W6Timer(int timerID, int invocations, int clientData)
{
	Log("DXWIN6: timer " $ Label $ " data " $ clientData $ " invocations " $ invocations);
}
