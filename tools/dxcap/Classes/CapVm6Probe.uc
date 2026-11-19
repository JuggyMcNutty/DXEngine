//=============================================================================
// CapVm6Probe: the class of Vm6Console's NoneProbe, which stays None: its
// variables and functions are what the Accessed None cases reach through it.
//=============================================================================
class CapVm6Probe extends Actor;

var int Count;
var float Energy;
var bool bFlag;

function int Get()
{
	return 7;
}

function Nudge()
{
	Count++;
}

defaultproperties
{
	bHidden=True
}
