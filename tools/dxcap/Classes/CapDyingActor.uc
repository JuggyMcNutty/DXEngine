//=============================================================================
// CapDyingActor: StateConsole's witness -- its state code logs, destroys
// the actor, and logs again ("DXSTATE:"): the line after Destroy runs only
// where state code goes on after its actor is destroyed.
//=============================================================================
class CapDyingActor extends Info;

auto state Dying
{
Begin:
	Log("DXSTATE: " $ Self $ " before Destroy");
	Destroy();
	Log("DXSTATE: " $ Self $ " after Destroy, bDeleteMe " $ bDeleteMe);
	Log("DXSTATE: " $ Self $ " a second line after Destroy");
}
