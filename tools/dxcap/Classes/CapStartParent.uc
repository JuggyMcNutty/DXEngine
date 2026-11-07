//=============================================================================
// CapStartParent: ComputerStartConsole's plain window holding a computer
// window. Its ComputerStart, which the original's computer window sends up
// its parents until one takes it (XComputerWindow::Tick, Extension.dll
// 0x1000f7b0), logs with "DXSTART:" and takes it.
//=============================================================================
class CapStartParent extends Window;

function bool ComputerStart()
{
	Log("DXSTART: parent ComputerStart");
	return True;
}
