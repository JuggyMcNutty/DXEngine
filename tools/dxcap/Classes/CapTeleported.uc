//=============================================================================
// CapTeleported: an actor TeleportConsole moves with SetLocation, and one it
// moves onto -- each Touch, UnTouch, BaseChange and EncroachedBy logged with
// "DXTELE:", its Tag, the other actor's Tag and where each stands at that
// moment. It collides with actors, not the world, and blocks nothing.
//=============================================================================
class CapTeleported extends Actor;

function Say(string S)
{
	Log("DXTELE: " $ Tag $ " " $ S);
}

function string TagOf(Actor A)
{
	if (A == None)
		return "None";
	return string(A.Tag);
}

event Touch(Actor Other) { Say("touch " $ TagOf(Other) $ " at " $ Other.Location $ ", me at " $ Location $ ", my base " $ TagOf(Base)); }
event UnTouch(Actor Other) { Say("untouch " $ TagOf(Other) $ " at " $ Other.Location $ ", me at " $ Location); }
event BaseChange() { Say("basechange to " $ TagOf(Base) $ ", me at " $ Location); }
event EncroachedBy(Actor Other) { Say("encroachedby " $ TagOf(Other) $ " at " $ Other.Location $ ", me at " $ Location); }

defaultproperties
{
	bCollideActors=True
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	bHidden=True
	CollisionRadius=20.000000
	CollisionHeight=20.000000
}
