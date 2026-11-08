//=============================================================================
// UdpRebindConsole: a UdpLink after a BindPort that fails -- 2 s into
// whatever map the run starts on (both engines; dxe on DX.dx, the original
// from its menu map). bugs.md NET.30.
//
// Link A (a CapRebindLink, events, text) binds port 2150. Link B asks for
// A's port, which fails, then for the next port, and sends "from B" to A on
// 127.0.0.1; link C asks for A's port, which fails, and sends "from C" to A
// at once. Each answer is logged, and a second later what A received and
// from which port. The original's BindPort closes a socket whose bind
// failed and leaves the link's Socket 0 (IpDrv.dll 0x1070c979, 0x1070c9a2):
// a second BindPort answers 0, logged "BindPort: already bound"
// (0x1070c88b); SendText answers True and sends nothing (0x1070cb5d); the
// tick reads nothing (0x1070cf25). Each line starts "DXREBIND:".
//=============================================================================
class UdpRebindConsole extends Console;

var float RunTime;
var int Step;
var CapRebindLink A, B, C;

event Tick(float Delta)
{
	local PlayerPawn P;
	local int PortA, R;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (Step == 0 && RunTime > 2.0)
	{
		A = P.Spawn(class'CapRebindLink');
		A.Label = "A";
		PortA = A.BindPort(2150, false);
		Log("DXREBIND: A bound " $ PortA);

		B = P.Spawn(class'CapRebindLink');
		B.Label = "B";
		R = B.BindPort(PortA, false);
		Log("DXREBIND: B asked for " $ PortA $ ": " $ R);
		R = B.BindPort(PortA + 1, false);
		Log("DXREBIND: B asked again for " $ (PortA + 1) $ ": " $ R $ ", its Port " $ B.Port);
		Log("DXREBIND: B sent " $ B.SendTo(PortA, "from B"));

		C = P.Spawn(class'CapRebindLink');
		C.Label = "C";
		R = C.BindPort(PortA, false);
		Log("DXREBIND: C asked for " $ PortA $ ": " $ R);
		Log("DXREBIND: C sent " $ C.SendTo(PortA, "from C"));
		Step = 1;
	}
	else if (Step == 1 && RunTime > 3.0)
	{
		Log("DXREBIND: done");
		A.Destroy();
		B.Destroy();
		C.Destroy();
		P.ConsoleCommand("exit");
		Step = 2;
	}
}
