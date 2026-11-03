//=============================================================================
// BlendFieldsConsole: the blend slots' fields as
// PlayBlendAnim and TweenBlendAnim set them, and as the tick moves them, in
// both engines (Engine.dll execPlayBlendAnim 0x103e0d80, execTweenBlendAnim
// 0x103e1300, AActor::Tick's slot loop from 0x103a1aa0's LABEL_76).
//
// On Liberty Island, 1 s in, the first PaulDenton (GM_Trench: HeadLeft,
// HeadRight, Still and MouthA are one-frame sequences, Blink two):
//   a. slot 3 set to Still by TweenBlendAnim, then PlayBlendAnim('HeadLeft',
//      1.0, 1.0, 3), a one-frame sequence: the original's frame -1 and tween
//      rate 1, the head turning over 1 s; DXEngine's frame 0, at once;
//   b. PlayBlendAnim('Blink', 1.0, 0.1, 2) on slot 2, never used (None): the
//      original takes no tween (rate 0, frame 0.001), DXEngine tweens 0.1 s;
//   c. PlayBlendAnim('Blink', 1.0, -1, 2) again: the original's frame -0.5,
//      its tween at OldBlendAnimRate; DXEngine's frame 0;
//   d. TweenBlendAnim('MouthA', 0.1, 0): SimBlendAnim[0] packed in the
//      original (frame, rate, tween, last as X, Y, Z, W), not in DXEngine;
//   then every tick for 1.5 s slot 3's and slot 2's frame, with the main
//   animation's sequence, frame and rate, so the slots' per-tick movement
//   (passes and time shared with the main animation in the original, three
//   whole-tick passes in DXEngine) can be lined up.
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
			Phase = 3;
			return;
		}
		Paul.LoopAnim('BreatheLight', 1.0);
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
		Log("DXBLEND: tick " $ Delta $ " main " $ Paul.AnimSequence $ " " $ Paul.AnimFrame $ " " $ Paul.AnimRate
			$ " s3 " $ Paul.BlendAnimFrame[3] $ " s2 " $ Paul.BlendAnimFrame[2]);
		if (WatchTime > 1.5)
		{
			Log("DXCAP: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 3;
		}
	}
}
