//=============================================================================
// CapTrigger: TouchConsole's trigger, a plain Trigger that logs each Touch
// and UnTouch it gets with "DXTOUCH:", its four Touching slots after.
//=============================================================================
class CapTrigger extends Trigger;

function string Slots()
{
	local string S;
	local int i;

	for (i = 0; i < 4; i++)
	{
		if (i > 0)
			S = S $ ",";
		if (Touching[i] == None)
			S = S $ "-";
		else
			S = S $ Touching[i].Class.Name;
	}
	return S;
}

event Touch(Actor Other)
{
	Log("DXTOUCH: trigger touched by " $ Other.Class.Name $ ", slots " $ Slots());
}

event UnTouch(Actor Other)
{
	Log("DXTOUCH: trigger untouched by " $ Other.Class.Name $ ", slots " $ Slots());
}

defaultproperties
{
	bCollideWorld=False
	CollisionRadius=150.000000
	CollisionHeight=80.000000
}
