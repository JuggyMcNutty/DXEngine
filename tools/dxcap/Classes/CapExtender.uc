//=============================================================================
// CapExtender: ExtendConsole's actor -- a state Lower with a function and
// two labels, and a state Upper extending it with nothing of its own; a
// class function of the state function's name, for the lookup to pass over
// when the state's is found. Each line starts "DXEXTEND:".
//=============================================================================
class CapExtender extends Actor;

function Hello()
{
	Log("DXEXTEND: Hello from the class, in state " $ GetStateName());
}

state Lower
{
	function Hello()
	{
		Log("DXEXTEND: Hello from Lower, in state " $ GetStateName());
	}

Begin:
	Log("DXEXTEND: Lower's Begin, in state " $ GetStateName());
Other:
	Log("DXEXTEND: Lower's Other, in state " $ GetStateName());
}

state Upper extends Lower
{
}

defaultproperties
{
	bHidden=True
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
}
