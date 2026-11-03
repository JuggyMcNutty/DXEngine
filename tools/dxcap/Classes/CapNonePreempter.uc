//=============================================================================
// CapNonePreempter: NonePreemptConsole's actor -- Idle, whose
// EndState sends it to no state (GotoState('')) when bToNone is set, or to a
// state its class has not when bToMissing is set; Goal, whose BeginState and
// Begin label log. Each line starts "DXNONEPREEMPT:".
//=============================================================================
class CapNonePreempter extends Actor;

var bool bToNone;
var bool bToMissing;

state Idle
{
	function EndState()
	{
		Log("DXNONEPREEMPT: Idle's EndState, in " $ GetStateName());
		if (bToNone)
		{
			bToNone = False;
			GotoState('');
			Log("DXNONEPREEMPT: Idle's EndState after GotoState None, in " $ GetStateName());
		}
		if (bToMissing)
		{
			bToMissing = False;
			GotoState('NoSuchState');
			Log("DXNONEPREEMPT: Idle's EndState after GotoState NoSuchState, in " $ GetStateName());
		}
	}
}

state Goal
{
	function BeginState()
	{
		Log("DXNONEPREEMPT: Goal's BeginState, in " $ GetStateName());
	}

Begin:
	Log("DXNONEPREEMPT: Goal's Begin, in " $ GetStateName());
}

defaultproperties
{
	bHidden=True
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
}
