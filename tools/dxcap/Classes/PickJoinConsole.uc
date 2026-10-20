//=============================================================================
// PickJoinConsole: the joining side of a net pickup test -- opens the server
// on this machine (a dedicated one running PickServer), stands looking
// level, and 12 s in
// uses what it looks at as the right mouse button does (ParseRightClick,
// the stock binding's command); its frob target, and whether it holds a
// mini crossbow, logged each second, with its name, in which the server's
// PickServer puts its own frob target for the player. Exits 22 s into the
// game, or back in the menu.
//=============================================================================
class PickJoinConsole extends Console;

var float MenuTime, GameTime, LogTime;
var int Step;

event Tick(float Delta)
{
	local DeusExPlayer P;

	Super.Tick(Delta);
	if (Viewport == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = DeusExPlayer(Viewport.Actor);

	if (P.Level.NetMode != NM_Client)
	{
		MenuTime += Delta;
		if (Step == 0 && MenuTime > 3.0)
		{
			Log("DXPICK: opening 127.0.0.1:7790");
			P.ConsoleCommand("open 127.0.0.1:7790");
			Step = 1;
		}
		else if (Step >= 1 && Step < 5 && (GameTime > 0.0 || MenuTime > 40.0))
		{
			Log("DXPICK: in the menu, " $ GameTime $ " s of game; exiting");
			P.ConsoleCommand("exit");
			Step = 5;
		}
		return;
	}

	GameTime += Delta;
	P.ViewRotation.Pitch = 0;
	if (GameTime - LogTime >= 1.0)
	{
		LogTime = GameTime;
		Log("DXPICK: t=" $ int(GameTime) $ " frob target " $ P.FrobTarget $ " holds one " $ (P.FindInventoryType(class'DeusEx.WeaponMiniCrossbow') != None) $ " in hand " $ P.inHand $ " at " $ P.Location $ " name " $ P.PlayerReplicationInfo.PlayerName);
	}
	if (Step == 1 && GameTime > 12.0)
	{
		Log("DXPICK: using " $ P.FrobTarget);
		P.ConsoleCommand("ParseRightClick");
		Step = 2;
	}
	else if (Step == 2 && GameTime > 22.0)
	{
		Log("DXPICK: exiting");
		P.ConsoleCommand("exit");
		Step = 5;
	}
}
