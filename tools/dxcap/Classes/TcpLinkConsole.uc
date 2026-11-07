//=============================================================================
// TcpLinkConsole: TcpLinks as the original's natives answer them (bugs.md
// NET.20) -- 2 s into whatever map the run starts on, four CapTcpLinks: A's
// BindPort fails (port 1), and each native after it is logged with its
// answer, the link's state and Socket -- the original's a second BindPort
// 0, Listen, Open and Close true doing nothing, the sends and reads 0, the
// string read into kept, Socket 0; B opens with no BindPort; C binds from
// 7880, queues "lost", listens -- its backlog 1: ss -ltn 'sport = :7880'
// shows Send-Q 1 -- and reads by hand from 4 s to 9 s; D binds, queues
// "early", opens to C and, opened, sends 600 characters. Listen and Open
// empty the send queue, so D receives nothing and C's reads start "aaaaa";
// a read takes 255 bytes at most: 255, 255 and 90. The run exits at 12 s.
// Each line starts "DXTCP:".
//=============================================================================
class TcpLinkConsole extends Console;

var float RunTime;
var int Step, Port;
var CapTcpLink A, B, C, D;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (Step == 0 && RunTime > 2.0)
	{
		A = P.Spawn(class'CapTcpLink');
		A.Who = "A";
		A.DeadSteps();
		B = P.Spawn(class'CapTcpLink');
		B.Who = "B";
		B.NeverBound();
		C = P.Spawn(class'CapTcpLink');
		C.Who = "C";
		Port = C.StartListener();
		D = P.Spawn(class'CapTcpLink');
		D.Who = "D";
		D.StartOpener(Port);
		Step = 1;
	}
	else if (Step == 1 && RunTime > 4.0 && RunTime < 9.0)
		C.ReadByHand();
	else if (Step == 1 && RunTime > 12.0)
	{
		Log("DXTCP: exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
}
