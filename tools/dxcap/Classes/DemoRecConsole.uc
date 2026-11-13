//=============================================================================
// DemoRecConsole: a demo recorded on Liberty Island for the other engine to
// play back (bugs.md WIN.30), logged with "DXDEMOREC:". On the island
// (DXEngine's map, the original's DXCAP_START), 2 s in, DEMOREC DemoName
// (DxCapRec: DxCapRec.dem in the run's System folder) by the player's
// ConsoleCommand, its answer logged; then for Seconds (8) the player turned
// 45 degrees a second and moved 40 units a second along his view, Beep4
// played 1 s and 5 s in, a ClientMessage 3 s in, and each second his place
// and view logged; then STOPDEMO, its answer, and an exit. Each engine's
// run names its own file: DXCAP_CONFIG="DemoName=DxCapRecDxe", or
// "DemoName=DxCapRecOrig". Play one back with DemoPlayConsole.
//=============================================================================
class DemoRecConsole extends Console;

var config string DemoName;
var config float Seconds;

var float RunTime, RecordTime, NextLog;
var int Stage;
var bool bBeep1, bMessage, bBeep2;

function string DemoFile()
{
	if (DemoName == "")
		return "DxCapRec";
	return DemoName;
}

function float RecordSeconds()
{
	if (Seconds <= 0)
		return 8.0;
	return Seconds;
}

function LogPlace(PlayerPawn P, string When)
{
	Log("DXDEMOREC: " $ When $ ": at " $ int(P.Location.X) $ "," $ int(P.Location.Y) $ "," $ int(P.Location.Z) $
		" view " $ P.ViewRotation.Pitch $ "," $ P.ViewRotation.Yaw $ " net mode " $ P.Level.NetMode);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local rotator R;
	local vector Dir;
	local float Since;
	local string Answer;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || Stage >= 2)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (Stage == 0)
	{
		if (RunTime < 2.0)
			return;
		Answer = P.ConsoleCommand("DEMOREC " $ DemoFile());
		Log("DXDEMOREC: 'DEMOREC " $ DemoFile() $ "' gave '" $ Answer $ "'");
		LogPlace(P, "start");
		RecordTime = RunTime;
		NextLog = 1.0;
		Stage = 1;
		return;
	}

	Since = RunTime - RecordTime;
	R = P.ViewRotation;
	R.Yaw = R.Yaw + int(8192.0 * Delta);
	P.ViewRotation = R;
	Dir = vector(R);
	Dir.Z = 0;
	P.SetLocation(P.Location + Normal(Dir) * 40.0 * Delta);

	if (!bBeep1 && Since >= 1.0)
	{
		bBeep1 = true;
		P.PlaySound(Sound'DeusExSounds.Generic.Beep4', SLOT_Misc, 1.0, false, 65536.0);
		Log("DXDEMOREC: beep 1");
	}
	if (!bMessage && Since >= 3.0)
	{
		bMessage = true;
		P.ClientMessage("DXDEMOREC message");
		Log("DXDEMOREC: message");
	}
	if (!bBeep2 && Since >= 5.0)
	{
		bBeep2 = true;
		P.PlaySound(Sound'DeusExSounds.Generic.Beep4', SLOT_Misc, 1.0, false, 65536.0);
		Log("DXDEMOREC: beep 2");
	}
	if (Since >= NextLog)
	{
		LogPlace(P, int(NextLog) $ " s");
		NextLog = NextLog + 1.0;
	}
	if (Since >= RecordSeconds())
	{
		Answer = P.ConsoleCommand("STOPDEMO");
		Log("DXDEMOREC: 'STOPDEMO' gave '" $ Answer $ "'; done, exiting");
		Stage = 2;
		P.ConsoleCommand("exit");
	}
}
