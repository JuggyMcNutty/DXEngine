//=============================================================================
// DemoPlayConsole: a demo played back (bugs.md WIN.30), logged with
// "DXDEMOPLAY:". From wherever the run starts (the original's menu map,
// DXEngine's DX.dx), 2 s in, DEMOPLAY DemoName (DxCapPlay: DxCapPlay.dem in
// the run's System folder, a DemoRecConsole run's file of the other engine
// copied there; given with no ".dem", which both engines' URLs would read
// as a host) by the player's ConsoleCommand, its answer logged; then every
// half second the level's URL, net mode and level action, and the viewport
// player's class, roles, place and view: the playback's map loaded as a
// client's, the recorded player taken by the viewport (an autonomous
// proxy), his view the recording's DemoViewPitch and DemoViewYaw, and at
// the demo's end the Entry level. Ends there once the demo's level has been
// seen (the original's connects in the Entry level first), or after
// Seconds (40); then an exit.
//=============================================================================
class DemoPlayConsole extends Console;

var config string DemoName;
var config float Seconds;

var float RunTime, NextLog;
var int Stage;
var bool bLeftStart;
var string StartURL;

function string DemoFile()
{
	if (DemoName == "")
		return "DxCapPlay";
	return DemoName;
}

function float PlaySeconds()
{
	if (Seconds <= 0)
		return 40.0;
	return Seconds;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string Answer, URL;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || Stage >= 2)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (Stage == 0)
	{
		if (RunTime < 2.0)
			return;
		StartURL = P.Level.GetLocalURL();
		Answer = P.ConsoleCommand("DEMOPLAY " $ DemoFile());
		Log("DXDEMOPLAY: 'DEMOPLAY " $ DemoFile() $ "' gave '" $ Answer $ "' from " $ StartURL);
		NextLog = RunTime;
		Stage = 1;
		return;
	}

	if (RunTime < NextLog)
		return;
	NextLog = RunTime + 0.5;
	URL = P.Level.GetLocalURL();
	Log("DXDEMOPLAY: " $ int(RunTime * 10) / 10.0 $ " s: " $ URL $ " net mode " $ P.Level.NetMode $ " action " $ P.Level.LevelAction $
		" player " $ P $ " role " $ P.Role $ "/" $ P.RemoteRole $
		" at " $ int(P.Location.X) $ "," $ int(P.Location.Y) $ "," $ int(P.Location.Z) $
		" view " $ P.ViewRotation.Pitch $ "," $ P.ViewRotation.Yaw);
	// Begun once the level is the demo's: the original's connects in the
	// Entry level first (its action LEVACT_Connecting), DXEngine's goes
	// straight to the demo's map
	if (InStr(Caps(URL), ".DEM") >= 0)
		bLeftStart = true;
	if ((bLeftStart && InStr(Caps(URL), "ENTRY") >= 0) || RunTime > PlaySeconds())
	{
		Log("DXDEMOPLAY: done, exiting");
		Stage = 2;
		P.ConsoleCommand("exit");
	}
}
