//=============================================================================
// NetStatsJoinConsole: STAT NET on a client (bugs.md WIN.29), logged with
// "DXNETSTAT:". From the menu map it opens the server on this machine
// (ServeConsole's, the other engine's), and once in the game: 6 s in, `STAT
// NET` (URender::Exec, Render.dll 0x10b147b0), the panel DrawStats draws
// for a client's connection (0x10b13dc0-0x10b14116) -- "NET (IN/OUT):" from
// 16 pixels down at the left, the ping, the channels, the packets out of
// order, the loss, the packets, bunches and bytes a second each way, a star
// by a rate past the speed, and the speed twice, "NetSpeed" -- and each
// stats period's line to the log under its header ("Ping  Ch Loss IRate
// ORate IBnch OBnch  IPkt  OPkt ILoss OLoss"); a marked shot 4 s after; then
// `STAT NET` off, on again 2 s later (the header logged again, the panel off
// the frame before), off 4 s after, and an exit 20 s into the game -- or
// back in the menu, dropped or never in after 40 s. The player's connection
// figures the scripts see are logged with each step (its NetSpeed, and the
// GETPING and GETLOSS words' answers). Compare the shots' panel -- its lines
// and places alike, the figures each run's -- and the log's lines, the
// header at each turn on, a line a period, about one a second.
//=============================================================================
class NetStatsJoinConsole extends Console;

var float MenuTime, GameTime;
var int Step;
var bool bMarking;

function Ask(PlayerPawn P, string W)
{
	Log("DXNETSTAT: t=" $ int(GameTime) $ " '" $ W $ "' gave '" $ P.ConsoleCommand(W) $ "'");
}

function ShowLink(PlayerPawn P)
{
	Log("DXNETSTAT: t=" $ int(GameTime) $ " NetSpeed " $ P.Player.CurrentNetSpeed);
	Ask(P, "GETPING");
	Ask(P, "GETLOSS");
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || Step == 99)
		return;
	P = Viewport.Actor;

	if (P.Level.NetMode != NM_Client)
	{
		MenuTime += Delta;
		if (Step == 0 && MenuTime > 3.0)
		{
			Log("DXNETSTAT: opening 127.0.0.1:7790");
			P.ConsoleCommand("open 127.0.0.1:7790");
			Step = 1;
		}
		else if (Step >= 1 && (GameTime > 0.0 || MenuTime > 40.0))
		{
			Log("DXNETSTAT: in the menu, " $ GameTime $ " s of game; exiting");
			P.ConsoleCommand("exit");
			Step = 99;
		}
		return;
	}

	GameTime += Delta;
	if (Step == 1 && GameTime > 6.0)
	{
		ShowLink(P);
		Ask(P, "STAT NET");
		bMarking = true;
		Step = 2;
	}
	else if (Step == 2 && GameTime > 10.0)
	{
		Log("DXNETSTAT: shot 1, STAT NET");
		Log("DXCAP: shot 1");
		P.ConsoleCommand("shot");
		bMarking = false;
		ShowLink(P);
		Ask(P, "STAT NET");
		Step = 3;
	}
	else if (Step == 3 && GameTime > 12.0)
	{
		Ask(P, "STAT NET");
		Step = 4;
	}
	else if (Step == 4 && GameTime > 16.0)
	{
		ShowLink(P);
		Ask(P, "STAT NET");
		Step = 5;
	}
	else if (Step == 5 && GameTime > 20.0)
	{
		Log("DXNETSTAT: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
	}
}

// The mark for the original's screen grabber, as ViewWordsConsole's: the
// shot's number in its bits, white on black after a magenta square, above
// the panel's first line
event PostRender(canvas C)
{
	local int i;

	Super.PostRender(C);
	if (!bMarking)
		return;
	C.Style = 1;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		if ((1 & (1 << i)) != 0)
		{
			C.DrawColor.R = 255;
			C.DrawColor.G = 255;
			C.DrawColor.B = 255;
		}
		else
		{
			C.DrawColor.R = 0;
			C.DrawColor.G = 0;
			C.DrawColor.B = 0;
		}
		C.SetPos(12 + 12 * i, 0);
		C.DrawRect(Texture'Solid', 12, 12);
	}
}
