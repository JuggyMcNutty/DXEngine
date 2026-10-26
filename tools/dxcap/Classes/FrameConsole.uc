//=============================================================================
// FrameConsole: a BorderWindow's frame, shot in both engines
// (docs/DEVELOPMENT.md, scripted runs). From wherever the run starts, 2 s
// in: the cheats enabled and the EditFlags cheat screen opened, a
// ToolWindow framed by a BorderWindow of nine textures; a marked shot a
// moment later, then an exit. "DXFRAME:" lines say what was opened. What is
// compared between the engines: the frame's corners and edges.
//=============================================================================
class FrameConsole extends Console;

var float RunTime;
var int Phase;
var int MarkShot;
var bool bMarking;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		P.ConsoleCommand("set DeusEx.JCDentonMale bCheatsEnabled True");
		P.ConsoleCommand("set DeusEx.JCDentonFemale bCheatsEnabled True");
		Log("DXFRAME: opening the EditFlags cheat screen");
		P.ConsoleCommand("editflags");
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.5)
	{
		MarkShot = 1;
		bMarking = true;
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 4.3)
	{
		Log("DXFRAME: shot 1, the EditFlags screen");
		Log("DXCAP: shot 1");
		P.ConsoleCommand("shot");
		bMarking = false;
		Phase = 3;
	}
	else if (Phase == 3 && RunTime > 6.0)
	{
		Log("DXFRAME: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}

// The mark for the original's screen grabber, as BorderConsole's.
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
