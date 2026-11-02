//=============================================================================
// AskServer: the serving side of a net test of calls with a return value
// (AskJoinConsole), one of a dedicated server's ServerActors (tools/dxcap.sh
// dedicated; the map DXMP_Cathedral?game=DXCapture.CapDeathMatch, which lets
// the scripted client in). Once a player has been in 3 s, a CapAsker owned by
// its pawn. Once the server has taken the client's 10 calls of it, or 8 s
// after placing it, the server calls the asker's three Client functions,
// which go to the client, each into a variable set before: an int (7), a
// string ("kept") and an actor (the player). The original's call that goes to
// the other side writes nothing (Core.dll ProcessInternal 0x1013f690), so
// each keeps its value. What they hold then, and the client's calls the
// server took (CapAsker's Took and Calls), it says in the asker's Saw, which
// goes to the client, and in this log. 20 s after the asker is placed the
// server exits, so its log is written whole.
//=============================================================================
class AskServer extends Info;

var float MapTime, InTime, PlacedTime;
var CapAsker Asker;
var string Results;
var bool bPlaced, bAsked, bDone;

event Tick(float Delta)
{
	local DeusExPlayer Other, Joined;
	local int I;
	local string S;
	local Actor A;

	MapTime += Delta;
	if (bPlaced && !bDone && MapTime - PlacedTime > 20.0)
	{
		Log("DXASK: server exiting");
		ConsoleCommand("exit");
		bDone = true;
	}
	foreach AllActors(class'DeusExPlayer', Other)
		Joined = Other;
	if (Joined == None)
		return;
	InTime += Delta;
	if (!bPlaced && InTime > 3.0)
	{
		Asker = Spawn(class'CapAsker', Joined);
		Log("DXASK: server placed the asker " $ (Asker != None) $ ", owned by the player " $ (Asker != None && Asker.Owner == Joined));
		bPlaced = true;
		PlacedTime = MapTime;
	}
	if (Asker == None || Asker.bDeleteMe)
		return;
	if (!bAsked && (Asker.Calls >= 10 || MapTime - PlacedTime > 8.0))
	{
		I = 7;
		I = Asker.ClientInt(5);
		S = "kept";
		S = Asker.ClientString();
		A = Joined;
		A = Asker.ClientActor();
		Results = "int " $ I $ ", string [" $ S $ "], actor kept " $ (A == Joined) $ ", None " $ (A == None);
		Log("DXASK: server sent: 7 then ClientInt(5), [kept] then ClientString(), the player then ClientActor() leave " $ Results);
		bAsked = true;
	}
	if (bAsked)
	{
		S = "took " $ Asker.Took $ " in " $ Asker.Calls $ " calls; " $ Results;
		if (S != Asker.Saw)
		{
			Asker.Saw = S;
			Log("DXASK: server says " $ S);
		}
	}
}

defaultproperties
{
	RemoteRole=ROLE_None
}
