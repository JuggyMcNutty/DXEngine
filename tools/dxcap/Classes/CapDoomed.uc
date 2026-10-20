//=============================================================================
// CapDoomed: an actor DestroyConsole sets around the one it destroys -- each
// event it takes logged with "DXDESTROY:", its Tag, the other actor's Tag and
// its own bDeleteMe; in state Waiting, its EndState too (Lingering extends
// it, for RotatorConsole). Its Destroyed also logs whether AllActors finds it
// and its owner's Tag.
//=============================================================================
class CapDoomed extends Actor;

function Say(string S)
{
	Log("DXDESTROY: " $ Tag $ " " $ S $ " deleteme " $ bDeleteMe);
}

function string TagOf(Actor A)
{
	if (A == None)
		return "None";
	return string(A.Tag);
}

event Touch(Actor Other) { Say("touch " $ TagOf(Other)); }
event UnTouch(Actor Other) { Say("untouch " $ TagOf(Other)); }
event BaseChange() { Say("basechange " $ TagOf(Base)); }
event Attach(Actor Other) { Say("attach " $ TagOf(Other)); }
event Detach(Actor Other) { Say("detach " $ TagOf(Other)); }
event GainedChild(Actor Other) { Say("gainedchild " $ TagOf(Other)); }
event LostChild(Actor Other) { Say("lostchild " $ TagOf(Other)); }

event Destroyed()
{
	local Actor A;
	local int Found;

	foreach AllActors(class'Actor', A)
	{
		if (A == self)
			Found++;
	}
	Say("destroyed found " $ Found $ " owner " $ TagOf(Owner));
}

state Waiting
{
	event EndState() { Say("endstate"); }
}

// A state extending another, for RotatorConsole's IsInState.
state Lingering extends Waiting
{
}

defaultproperties
{
	bHidden=True
}
