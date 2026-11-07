//=============================================================================
// ViewServeConsole: the serving side of the view targets net test
// (ViewJoinConsole; bugs.md NET.22, NET.23) -- a listen server on
// DXMP_Cathedral (CapDeathMatch: without the stock console check), started
// from the menu map as ServeConsole starts one. 3 s after another player is
// in, the host turned to a yaw of 16384, and the other player made to view
// it from behind -- its ViewTarget the host, its bBehindView set, as a
// spectator's view would have it (Deus Ex's game refuses ViewPlayer). Then
// each second for 4 s the host's view rotation, eye height and weapon's
// PlayerViewOffset, and the other player's TargetViewRotation,
// TargetEyeHeight and TargetWeaponViewOffset, which the original's server
// fills each tick from the pawn it views (Engine.dll 0x103a23e6-0x103a249c);
// and the other player's bAdmin, which the original's SpawnPlayActor sets
// on a server only with -alladmin (0x103971ad). Logged with "DXVIEW:".
// Exits 8 s after, so the client leaves first, or after 140 s.
//=============================================================================
class ViewServeConsole extends Console;

var float CapTime, InTime, StartTime;
var int Phase, Step;
var PlayerPawn Joined;

function string Held(Pawn P)
{
	if (P.Weapon == None)
		return "no weapon";
	return "weapon offset " $ P.Weapon.PlayerViewOffset;
}

event Tick(float Delta)
{
	local PlayerPawn P, Other;
	local rotator R;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	CapTime += Delta;

	if (Phase == 0 && CapTime > 3.0)
	{
		Log("DXVIEW: starting the listen server");
		P.ConsoleCommand("start DXMP_Cathedral?game=DXCapture.CapDeathMatch?listen");
		Phase = 1;
	}
	else if (Phase == 1 && P.Level.NetMode != NM_Standalone)
	{
		Log("DXVIEW: serving " $ P.Level.GetLocalURL() $ ", net mode " $ P.Level.NetMode);
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
			R.Yaw = 16384;
			P.ViewRotation = R;
			P.SetRotation(R);
			Joined.ViewTarget = P;
			Joined.bBehindView = true;
			Log("DXVIEW: server: " $ Joined.PlayerReplicationInfo.PlayerName $ " views the host from behind; its bAdmin " $ Joined.bAdmin $ ", its role " $ Joined.Role $ ", remote role " $ Joined.RemoteRole);
			StartTime = CapTime;
			Phase = 3;
		}
	}
	else if (Phase == 3)
	{
		if (Joined == None || Joined.bDeleteMe)
		{
			Log("DXVIEW: server lost the player at step " $ Step $ "; exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		if (Step < 4 && CapTime - StartTime >= 1.0 + Step)
		{
			Step++;
			Log("DXVIEW: server step " $ Step $ ": the host's view rotation " $ P.ViewRotation $ ", eye height " $ P.EyeHeight $ ", " $ Held(P));
			Log("DXVIEW: server step " $ Step $ ": the player's targets: view rotation " $ Joined.TargetViewRotation $ ", eye height " $ Joined.TargetEyeHeight $ ", weapon offset " $ Joined.TargetWeaponViewOffset $ "; viewing " $ Joined.ViewTarget);
		}
		if (Step >= 4 && CapTime - StartTime >= 12.0)
		{
			Log("DXVIEW: server done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}

	if (Phase >= 1 && Phase < 9 && CapTime > 140.0)
	{
		Log("DXVIEW: server out of time, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
