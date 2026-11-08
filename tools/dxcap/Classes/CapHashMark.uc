//=============================================================================
// CapHashMark (bugs.md PHYS.36): HashOrderConsole's mark, an actor
// in the collision hash that blocks nothing, with no physics, so that the
// hash's order of it against the others stays the order of their last
// filing (spawn, SetLocation). Each Touch, UnTouch and BaseChange logged
// with "DXHASH:", its Tag and the other's.
//=============================================================================
class CapHashMark extends Actor;

function Say(string S)
{
	Log("DXHASH: " $ Tag $ " " $ S);
}

function string TagOf(Actor A)
{
	if (A == None)
		return "None";
	return string(A.Tag);
}

event Touch(Actor Other) { Say("touch " $ TagOf(Other)); }
event UnTouch(Actor Other) { Say("untouch " $ TagOf(Other)); }
event BaseChange() { Say("basechange to " $ TagOf(Base)); }

defaultproperties
{
	bHidden=True
	DrawType=DT_None
	bCollideActors=True
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	Physics=PHYS_None
	CollisionRadius=8.000000
	CollisionHeight=8.000000
}
