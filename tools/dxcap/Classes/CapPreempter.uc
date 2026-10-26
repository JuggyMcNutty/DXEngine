//=============================================================================
// CapPreempter: PreemptConsole's actor -- Idle, whose EndState diverts it to
// Diverted when bDivert is set; Goal, whose BeginState bounces it to
// Bounced when bBounce is set, and whose Begin label logs; Diverted and
// Bounced, whose Begin labels log. Each line starts "DXPREEMPT:".
//=============================================================================
class CapPreempter extends Actor;

var bool bDivert;
var bool bBounce;

state Idle
{
	function BeginState()
	{
		Log("DXPREEMPT: Idle's BeginState, in " $ GetStateName());
	}

	function EndState()
	{
		Log("DXPREEMPT: Idle's EndState, in " $ GetStateName());
		if (bDivert)
		{
			bDivert = False;
			GotoState('Diverted');
		}
	}
}

state Goal
{
	function BeginState()
	{
		Log("DXPREEMPT: Goal's BeginState, in " $ GetStateName());
		if (bBounce)
		{
			bBounce = False;
			GotoState('Bounced');
		}
	}

Begin:
	Log("DXPREEMPT: Goal's Begin, in " $ GetStateName());
}

state Diverted
{
	function BeginState()
	{
		Log("DXPREEMPT: Diverted's BeginState, in " $ GetStateName());
	}

Begin:
	Log("DXPREEMPT: Diverted's Begin, in " $ GetStateName());
}

state Bounced
{
Begin:
	Log("DXPREEMPT: Bounced's Begin, in " $ GetStateName());
}

defaultproperties
{
	bHidden=True
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
}
