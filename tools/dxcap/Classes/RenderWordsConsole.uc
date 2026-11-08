//=============================================================================
// RenderWordsConsole: the render device's console words, BRIGHTNESS and the
// PRECACHING frame after a FLUSH (bugs.md REN.43, REN.44), logged with
// "DXRWORDS:". On the map the run starts on, 3 s in: GETDETAILTEXTURES,
// TOGGLEDETAILTEXTURES, GETDETAILTEXTURES and TOGGLEDETAILTEXTURES again,
// SHOWPOOLS, each one's output logged, then shot 0; at 4 s LODBIAS 2, at 5 s
// shot 1 (the mips a step coarser); at 6 s and 8 s TOGGLEFULLSCREEN, full
// screen and back, at 10 s shot 2 (the bias the device's own again, as the
// original's SetRes sets it); at 11 s BRIGHTNESS twice, the client's
// Brightness after each and the console's messages logged; at 12 s FLUSH,
// the level's action logged at each of the five ticks after it (Precaching
// none in either engine: the original's FLUSH precaches nothing, the map's
// load alone does); at 13 s an exit. The original with
// DXCAP_RENDERER=D3D, its shots marked for the grabber. Compared: the
// lines, and shots.py over shots 0 to 2 (docs/DEVELOPMENT.md, scripted runs).
// The level's action at the map's first five ticks is logged too, the
// control of the flush's.
//=============================================================================
class RenderWordsConsole extends Console;

var float RunTime;
var int Phase;
var int FlushTicks;
var int StartTicks;

// The mark for the original's screen grabber, as CaptureConsole's: shown
// for the 0.6 s before each shot, the shot's number and one in it (the
// grabber names its shots by their marks, 1 to 3).
var int MarkShot;
var bool bMarking;

event PostRender(canvas C)
{
	local int i;
	Super.PostRender(C);
	if (!bMarking)
		return;
	C.Style = 1;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		if ((MarkShot & (1 << i)) != 0)
		{
			C.DrawColor.R = 255;
			C.DrawColor.G = 255;
			C.DrawColor.B = 255;
		}
		else
		{
			C.DrawColor.R = 0;
			C.DrawColor.G = 0;
			C.DrawColor.B = 0;
		}
		C.SetPos(12 + 12 * i, 0);
		C.DrawRect(Texture'Solid', 12, 12);
	}
}

function Shot(PlayerPawn P, int N)
{
	P.ConsoleCommand("shot");
	bMarking = false;
	Log("DXRWORDS: shot " $ N);
}

function Ask(PlayerPawn P, string Command)
{
	Log("DXRWORDS: " $ Command $ " gives |" $ P.ConsoleCommand(Command) $ "|");
}

function LogBrightness(PlayerPawn P, string When)
{
	Log("DXRWORDS: brightness " $ When $ " " $ P.ConsoleCommand("get ini:Engine.Engine.ViewportManager Brightness"));
}

event Message(PlayerReplicationInfo PRI, coerce string Msg, name N)
{
	Super.Message(PRI, Msg, N);
	Log("DXRWORDS: message |" $ Msg $ "| type " $ N);
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	// The control: the level's action at the first five ticks of the map,
	// Precaching in the first after its load with DXCAP_PRECACHE=1
	if (StartTicks < 5)
	{
		StartTicks++;
		Log("DXRWORDS: tick " $ StartTicks $ " of the map: level action " $ P.Level.LevelAction);
	}

	if (FlushTicks > 0)
	{
		Log("DXRWORDS: tick " $ (6 - FlushTicks) $ " after the flush: level action " $ P.Level.LevelAction);
		FlushTicks--;
	}

	if (Phase == 0 && RunTime > 2.4)
	{
		MarkShot = 1;
		bMarking = true;
		Phase = 100;
	}
	else if (Phase == 100 && RunTime > 3.0)
	{
		Ask(P, "getdetailtextures");
		Ask(P, "toggledetailtextures");
		Ask(P, "getdetailtextures");
		Ask(P, "toggledetailtextures");
		Ask(P, "showpools");
		Shot(P, 0);
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 4.0)
	{
		Ask(P, "lodbias 2");
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 4.4)
	{
		MarkShot = 2;
		bMarking = true;
		Phase = 102;
	}
	else if (Phase == 102 && RunTime > 5.0)
	{
		Shot(P, 1);
		Phase = 3;
	}
	else if (Phase == 3 && RunTime > 6.0)
	{
		P.ConsoleCommand("togglefullscreen");
		Phase = 4;
	}
	else if (Phase == 4 && RunTime > 8.0)
	{
		P.ConsoleCommand("togglefullscreen");
		Phase = 5;
	}
	else if (Phase == 5 && RunTime > 9.4)
	{
		MarkShot = 3;
		bMarking = true;
		Phase = 105;
	}
	else if (Phase == 105 && RunTime > 10.0)
	{
		Shot(P, 2);
		Phase = 6;
	}
	else if (Phase == 6 && RunTime > 11.0)
	{
		LogBrightness(P, "before");
		Ask(P, "brightness");
		LogBrightness(P, "after one");
		Ask(P, "brightness");
		LogBrightness(P, "after two");
		Phase = 7;
	}
	else if (Phase == 7 && RunTime > 12.0)
	{
		Log("DXRWORDS: flush, level action " $ P.Level.LevelAction);
		P.ConsoleCommand("flush");
		FlushTicks = 5;
		Phase = 8;
	}
	else if (Phase == 8 && RunTime > 13.0)
	{
		Log("DXRWORDS: exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
