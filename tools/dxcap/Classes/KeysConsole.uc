//=============================================================================
// KeysConsole: the game's view of a timeline's held shift. The engine's
// Shift+~ (Engine::Key) asks the window's key state and puts the console
// into its Typing state; the root window's IsKeyDown marks the presses it
// sees. Every quarter second the console's state and the root's marks of
// Shift, Ctrl and Alt, logged with "DXKEYS:", beside a timeline that holds
// Left Shift while it types ~ (timelines/held-keys.txt): the state must turn
// Typing, as under a held key of the keyboard (docs/DEVELOPMENT.md,
// timelines).
//=============================================================================
class KeysConsole extends Console;

var float RunTime, LogTime;

event Tick(float Delta)
{
	local DeusExPlayer P;

	Super.Tick(Delta);
	if (Viewport == None)
		return;
	P = DeusExPlayer(Viewport.Actor);
	if (P == None || P.rootWindow == None)
		return;
	RunTime += Delta;
	if (RunTime - LogTime < 0.25)
		return;
	LogTime = RunTime;
	Log("DXKEYS: t=" $ int(RunTime * 4) / 4.0 $ " state=" $ GetStateName() $ " shift=" $ P.rootWindow.IsKeyDown(IK_Shift) $ " ctrl=" $ P.rootWindow.IsKeyDown(IK_Ctrl) $ " alt=" $ P.rootWindow.IsKeyDown(IK_Alt));
}
