//=============================================================================
// CapW6Modal: a modal for TreeChangeConsole (bugs.md EXT.45, EXT.44) that destroys
// itself in its own VisibilityChanged(True), as it is shown: the root's grab
// counts before and after tell whether the native half's grab and ungrab
// stay paired (UModalWindow::VisibilityChanged in DXEngine).
//=============================================================================
class CapW6Modal extends ModalWindow;

var bool bDie;

event VisibilityChanged(bool bNewVisibility)
{
	Log("DXWIN6: modal VisibilityChanged " $ bNewVisibility);
	if (bNewVisibility && bDie)
	{
		bDie = False;
		Log("DXWIN6: modal destroys itself as it shows");
		Destroy();
	}
}
