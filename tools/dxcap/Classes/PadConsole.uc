//=============================================================================
// PadConsole: what a pad does in play, beside a timeline that drives the
// window's virtual pad (timelines/pad.txt): every half second of the map the
// player's place, its speed, its facing and its physics, logged with
// "DXPAD:"; each key the game gets is the timeline's own log (DXKEY:)
// (docs/DEVELOPMENT.md, timelines).
//=============================================================================
class PadConsole extends Console;

var float RunTime, LogTime;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (RunTime - LogTime < 0.5)
		return;
	LogTime = RunTime;
	Log("DXPAD: t=" $ int(RunTime * 2) / 2.0 $ " at " $ P.Location $ " speed " $ int(VSize(P.Velocity)) $ " yaw " $ (P.Rotation.Yaw & 65535) $ " physics " $ P.Physics);
}
