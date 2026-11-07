//=============================================================================
// ViewJoinConsole: the joining side of the view targets net test
// (ViewServeConsole; bugs.md NET.22) -- from the menu map it opens the server
// on this machine; once in the game and viewing another pawn (its
// ViewTarget, which the server sets), each second for 4 s its
// TargetViewRotation, TargetEyeHeight and TargetWeaponViewOffset as the
// server sent them, then what its PlayerCalcView makes of them: the
// PlayerPawn's view puts them on its copy of the viewed pawn, the view
// rotation if it is a player (bIsPlayer as this side has it), so the
// pawn's view rotation and eye height here after it. Logged with "DXVIEW:".
// Exits after the fourth, or 60 s into the game -- or back in the menu,
// dropped or never in after 40 s.
//=============================================================================
class ViewJoinConsole extends Console;

var float MenuTime, GameTime, ViewTime;
var int Step;

event Tick(float Delta)
{
	local PlayerPawn P;
	local Pawn Viewed;
	local Actor ViewActor;
	local vector CameraLocation;
	local rotator CameraRotation;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;

	if (P.Level.NetMode != NM_Client)
	{
		MenuTime += Delta;
		if (Step == 0 && MenuTime > 3.0)
		{
			Log("DXVIEW: opening 127.0.0.1:7790");
			P.ConsoleCommand("open 127.0.0.1:7790");
			Step = 1;
		}
		else if (Step >= 1 && Step < 9 && (GameTime > 0.0 || MenuTime > 40.0))
		{
			// Dropped back to the menu, or never in: exit, so the log is
			// written.
			Log("DXVIEW: in the menu, " $ GameTime $ " s of game; exiting");
			P.ConsoleCommand("exit");
			Step = 9;
		}
		return;
	}

	GameTime += Delta;
	Viewed = Pawn(P.ViewTarget);
	if (Step == 1 && Viewed != None && Viewed != P)
	{
		Log("DXVIEW: client views another pawn " $ GameTime $ " s into the game, behind view " $ P.bBehindView $ ", its bIsPlayer here " $ Viewed.bIsPlayer);
		ViewTime = GameTime;
		Step = 2;
	}
	if (Step >= 2 && Step < 6 && Viewed != None && GameTime - ViewTime >= float(Step - 1))
	{
		Log("DXVIEW: client step " $ (Step - 1) $ ": targets: view rotation " $ P.TargetViewRotation $ ", eye height " $ P.TargetEyeHeight $ ", weapon offset " $ P.TargetWeaponViewOffset);
		ViewActor = None;
		P.PlayerCalcView(ViewActor, CameraLocation, CameraRotation);
		Log("DXVIEW: client step " $ (Step - 1) $ ": after PlayerCalcView the viewed pawn's view rotation " $ Viewed.ViewRotation $ ", eye height " $ Viewed.EyeHeight $ "; the camera's rotation " $ CameraRotation);
		Step++;
	}
	if (Step < 9 && (Step == 6 || GameTime > 60.0))
	{
		Log("DXVIEW: client exiting " $ GameTime $ " s into the game");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
