//=============================================================================
// UnpackServeConsole: the serving side of the unpacking net test
// (UnpackJoinConsole; bugs.md NET.27) -- a listen server on DXMP_Cathedral
// (CapDeathMatch: without the stock console check), started from the menu
// map as ServeConsole starts one. 3 s after another player is in, a
// CapUnpackPawn 160 units before it, always relevant, so a simulated proxy
// pawn on the client; then, from this console's tick, its SimAnim set a
// stage every 2 s, each logged with "DXUNPACK:": stage 1 (3, 3, 5, -9333)
// and stage 2 (9333, 1000, 22, 10), whose every value a float constant's
// product makes a float step off the original's double one -- the frame,
// rate, tween rate and last frame, the last a loop's at stage 1 --, stage 3
// (10000, 5000, 1000, 6667), alike either way; stage 4, done. A plane goes
// as four 16-bit numbers, so these whole values arrive as they are. Exits
// 8 s after stage 4, so the client leaves first, or after 140 s.
//=============================================================================
class UnpackServeConsole extends Console;

var float CapTime, InTime, PlacedTime;
var int Phase, Step;
var CapUnpackPawn Shown;

// Stage S's SimAnim
function SetSim(byte S, float X, float Y, float Z, float W)
{
	Shown.SimAnim.X = X;
	Shown.SimAnim.Y = Y;
	Shown.SimAnim.Z = Z;
	Shown.SimAnim.W = W;
	Shown.Stage = S;
	Log("DXUNPACK: server stage " $ S $ " SimAnim " $ X $ "," $ Y $ "," $ Z $ "," $ W);
}

// Each stage once its time from the placing has come, in order
function Play(float T)
{
	if (Shown == None || Shown.bDeleteMe)
	{
		Log("DXUNPACK: server lost the pawn at step " $ Step $ "; exiting");
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 9;
		return;
	}
	if (Step == 0 && T >= 2.0)
		SetSim(1, 3.0, 3.0, 5.0, -9333.0);
	else if (Step == 1 && T >= 4.0)
		SetSim(2, 9333.0, 1000.0, 22.0, 10.0);
	else if (Step == 2 && T >= 6.0)
		SetSim(3, 10000.0, 5000.0, 1000.0, 6667.0);
	else if (Step == 3 && T >= 8.0)
	{
		Shown.Stage = 4;
		Log("DXUNPACK: server stage 4, done");
	}
	else
		return;
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
		Log("DXUNPACK: starting the listen server");
		P.ConsoleCommand("start DXMP_Cathedral?game=DXCapture.CapDeathMatch?listen");
		Phase = 1;
	}
	else if (Phase == 1 && P.Level.NetMode != NM_Standalone)
	{
		Log("DXUNPACK: serving " $ P.Level.GetLocalURL() $ ", net mode " $ P.Level.NetMode);
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
			Shown = P.Spawn(class'CapUnpackPawn',,, Joined.Location + 160 * vector(R) + vect(0, 0, 20), R);
			if (Shown == None)
			{
				Log("DXUNPACK: server placed no pawn; exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			// No sequence: nothing moves the frame on either side
			Shown.AnimSequence = '';
			Log("DXUNPACK: server placed " $ Shown.Name $ " before " $ Joined.PlayerReplicationInfo.PlayerName);
			PlacedTime = CapTime;
			Phase = 3;
		}
	}
	else if (Phase == 3)
	{
		Play(CapTime - PlacedTime);
		if (Phase == 3 && Step > 3 && CapTime - PlacedTime >= 16.0)
		{
			Log("DXUNPACK: server done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}

	if (Phase >= 1 && Phase < 9 && CapTime > 140.0)
	{
		Log("DXUNPACK: server out of time, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
