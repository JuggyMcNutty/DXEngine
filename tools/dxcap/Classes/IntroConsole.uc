//=============================================================================
// IntroConsole: the menu map's intro as it plays, shot in both engines
// (docs/DEVELOPMENT.md, scripted runs). On the menu map, no window touched:
// from First s in (3 by default), Shots shots (12 by default), one every
// Every s (1 by default) of the console's clock, each marked for the
// original's grabber. First the map's InterpolationPoints are logged (each
// one's tag, position, place, rate and game speed modifiers, flags and
// neighbours); with each shot, the view's place and facing, the player's
// physics, path target, PhysRate and PhysAlpha, the level's time and
// TimeDilation, and the root's top window, all with "DXINTRO:" in front.
// From FrameFrom s to FrameTo s (none by default), every tick is logged:
// the console's clock and the tick's time, the level's time, the clock on
// the wall (the level's Second and Millisecond), the path's target and alpha.
//=============================================================================
class IntroConsole extends Console;

var config float First;
var config float Every;
var config int Shots;
var config float FrameFrom;
var config float FrameTo;

var float RunTime;
var float NextShot;
var int Taken;
var int Phase;
var int MarkShot;
var bool bMarking;

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;
	local InterpolationPoint I;
	local string Top;

	Super.Tick(Delta);
	if (Phase == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (RunTime >= FrameFrom && RunTime < FrameTo)
		Log("DXINTRO: tick at " $ RunTime $ " of " $ Delta $ ", the level's time " $ P.Level.TimeSeconds
			$ ", the wall's " $ P.Level.Second $ "." $ P.Level.Millisecond $ ", target " $ P.Target $ ", alpha " $ P.PhysAlpha);
	if (Phase == 0)
	{
		if (First == 0)
			First = 3;
		if (Every == 0)
			Every = 1;
		if (Shots == 0)
			Shots = 12;
		NextShot = First;
		foreach P.AllActors(class'InterpolationPoint', I)
			Log("DXINTRO: point " $ I.Name $ " tag " $ I.Tag $ " position " $ I.Position $ " at " $ I.Location $ " facing " $ I.Rotation
				$ ", rate " $ I.RateModifier $ ", game speed " $ I.GameSpeedModifier $ ", fov " $ I.FovModifier
				$ ", end " $ I.bEndOfPath $ ", skip " $ I.bSkipNextPath $ ", prev " $ I.Prev $ ", next " $ I.Next);
		Phase = 1;
	}
	if (Phase == 1 && RunTime > NextShot - 0.3)
	{
		MarkShot = Taken;
		bMarking = true;
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > NextShot)
	{
		Top = "none";
		if (DeusExPlayer(P) != None)
		{
			Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
			if (Root != None && Root.GetTopWindow() != None)
				Top = string(Root.GetTopWindow().Class.Name);
		}
		Log("DXINTRO: shot " $ Taken $ " at " $ RunTime $ ": the view at " $ P.Location $ " facing " $ P.ViewRotation
			$ ", physics " $ P.Physics $ ", target " $ P.Target $ ", rate " $ P.PhysRate $ ", alpha " $ P.PhysAlpha
			$ ", the level's time " $ P.Level.TimeSeconds $ " dilated " $ P.Level.TimeDilation $ ", the top window " $ Top);
		Log("DXCAP: shot " $ Taken);
		P.ConsoleCommand("shot");
		bMarking = false;
		Taken++;
		NextShot += Every;
		if (Taken < Shots)
			Phase = 1;
		else
			Phase = 3;
	}
	else if (Phase == 3 && RunTime > NextShot)
	{
		Log("DXINTRO: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
	}
}

// The mark for the original's screen grabber, as CaptureConsole's: a
// magenta block, then the shot's number in eight blocks
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
