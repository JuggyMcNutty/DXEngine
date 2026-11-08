//=============================================================================
// UdpEdgesConsole: the script's links at their edges -- 2 s into whatever
// map the run starts on (both engines; dxe on DX.dx, the original from its
// menu map). bugs.md NET.31, NET.37. Each line starts "DXUDPEDGE:".
//
// A CapUdpEdgeLink R resolves 127.0.0.1, 255.255.255.255 and localhost,
// each answer logged as raised within the Resolve call or at a later tick:
// the original's takes a dotted address at once and fails 255.255.255.255
// at once, a name from a later tick (IpDrv.dll AInternetLink::execResolve
// 0x10702d70). R binds an ephemeral port, and so does a TcpLink: each
// BindPort's answer and the link's Port, 0 in the original's
// (0x1070c810, 0x10706960). By hand (RMODE_Manual, set at its spawn: the
// link's constructor sets receiving by events) R sends itself 100
// bytes, which a ReadBinary of 50 reads, then a ReadBinary with nothing
// waiting; 5,000 characters of text and then "short", each read by
// ReadText, then a ReadText with nothing waiting, the string set to "keep"
// before each: the count, the string's length and GetLastError after each.
// The original's read of a datagram longer than the room fails as
// WSAEMSGSIZE (10040), logging "ReadBinary: Error reading text." or
// "ReadText: Error reading text.", the datagram gone (0x1070d6f0,
// 0x1070d380), a ReadBinary's array holding its first bytes all the same;
// its ReadText empties the string first and makes it of its buffer up to a
// NUL, at most 1,023 characters, not of the count read (bugs.md D.76); its
// GetLastError is WinSock's last error of the thread (0x10703490), 10035
// after a read with nothing waiting, 0 after one that took a datagram. Then by events: 5,000 characters and "after", each
// event logged by its length -- the original's tick reads up to 4,095
// bytes and raises nothing for a longer datagram (0x1070cee0). Exits 3 s
// after.
//=============================================================================
class UdpEdgesConsole extends Console;

var float RunTime, EventsAt;
var int Step;
var CapUdpEdgeLink R;
var TcpLink T;
var string Long;

event Tick(float Delta)
{
	local PlayerPawn P;
	local int i, Bound;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (R != None)
		R.Ticks++;

	if (Step == 0 && RunTime > 2.0)
	{
		R = P.Spawn(class'CapUdpEdgeLink');
		R.Label = "R";
		R.ToManual();
		R.bInCall = true;
		R.Resolve("127.0.0.1");
		R.bInCall = false;
		Log("DXUDPEDGE: after Resolve(127.0.0.1) at tick " $ R.Ticks);
		R.bInCall = true;
		R.Resolve("255.255.255.255");
		R.bInCall = false;
		Log("DXUDPEDGE: after Resolve(255.255.255.255) at tick " $ R.Ticks);
		R.bInCall = true;
		R.Resolve("localhost");
		R.bInCall = false;
		Log("DXUDPEDGE: after Resolve(localhost) at tick " $ R.Ticks);

		Bound = R.BindPort();
		Log("DXUDPEDGE: UdpLink BindPort() bound a port " $ (Bound > 0) $ ", its Port the port bound " $ (R.Port == Bound) $ ", its Port 0 " $ (R.Port == 0));
		T = P.Spawn(class'TcpLink');
		i = T.BindPort();
		Log("DXUDPEDGE: TcpLink BindPort() bound a port " $ (i > 0) $ ", its Port the port bound " $ (T.Port == i) $ ", its Port 0 " $ (T.Port == 0));

		R.AimAtSelf(Bound);
		Log("DXUDPEDGE: sent 100 bytes " $ R.SendBytes(100));

		Long = "x";
		for (i = 0; i < 12; i++)
			Long = Long $ Long;
		Long = Long $ Left(Long, 904);
		Step = 1;
	}
	else if (Step == 1 && RunTime > 2.3)
	{
		R.ReadBin(50);
		R.ReadBin(50);
		Log("DXUDPEDGE: sent " $ Len(Long) $ " characters " $ R.SendToSelf(Long) $ ", then 'short' " $ R.SendToSelf("short"));
		Step = 2;
	}
	else if (Step == 2 && RunTime > 2.6)
	{
		R.ReadTxt();
		R.ReadTxt();
		R.ReadTxt();
		R.ToEvents();
		Log("DXUDPEDGE: by events: sent " $ Len(Long) $ " characters " $ R.SendToSelf(Long) $ ", then 'after' " $ R.SendToSelf("after"));
		EventsAt = RunTime;
		Step = 3;
	}
	else if (Step == 3 && RunTime > EventsAt + 3.0)
	{
		Log("DXUDPEDGE: done");
		R.Destroy();
		T.Destroy();
		P.ConsoleCommand("exit");
		Step = 4;
	}
}
