//=============================================================================
// OwnerCallJoinConsole: the joining side (with OwnerCallServer):
// opens 127.0.0.1:7790 3 s after the menu, and once in, logs each
// CapOwnerCaller call as it runs (the caller's own DXNET6: lines); 25 s
// after it first sees the caller, or back in the menu, or with no caller
// after 40 s, it logs the counts and exits. Diff the two clients' DXNET6:
// lines and the server's last line.
//=============================================================================
class OwnerCallJoinConsole extends Console;

var float MenuTime, GameTime, SeenTime;
var int Step;
var CapOwnerCaller Caller;

event Tick(float Delta)
{
	local DeusExPlayer P;
	local CapOwnerCaller Found;

	Super.Tick(Delta);
	if (Viewport == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = DeusExPlayer(Viewport.Actor);

	if (P.Level.NetMode != NM_Client)
	{
		MenuTime += Delta;
		if (Step == 0 && MenuTime > 3.0)
		{
			Log("DXNET6: opening 127.0.0.1:7790");
			P.ConsoleCommand("open 127.0.0.1:7790");
			Step = 1;
		}
		else if (Step >= 1 && Step < 5 && (GameTime > 0.0 || MenuTime > 40.0))
		{
			Log("DXNET6: in the menu, " $ GameTime $ " s of game; exiting");
			P.ConsoleCommand("exit");
			Step = 5;
		}
		return;
	}

	GameTime += Delta;
	if (Caller == None)
	{
		foreach P.AllActors(class'CapOwnerCaller', Found)
			Caller = Found;
		if (Caller != None)
		{
			SeenTime = GameTime;
			Log("DXNET6: the caller is here, owned by the player " $ (Caller.Owner == P));
		}
	}
	if ((Caller != None && GameTime - SeenTime > 25.0) || (Caller == None && GameTime > 40.0))
	{
		if (Caller != None)
			Log("DXNET6: client got ClientOwned " $ Caller.OwnedHere $ ", ClientPlain " $ Caller.PlainHere $ "; exiting");
		else
			Log("DXNET6: no caller after " $ int(GameTime) $ " s; exiting");
		P.ConsoleCommand("exit");
		Step = 5;
	}
}
