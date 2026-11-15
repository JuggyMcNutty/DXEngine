//=============================================================================
// CapGhostToucher: GhostDoorConsole's actor, a CapToucher that logs each
// Touch and UnTouch with "DXGHOST:" in front.
//=============================================================================
class CapGhostToucher extends CapToucher;

event Touch(Actor Other)
{
	Log("DXGHOST: touch " $ Other.Name);
}

event UnTouch(Actor Other)
{
	Log("DXGHOST: untouch " $ Other.Name);
}
