//=============================================================================
// DoubleFileToucher: DoubleFileConsole's actor, whose Touch sets its own
// collision again, inside the window in which SetLocation has taken it out of
// the collision hash.
//=============================================================================
class DoubleFileToucher extends Actor;

event Touch(Actor Other)
{
	SetCollision(true, false, false);
}

defaultproperties
{
	bCollideActors=True
	CollisionRadius=20
	CollisionHeight=20
	bHidden=True
}
