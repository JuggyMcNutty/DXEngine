//=============================================================================
// FatalPrefsConsole (bugs.md ENG.43): a fatal error while Advanced Options is
// open. 3 s in PREFERENCES; 6 s in a loop that never ends, which the VM
// stops at its ten millionth jump with a critical error. Run full screen
// (DXCAP_FULLSCREEN=1) under the sanitizer build: the engine must end with
// its "DXEngine error:" line and no report. Logged with "DXFATAL:".
//=============================================================================
class FatalPrefsConsole extends Console;

var float RunTime;
var int Phase;

event Tick(float Delta)
{
	local int i;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 3.0)
	{
		Log("DXFATAL: PREFERENCES");
		Viewport.Actor.ConsoleCommand("PREFERENCES");
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 6.0)
	{
		Log("DXFATAL: the endless loop");
		Phase = 2;
		while (true)
			i++;
	}
}

defaultproperties
{
}
