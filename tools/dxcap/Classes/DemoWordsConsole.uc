//=============================================================================
// DemoWordsConsole: a demo recorded from single player by the console's
// words (bugs.md WIN.30), logged with "DXDEMO:". DXEngine has no
// demo driver, recording or playback at all; the original's level takes
// DEMOREC and DEMOPLAY (ULevel::Exec, Engine.dll 0x1039dfe0), and its demo
// driver, while there is one, DEMOREC, DEMOPLAY and STOPDEMO
// (UDemoRecDriver::Exec, 0x104083c0). On Liberty Island (DXEngine's map,
// the original's DXCAP_START), by the player's ConsoleCommand, 2 s apart:
//   1. DEMOREC              "You must specify a filename"
//   2. DEMOREC DxCapDemo    "Demo recording started to DxCapDemo.dem"
//                           (the log first "Attempting to record demo
//                           DxCapDemo.dem"; the driver Engine.DemoRecDriver,
//                           [Engine.Engine] DemoRecordingDevice; standalone,
//                           a CLIENTDEMO with an Engine.DemoRecSpectator,
//                           InitListen 0x10407870); or "Demo recording
//                           failed: <why>"
//   3. DEMOREC DxCapOther   "Demo recording currently active: DxCapDemo.dem"
//   4. DEMOPLAY DxCapOther  the same: the demo driver takes DEMOPLAY first
//   5. STOPDEMO             "Demo DxCapDemo.dem stopped (<n> frames)"
//   6. STOPDEMO             '' (no demo driver: no taker)
//   7. DEMOPLAY             "You must specify a filename"
// the level's NetMode logged at each step; then an
// exit. DXEngine fe46ad13, by reading: '' for each, "Unknown command" in its
// log. The original writes System/DxCapDemo.dem: delete it after the run.
//=============================================================================
class DemoWordsConsole extends Console;

var float RunTime;
var int Step;
var string Words[8];

event Tick(float Delta)
{
	local PlayerPawn P;
	local string R;

	Super.Tick(Delta);
	if (Step >= ArrayCount(Words) || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (RunTime < 2.0 + 2.0 * Step)
		return;
	if (Words[Step] == "")
	{
		Log("DXDEMO: done, exiting");
		Step = ArrayCount(Words);
		P.ConsoleCommand("exit");
		return;
	}
	R = P.ConsoleCommand(Words[Step]);
	Log("DXDEMO: '" $ Words[Step] $ "' gave '" $ R $ "'; net mode " $ P.Level.NetMode);
	Step++;
}

defaultproperties
{
	Words(0)="DEMOREC"
	Words(1)="DEMOREC DxCapDemo"
	Words(2)="DEMOREC DxCapOther"
	Words(3)="DEMOPLAY DxCapOther"
	Words(4)="STOPDEMO"
	Words(5)="STOPDEMO"
	Words(6)="DEMOPLAY"
}
