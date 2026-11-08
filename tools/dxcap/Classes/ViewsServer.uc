//=============================================================================
// ViewsServer: the serving side of the additional views test
// (ViewsJoinConsole), one of a dedicated server's ServerActors
// (tools/dxcap.sh dedicated <engine> ViewsServer
// 'DXMP_Cathedral?game=DXCapture.CapDeathMatch' 60). bugs.md NET.28.
//
// Once a player has been in 3 s: of the level's PlayerStarts, the one
// farthest from the player whose spot 32 units above it the player's eyes
// cannot see (FastTrace) is chosen, a CapViewMarker -- visible, not always
// relevant, owned by no one -- placed on that spot, and the start's name
// put in the player's name ("views:<name>"), which every client is sent,
// so the client knows what to view from. Every second, as the server has
// them: the player's AdditionalViews, which only its client fills (its
// viewport window's SetViewportActor) and its pawn's replication sends up
// (the original's APlayerPawn::GetOptimizedRepList, Engine.dll 0x103649d0,
// its client branch 0x10364f9e-0x10365105), and whether the marker can see
// the start and the player. 25 s after the marker is placed the server
// exits, so its log is written whole. Each line starts "DXVIEWS:".
//=============================================================================
class ViewsServer extends Info;

var float MapTime, LogTime, InTime, PlacedTime;
var PlayerStart Chosen;
var CapViewMarker Marker;
var bool bPlaced, bDone;

function PlayerStart ChooseStart(DeusExPlayer P)
{
	local PlayerStart S, Best;
	local vector Eye, Spot;
	local float D, BestD;

	Eye = P.Location + vect(0,0,1) * P.BaseEyeHeight;
	BestD = -1;
	foreach AllActors(class'PlayerStart', S)
	{
		Spot = S.Location + vect(0,0,32);
		if (P.FastTrace(Spot, Eye))
			continue;
		D = VSize(S.Location - P.Location);
		if (D > BestD)
		{
			BestD = D;
			Best = S;
		}
	}
	return Best;
}

event Tick(float Delta)
{
	local DeusExPlayer Other, Joined;
	local vector Eye, Spot;

	MapTime += Delta;
	if (bPlaced && !bDone && MapTime - PlacedTime > 25.0)
	{
		Log("DXVIEWS: server exiting");
		ConsoleCommand("exit");
		bDone = true;
	}
	foreach AllActors(class'DeusExPlayer', Other)
		Joined = Other;
	if (Joined == None)
		return;
	InTime += Delta;
	Eye = Joined.Location + vect(0,0,1) * Joined.BaseEyeHeight;
	if (!bPlaced && InTime > 3.0)
	{
		bPlaced = true;
		PlacedTime = MapTime;
		Chosen = ChooseStart(Joined);
		if (Chosen == None)
		{
			Log("DXVIEWS: server found no start out of the player's sight");
			return;
		}
		Spot = Chosen.Location + vect(0,0,32);
		Marker = Spawn(class'CapViewMarker',,, Spot);
		Log("DXVIEWS: server chose " $ Chosen.Name $ " at " $ Chosen.Location $ ", " $ int(VSize(Chosen.Location - Joined.Location))
			$ " units from the player at " $ Joined.Location $ "; marker " $ Marker $ "; start sees the marker " $ FastTrace(Spot, Chosen.Location)
			$ ", the player's eyes see it " $ FastTrace(Spot, Eye));
		Joined.PlayerReplicationInfo.PlayerName = "views:" $ Chosen.Name;
	}
	if (bPlaced && MapTime - LogTime >= 1.0)
	{
		LogTime = MapTime;
		Log("DXVIEWS: server t=" $ int(InTime) $ " views " $ Joined.AdditionalViews[0] $ "," $ Joined.AdditionalViews[1] $ "," $ Joined.AdditionalViews[2]
			$ " the player's eyes see the marker " $ (Marker != None && FastTrace(Marker.Location, Eye)));
	}
}
