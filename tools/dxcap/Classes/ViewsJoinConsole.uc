//=============================================================================
// ViewsJoinConsole: the joining side of the additional views test -- opens
// the server on this machine (a dedicated one running ViewsServer). bugs.md
// NET.28.
//
// Once its name, as the server sends it, reads "views:<start>", a viewport
// window on its root views from that PlayerStart (SetViewportActor), as a
// security computer's camera window and the spy drone's view do in a net
// game (ComputerSecurityCameraWindow, AugmentationDisplayWindow): the window
// makes the start one of the player's AdditionalViews, here on the client.
// Every second: its own AdditionalViews as the client has them, and whether
// it has the server's marker, which only the start's view makes relevant.
// Exits 22 s into the game, or back in the menu. Each line starts "DXVIEWS:".
//=============================================================================
class ViewsJoinConsole extends Console;

var float MenuTime, GameTime, LogTime;
var int Step;
var ViewportWindow Win;

event Tick(float Delta)
{
	local DeusExPlayer P;
	local PlayerStart S, Start;
	local CapViewMarker M, Seen;
	local string N;

	Super.Tick(Delta);
	if (Viewport == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = DeusExPlayer(Viewport.Actor);

	if (P.Level.NetMode != NM_Client)
	{
		MenuTime += Delta;
		if (Step == 0 && MenuTime > 3.0)
		{
			Log("DXVIEWS: opening 127.0.0.1:7790");
			P.ConsoleCommand("open 127.0.0.1:7790");
			Step = 1;
		}
		else if (Step >= 1 && Step < 5 && (GameTime > 0.0 || MenuTime > 40.0))
		{
			Log("DXVIEWS: in the menu, " $ GameTime $ " s of game; exiting");
			P.ConsoleCommand("exit");
			Step = 5;
		}
		return;
	}

	GameTime += Delta;
	if (Step == 1 && P.PlayerReplicationInfo != None && Left(P.PlayerReplicationInfo.PlayerName, 6) == "views:")
	{
		N = Mid(P.PlayerReplicationInfo.PlayerName, 6);
		foreach P.AllActors(class'PlayerStart', S)
			if (string(S.Name) == N)
				Start = S;
		if (Start != None && DeusExRootWindow(P.rootWindow) != None)
		{
			Win = ViewportWindow(DeusExRootWindow(P.rootWindow).NewChild(class'ViewportWindow'));
			Win.SetSize(160, 120);
			Win.SetViewportActor(Start);
			Log("DXVIEWS: client viewing from " $ Start.Name $ " at " $ Start.Location $ "; its views " $ P.AdditionalViews[0]);
		}
		else
			Log("DXVIEWS: client found no " $ N $ " or no root");
		Step = 2;
	}
	if (GameTime - LogTime >= 1.0)
	{
		LogTime = GameTime;
		Seen = None;
		foreach P.AllActors(class'CapViewMarker', M)
			Seen = M;
		Log("DXVIEWS: client t=" $ int(GameTime) $ " views " $ P.AdditionalViews[0] $ "," $ P.AdditionalViews[1] $ "," $ P.AdditionalViews[2]
			$ " marker " $ Seen $ " name " $ P.PlayerReplicationInfo.PlayerName);
	}
	if (Step >= 1 && Step < 5 && GameTime > 22.0)
	{
		Log("DXVIEWS: exiting");
		P.ConsoleCommand("exit");
		Step = 5;
	}
}
