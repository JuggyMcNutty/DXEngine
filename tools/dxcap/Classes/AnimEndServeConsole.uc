//=============================================================================
// AnimEndServeConsole: the serving side of the AnimEnd net test
// (AnimEndJoinConsole; bugs.md NET.15) -- a listen server on DXMP_Cathedral
// (CapDeathMatch: without the stock console check), started from the menu
// map as ServeConsole starts one. 3 s after another player is in, a
// CapAnimPawn 160 units before it, always relevant, so a simulated proxy
// pawn on the client; then, from this console's tick, its animations, a
// stage each, each call logged with "DXANIMEND:" as it is made: stage 1,
// three PlayAnims of Blowing 2 s apart, each ending a second later; stage
// 2, Blowing looped 5.5 s, five loops, then stopped by a TweenAnim of Still
// with no time, which ends nothing; stage 3, TweenAnims of Still over 0.5,
// 0.4 and 0.3 s 2 s apart, each ending with no rate to go on at; stage 4,
// done. The pawn logs its own AnimEnds, stops and wraps on each side, here
// the server's, and what each stage saw. Exits 8 s after stage 4, so the
// client leaves first, or after 140 s.
//=============================================================================
class AnimEndServeConsole extends Console;

var float CapTime, InTime, PlacedTime;
var int Phase, Step;
var CapAnimPawn Shown;

// Each call once its time from the placing has come, in order
function Play(float T)
{
	local string What;

	if (Shown == None || Shown.bDeleteMe)
	{
		Log("DXANIMEND: server lost the pawn at step " $ Step $ "; exiting");
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 9;
		return;
	}
	switch (Step)
	{
	case 0:
		if (T < 2.0)
			return;
		Shown.Stage = 1;
		Shown.PlayAnim('Blowing');
		What = "stage 1, PlayAnim Blowing";
		break;
	case 1:
		if (T < 4.0)
			return;
		Shown.PlayAnim('Blowing');
		What = "PlayAnim Blowing";
		break;
	case 2:
		if (T < 6.0)
			return;
		Shown.PlayAnim('Blowing');
		What = "PlayAnim Blowing";
		break;
	case 3:
		if (T < 8.5)
			return;
		Shown.Stage = 2;
		Shown.LoopAnim('Blowing');
		What = "stage 2, LoopAnim Blowing";
		break;
	case 4:
		if (T < 14.0)
			return;
		Shown.TweenAnim('Still', 0.0);
		What = "TweenAnim Still, no time";
		break;
	case 5:
		if (T < 15.0)
			return;
		Shown.Stage = 3;
		Shown.TweenAnim('Still', 0.5);
		What = "stage 3, TweenAnim Still over 0.5";
		break;
	case 6:
		if (T < 17.0)
			return;
		Shown.TweenAnim('Still', 0.4);
		What = "TweenAnim Still over 0.4";
		break;
	case 7:
		if (T < 19.0)
			return;
		Shown.TweenAnim('Still', 0.3);
		What = "TweenAnim Still over 0.3";
		break;
	case 8:
		if (T < 21.0)
			return;
		Shown.Stage = 4;
		What = "stage 4, done";
		break;
	default:
		return;
	}
	Log("DXANIMEND: server step " $ Step $ " " $ T $ " s after placing: " $ What $ "; sequence " $ Shown.AnimSequence $ " frame " $ Shown.AnimFrame $ " rate " $ Shown.AnimRate $ " tween " $ Shown.TweenRate $ " last " $ Shown.AnimLast $ " loop " $ Shown.bAnimLoop $ " sim " $ Shown.SimAnim.X $ "," $ Shown.SimAnim.Y $ "," $ Shown.SimAnim.Z $ "," $ Shown.SimAnim.W);
	Step++;
}

event Tick(float Delta)
{
	local PlayerPawn P, Other, Joined;
	local rotator R;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	CapTime += Delta;

	if (Phase == 0 && CapTime > 3.0)
	{
		Log("DXANIMEND: starting the listen server");
		P.ConsoleCommand("start DXMP_Cathedral?game=DXCapture.CapDeathMatch?listen");
		Phase = 1;
	}
	else if (Phase == 1 && P.Level.NetMode != NM_Standalone)
	{
		Log("DXANIMEND: serving " $ P.Level.GetLocalURL() $ ", net mode " $ P.Level.NetMode);
		Phase = 2;
	}
	else if (Phase == 2)
	{
		foreach P.AllActors(class'PlayerPawn', Other)
			if (Other != P)
				Joined = Other;
		if (Joined != None)
			InTime += Delta;
		if (Joined != None && InTime > 3.0)
		{
			R = Joined.Rotation;
			R.Pitch = 0;
			R.Roll = 0;
			Shown = P.Spawn(class'CapAnimPawn',,, Joined.Location + 160 * vector(R) + vect(0, 0, 20), R);
			if (Shown == None)
			{
				Log("DXANIMEND: server placed no pawn; exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			Log("DXANIMEND: server placed " $ Shown.Name $ " at " $ Shown.Location $ " before " $ Joined.PlayerReplicationInfo.PlayerName $ " at " $ Joined.Location);
			PlacedTime = CapTime;
			Phase = 3;
		}
	}
	else if (Phase == 3)
	{
		Play(CapTime - PlacedTime);
		if (Phase == 3 && Step > 8 && CapTime - PlacedTime >= 29.0)
		{
			Log("DXANIMEND: server done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}

	if (Phase >= 1 && Phase < 9 && CapTime > 140.0)
	{
		Log("DXANIMEND: server out of time, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
