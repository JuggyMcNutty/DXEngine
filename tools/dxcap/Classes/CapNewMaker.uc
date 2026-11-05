//=============================================================================
// CapNewMaker: NewOuterConsole's actor, which in its first Tick makes a
// CapNewProbe by new with no outer, with None and with Self, each logged as
// its path and its Outer, then destroys itself. Each line starts
// "DXNEWOUTER:".
//=============================================================================
class CapNewMaker extends Info;

var bool bDone;

function Report(string Which, Object O)
{
	Log("DXNEWOUTER: the actor's " $ Which $ ": " $ string(O) $ ", its Outer " $ O.Outer);
}

event Tick(float Delta)
{
	if (bDone)
		return;
	bDone = True;
	Report("new", new class'CapNewProbe');
	Report("new(None)", new(None) class'CapNewProbe');
	Report("new(Self)", new(Self) class'CapNewProbe');
	Destroy();
}
