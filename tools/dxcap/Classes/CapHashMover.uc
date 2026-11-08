//=============================================================================
// CapHashMover (bugs.md PHYS.36, PHYS.39): the actor HashOrderConsole moves
// with SetLocation onto its CapHashMarks; not a CapHashMark, so that the
// console's foreach over those passes it over. Wide enough to take three
// marks 20 units apart. Its Touch, UnTouch and BaseChange logged with
// "DXHASH:".
//=============================================================================
class CapHashMover extends Actor;

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
	CollisionRadius=32.000000
	CollisionHeight=8.000000
}
