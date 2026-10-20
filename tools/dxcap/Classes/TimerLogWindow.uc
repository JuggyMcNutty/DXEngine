//=============================================================================
// TimerLogWindow: a window of the package's own that adds timers and logs,
// with "DXTIMER:", each call they make: its arguments against what was
// given, and the most calls one timer makes in a tick of the windows.
// TimerConsole puts it on the main menu.
//=============================================================================
class TimerLogWindow extends Window;

var int OnceID, LoopID, ZeroID, FirstID, SecondID, NoNameID;
var int LoopCalls, ZeroCalls, OrderCalls;
var int LoopTickCalls, LoopMostCalls;

function Start()
{
	OnceID = AddTimer(0.5, False, 7, 'OnceTimer');
	LoopID = AddTimer(0.001, True, 9, 'LoopTimer');
	ZeroID = AddTimer(0, True, 11, 'ZeroTimer');
	FirstID = AddTimer(0.3, False, 1, 'OrderTimer');
	SecondID = AddTimer(0.3, False, 2, 'OrderTimer');
	NoNameID = AddTimer(0.2, False, 3);
	bTickEnabled = True;
	Log("DXTIMER: added: ids not 0 " $ (OnceID != 0 && LoopID != 0 && ZeroID != 0 && FirstID != 0 && SecondID != 0)
		$ ", each its own " $ (OnceID != LoopID && LoopID != ZeroID && FirstID != SecondID)
		$ "; with no function name: not 0 " $ (NoNameID != 0) $ ", -1 " $ (NoNameID == -1));
}

event Tick(float deltaSeconds)
{
	LoopTickCalls = 0;
}

function OnceTimer(int timerID, int invocations, int clientData)
{
	Log("DXTIMER: OnceTimer: its id " $ (timerID == OnceID) $ ", invocations " $ invocations $ ", client data " $ clientData);
}

function LoopTimer(int timerID, int invocations, int clientData)
{
	LoopCalls++;
	LoopTickCalls++;
	if (LoopTickCalls > LoopMostCalls)
		LoopMostCalls = LoopTickCalls;
	if (LoopCalls <= 3)
		Log("DXTIMER: LoopTimer call " $ LoopCalls $ ": its id " $ (timerID == LoopID) $ ", invocations over 1 "
			$ (invocations > 1) $ ", client data " $ clientData);
	if (LoopCalls == 50)
	{
		Log("DXTIMER: LoopTimer: the most calls in one tick " $ LoopMostCalls $ "; removed");
		RemoveTimer(LoopID);
	}
}

function ZeroTimer(int timerID, int invocations, int clientData)
{
	ZeroCalls++;
	if (ZeroCalls <= 2)
		Log("DXTIMER: ZeroTimer call " $ ZeroCalls $ ": its id " $ (timerID == ZeroID) $ ", invocations over 1000 "
			$ (invocations > 1000) $ ", client data " $ clientData);
	if (ZeroCalls == 2)
		RemoveTimer(ZeroID);
}

function OrderTimer(int timerID, int invocations, int clientData)
{
	OrderCalls++;
	Log("DXTIMER: OrderTimer call " $ OrderCalls $ ": client data " $ clientData $ ", the second added " $ (timerID == SecondID));
}
