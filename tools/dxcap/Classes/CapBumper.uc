//=============================================================================
// CapBumper: BumpConsole's actor, which logs its BumpWall and Bump events
// with "DXBUMP:". It collides with the world and with actors, draws nothing
// and does not fall.
//=============================================================================
class CapBumper extends Actor;

event BumpWall(vector HitLocation, vector HitNormal)
{
	Log("DXBUMP: BumpWall at " $ HitLocation $ ", normal " $ HitNormal $ ", the actor at " $ Location);
}

event Bump(Actor Other)
{
	Log("DXBUMP: Bump " $ Other.Name $ ", the actor at " $ Location);
}

defaultproperties
{
	bHidden=True
	DrawType=DT_None
	bCollideActors=True
	bCollideWorld=True
	bBlockActors=True
	bBlockPlayers=True
	CollisionRadius=10
	CollisionHeight=10
	Physics=PHYS_None
}
