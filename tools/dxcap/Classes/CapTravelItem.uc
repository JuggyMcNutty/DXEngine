//=============================================================================
// CapTravelItem: an item of the package's own, DefaultConsole's, with two
// travel properties its own spawn changes from their defaults, 1, to 2
// (PostBeginPlay), so that whether a map exit wrote a value shows in what
// arrives: Kept, set back to its default before the exit, and Changed, set
// to 3. Each logged with "DXDEFAULT:".
//=============================================================================
class CapTravelItem extends Inventory;

var travel int Kept;
var travel int Changed;

function PostBeginPlay()
{
	Super.PostBeginPlay();
	Kept = 2;
	Changed = 2;
	Log("DXDEFAULT: " $ Name $ " spawned: Kept " $ Kept $ ", Changed " $ Changed);
}

defaultproperties
{
	Kept=1
	Changed=1
}
