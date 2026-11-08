//=============================================================================
// LinkEdgesConsole: TcpLink past BindPort, as the original's tick and
// natives have it (IpDrv.dll ATcpLink::Tick 0x10706fb0, execClose
// 0x10707fb0, execIsConnected 0x10706cb0, CheckConnectionQueue 0x10707260),
// logged with "DXEDGE:" by CapEdgeLinks, on whatever map the run starts on.
// At 2 s: L binds 7890 and listens, greeting what it accepts; A binds 7891
// and opens to L. L is spawned first, so it ticks first: its accept and
// "hello" come before A's tick that finds the connection made.
// - The original reads while connecting (case 3 of the tick: the connect's
//   check, then the read), so A's Opened and its "hello" come in one tick;
//   DXEngine reads once connected, a tick later.
// - The original's accept keeps the peer's port as the address holds it,
//   in network order (0x107074da, 0x1070756a): L logs 54046 (7891's bytes
//   swapped); DXEngine 7891.
// Three ticks after A has its "hello": A Closes and is asked IsConnected
// at once -- the original's answer is whether its socket is writable, true
// while the close is pending; DXEngine's false past Connected. C binds
// 7893 and Closes without opening -- the original's goes to
// ConnectClosePending (6) keeping its socket, its next tick's shutdown of an
// unconnected socket failing ("Close: Error while attempting to close
// socket"); whether its tick's end check then finds the socket ended (and
// closes it, Closed, 0) is Wine's select's to say, so a second BindPort
// answers 0 ("already bound") while the socket is kept; DXEngine closes it
// at once (0), raises nothing and binds again (7894). L2 binds 7895, listens and Closes with no connection
// -- the original's goes to ListenClosing (7) at once, its socket kept,
// nothing raised, and Listen answers true again; DXEngine's goes to 5, then
// closes it with Closed and Initialized (0), and Listen answers false.
// Exits at 10 s.
//=============================================================================
class LinkEdgesConsole extends Console;

var float RunTime;
var int Step, StepTick, Ticks;
var CapEdgeLink L, A, C, L2;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Step == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	Ticks++;

	if (Step == 0 && RunTime > 2.0)
	{
		L = P.Spawn(class'CapEdgeLink');
		L.Who = "L";
		L.bGreet = true;
		Log("DXEDGE: L BindPort " $ L.BindPort(7890) $ ", Listen " $ L.Listen() $ ", state " $ L.LinkState);
		A = P.Spawn(class'CapEdgeLink');
		A.Who = "A";
		Log("DXEDGE: A BindPort " $ A.BindPort(7891) $ ", Open " $ A.OpenLocal(7890) $ ", state " $ A.LinkState);
		Step = 1;
	}
	else if (Step == 1 && A.bOpened && A.bGotText && A.Ticks > A.TextTick + 3)
	{
		Log("DXEDGE: A opened at its tick " $ A.OpenedTick $ ", first text at " $ A.TextTick);
		Log("DXEDGE: A Close " $ A.Close() $ ", then IsConnected " $ A.IsConnected() $ ", state " $ A.LinkState);
		C = P.Spawn(class'CapEdgeLink');
		C.Who = "C";
		Log("DXEDGE: C BindPort " $ C.BindPort(7893) $ ", state " $ C.LinkState);
		Log("DXEDGE: C Close " $ C.Close() $ ", state " $ C.LinkState);
		L2 = P.Spawn(class'CapEdgeLink');
		L2.Who = "L2";
		Log("DXEDGE: L2 BindPort " $ L2.BindPort(7895) $ ", Listen " $ L2.Listen() $ ", state " $ L2.LinkState);
		Log("DXEDGE: L2 Close " $ L2.Close() $ ", state " $ L2.LinkState);
		StepTick = Ticks;
		Step = 2;
	}
	else if (Step == 2 && Ticks > StepTick + 10)
	{
		Log("DXEDGE: C state " $ C.LinkState $ ", BindPort again " $ C.BindPort(7894) $ ", state " $ C.LinkState);
		Log("DXEDGE: L2 state " $ L2.LinkState $ ", Listen again " $ L2.Listen() $ ", state " $ L2.LinkState);
		Step = 3;
	}
	else if (RunTime > 10.0)
	{
		Log("DXEDGE: exiting, states L " $ L.LinkState $ ", A " $ A.LinkState);
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
