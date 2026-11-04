//=============================================================================
// BlendFieldsConsole: the blend slots' fields as
// PlayBlendAnim and TweenBlendAnim set them, and as the tick moves them, in
// both engines (Engine.dll execPlayBlendAnim 0x103e0d80, execTweenBlendAnim
// 0x103e1300, AActor::Tick's slot passes from 0x103a1fa1).
//
// On Liberty Island, 1 s in, the first PaulDenton (GM_Trench: HeadLeft,
// HeadRight, Still and MouthA are one-frame sequences, Blink two), his
// BreatheLight loop (0.5 a second, AnimLast 0.8) set at frame 0.3 so the
// main animation is at one place in both engines:
//   a. slot 3 set to Still by TweenBlendAnim, then PlayBlendAnim('HeadLeft',
//      1.0, 1.0, 3), a one-frame sequence: frame -1 and tween rate 1, the
//      head turning over 1 s;
//   b. PlayBlendAnim('Blink', 1.0, 0.1, 2) on slot 2, never used (None): no
//      tween taken (rate 0, frame 0.001);
//   c. PlayBlendAnim('Blink', 1.0, -1, 2) again: frame -0.5, its tween at
//      OldBlendAnimRate;
//   d. TweenBlendAnim('MouthA', 0.1, 0): SimBlendAnim[0] packed (frame,
//      rate, tween, last as X, Y, Z, W);
//   then every tick for 1.5 s the level's time, the main animation's
//   sequence, frame, rate and last frame, and slots 0, 2 and 3's frames:
//   three passes of the tick's time for the slots, less where slot 0 or 2
//   ends its tween or reaches its last frame, leaving the slots after it
//   the time past that;
//   e. the main animation set 0.1 short of AnimLast and
//      PlayBlendAnim('HeadRight', 1.0, 3.0, 2), a one-frame sequence on a
//      slot that has one, tweened at 1/3, then every tick for 2 s as
//      before: the slot moves by the tick's time (three passes), none on
//      the ticks the loop spends from AnimLast to 1.0 (its steps leave no
//      time), two passes of the time past 1.0 on the wrap's, and is at 0
//      some 1.4 s in.
// Lines "DXBLEND:".
//=============================================================================
class BlendFieldsConsole extends Console;

var float MapTime, WatchTime;
var int Phase;
var Actor Paul;

function string Slot(Actor A, int i)
{
	return "slot " $ i $ " seq " $ A.BlendAnimSequence[i] $ " frame " $ A.BlendAnimFrame[i]
		$ " rate " $ A.BlendAnimRate[i] $ " tween " $ A.BlendTweenRate[i] $ " last " $ A.BlendAnimLast[i]
		$ " minrate " $ A.BlendAnimMinRate[i] $ " oldrate " $ A.OldBlendAnimRate[i]
		$ " sim " $ A.SimBlendAnim[i].X $ "," $ A.SimBlendAnim[i].Y $ "," $ A.SimBlendAnim[i].Z $ "," $ A.SimBlendAnim[i].W;
}

function LogTick(float Delta)
{
	Log("DXBLEND: tick " $ Delta $ " at " $ Paul.Level.TimeSeconds $ " main " $ Paul.AnimSequence $ " " $ Paul.AnimFrame
		$ " " $ Paul.AnimRate $ " last " $ Paul.AnimLast $ " s0 " $ Paul.BlendAnimFrame[0]
		$ " s2 " $ Paul.BlendAnimFrame[2] $ " s3 " $ Paul.BlendAnimFrame[3]);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local Actor A;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	MapTime += Delta;

	if (Phase == 0)
	{
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") < 0)
		{
			if (MapTime > 3.0)
			{
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				MapTime = 0;
				Phase = 1;
			}
		}
		else
		{
			MapTime = 0;
			Phase = 1;
		}
	}
	else if (Phase == 1 && InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") >= 0 && MapTime > 1.0)
	{
		foreach P.AllActors(class'Actor', A)
			if (Paul == None && A.IsA('PaulDenton'))
				Paul = A;
		if (Paul == None)
		{
			Log("DXBLEND: no PaulDenton");
			P.ConsoleCommand("exit");
			Phase = 4;
			return;
		}
		Paul.LoopAnim('BreatheLight', 1.0);
		Paul.AnimFrame = 0.3;
		Paul.TweenBlendAnim('Still', 0.0, 3);
		Log("DXBLEND: a before " $ Slot(Paul, 3));
		Paul.PlayBlendAnim('HeadLeft', 1.0, 1.0, 3);
		Log("DXBLEND: a after " $ Slot(Paul, 3));
		Log("DXBLEND: b before " $ Slot(Paul, 2));
		Paul.PlayBlendAnim('Blink', 1.0, 0.1, 2);
		Log("DXBLEND: b after " $ Slot(Paul, 2));
		Paul.PlayBlendAnim('Blink', 1.0, -1.0, 2);
		Log("DXBLEND: c after " $ Slot(Paul, 2));
		Paul.TweenBlendAnim('MouthA', 0.1, 0);
		Log("DXBLEND: d after " $ Slot(Paul, 0));
		WatchTime = 0;
		Phase = 2;
	}
	else if (Phase == 2)
	{
		WatchTime += Delta;
		LogTick(Delta);
		if (WatchTime > 1.5)
		{
			Paul.AnimFrame = Paul.AnimLast - 0.1;
			Paul.PlayBlendAnim('HeadRight', 1.0, 3.0, 2);
			Log("DXBLEND: e after " $ Slot(Paul, 2));
			WatchTime = 0;
			Phase = 3;
		}
	}
	else if (Phase == 3)
	{
		WatchTime += Delta;
		LogTick(Delta);
		if (WatchTime > 2.0)
		{
			Log("DXCAP: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 4;
		}
	}
}
