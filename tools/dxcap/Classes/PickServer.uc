//=============================================================================
// PickServer: the serving side of a net pickup test (PickJoinConsole), one of
// a dedicated server's ServerActors (tools/dxcap.sh dedicated; the map
// DXMP_Cathedral?game=DXCapture.CapDeathMatch, which lets the scripted
// client in). Once a player has been in 3 s, a mini crossbow is held at its
// eye level 60 units ahead of where it looks, no physics, as a weapon on a
// wall. Every 2 s, as the server has them: whether that weapon is still
// there, whether the player holds a mini crossbow, the player's FrobTarget
// -- which its client finds and replicates up, and the server's
// ParseRightClick takes -- and what HighlightCenterObject's trace, made
// here, meets. The last two also go to the client in the player's name,
// which every client is sent (the client's log is the one kept when two
// originals share the game's folder). 20 s after the weapon is placed the
// server exits, so its log is written whole.
//=============================================================================
class PickServer extends Info;

var float MapTime, LogTime, InTime, PlacedTime;
var DeusExWeapon Item;
var bool bPlaced, bDone;

event Tick(float Delta)
{
	local DeusExPlayer Other, Joined;
	local rotator R;
	local vector Spot, HitLoc, HitNormal;
	local Actor Hit;

	MapTime += Delta;
	if (bPlaced && !bDone && MapTime - PlacedTime > 20.0)
	{
		Log("DXPICK: server exiting");
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
		R = Joined.ViewRotation;
		R.Pitch = 0;
		Spot = Joined.Location + vect(0,0,1) * Joined.BaseEyeHeight + 60 * vector(R);
		Item = Spawn(class'DeusEx.WeaponMiniCrossbow',,, Spot, R);
		if (Item != None)
		{
			Item.SetPhysics(PHYS_None);
			Log("DXPICK: server placed " $ Item.Name $ " at " $ Item.Location $ " before " $ Joined.Location $ " yaw " $ R.Yaw);
		}
		else
			Log("DXPICK: server found no room for the weapon at " $ Spot);
		bPlaced = true;
		PlacedTime = MapTime;
	}
	if (MapTime - LogTime >= 2.0)
	{
		LogTime = MapTime;
		foreach Joined.TraceActors(class'Actor', Hit, HitLoc, HitNormal,
			Joined.Location + Joined.BaseEyeHeight * vect(0,0,1) + Joined.MaxFrobDistance * vector(Joined.ViewRotation),
			Joined.Location + Joined.BaseEyeHeight * vect(0,0,1))
			break;
		Log("DXPICK: server t=" $ int(InTime) $ " weapon there " $ (Item != None && !Item.bDeleteMe && Item.Owner == None) $ " player holds one " $ (Joined.FindInventoryType(class'DeusEx.WeaponMiniCrossbow') != None) $ " frob target " $ Joined.FrobTarget $ " trace " $ Hit $ " view " $ Joined.ViewRotation);
		Joined.PlayerReplicationInfo.PlayerName = "srv" $ int(InTime) $ ":" $ Joined.FrobTarget $ ":trace " $ Hit;
	}
}
