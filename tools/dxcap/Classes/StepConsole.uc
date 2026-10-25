//=============================================================================
// StepConsole: an animated texture's steps as it is drawn every frame, every
// other frame and every ninth, logged with "DXSTEP:" (docs/DEVELOPMENT.md,
// scripted runs); both engines at DXCAP_FPS=60.
//
// From wherever the run starts, 2 s in: CoreTexMisc.Glass.WhiteNoise_A01, a
// chain of frames stepped at a MaxFrameRate of 10, drawn on the canvas for
// 120 frames at each cadence, its AnimCurrent read after each draw; the
// draws, the steps and the level's time over each cadence logged; and an
// exit. What is compared between the engines: the steps at each cadence --
// a texture steps by the time since its own last update, however often it
// is drawn.
//=============================================================================
class StepConsole extends Console;

var float RunTime, PhaseStart;
var int Phase, Frame, Steps, Drawn;
var int Cadence[3];
var texture T, Last;
var bool bDone, bStarted;

event PostRender(canvas C)
{
	local int every;

	Super.PostRender(C);
	if (bDone || Viewport == None || Viewport.Actor == None || RunTime < 2.0)
		return;
	if (!bStarted)
	{
		T = Texture(DynamicLoadObject("CoreTexMisc.Glass.WhiteNoise_A01", class'Texture'));
		if (T == None)
		{
			Log("DXSTEP: no texture, exiting");
			Viewport.Actor.ConsoleCommand("exit");
			bDone = true;
			return;
		}
		Log("DXSTEP: " $ T $ ": MaxFrameRate " $ T.MaxFrameRate $ ", MinFrameRate " $ T.MinFrameRate $ ", next " $ T.AnimNext);
		PhaseStart = Viewport.Actor.Level.TimeSeconds;
		bStarted = True;
	}
	every = Cadence[Phase];
	if (Frame % every == 0)
	{
		C.SetPos(100, 100);
		C.DrawTile(T, 128, 128, 0, 0, T.USize, T.VSize);
		Drawn++;
		if (T.AnimCurrent != Last)
		{
			Steps++;
			Last = T.AnimCurrent;
		}
	}
	Frame++;
	if (Frame >= 120)
	{
		Log("DXSTEP: drawn every " $ every $ " frames: " $ Drawn $ " draws, " $ Steps $ " steps in " $ Frame $ " frames, " $ (Viewport.Actor.Level.TimeSeconds - PhaseStart) $ " s");
		Frame = 0;
		Drawn = 0;
		Steps = 0;
		Phase++;
		PhaseStart = Viewport.Actor.Level.TimeSeconds;
		if (Phase == 3)
		{
			Log("DXSTEP: done, exiting");
			Viewport.Actor.ConsoleCommand("exit");
			bDone = true;
		}
	}
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	RunTime += Delta;
}

defaultproperties
{
	Cadence(0)=1
	Cadence(1)=2
	Cadence(2)=9
}
