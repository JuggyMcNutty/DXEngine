//=============================================================================
// WordsServer: a dedicated server's answer to the window's, the render
// device's, the viewport's and the engine's words (bugs.md WIN.20,
// WIN.21, WIN.22). One of the server's ServerActors:
//
//   tools/dxcap.sh dedicated <engine> WordsServer \
//       'DXMP_Cathedral?game=DXCapture.CapDeathMatch' 90
//   (and again with ?game=DXCapture.CapWordsMatch, whose exec functions are
//   named as the words), with a client: tools/dxcap.sh dxe JoinConsole DX.dx
//   (or original JoinConsole) once UDP 7790 is bound.
//
// Logged with "DXSWORDS:".
//   1. 3 s into the map: a CapHearer spawned (a MessagingSpectator that logs
//      each message it is sent), then each of ActorWords by this actor's own
//      ConsoleCommand -- the original's AActor::execConsoleCommand, which is
//      the engine's Exec (Engine.dll 0x103ded00; UGameEngine::Exec
//      0x1038a030, UEngine::Exec 0x10380ee0) -- each word and what it gave.
//   2. Once a player has been in 5 s: its replication info's Ping and
//      PacketLoss, then each of PlayerWords by the player's own
//      ConsoleCommand -- the original's APlayerPawn::execConsoleCommand, its
//      Player's Exec: a remote player's UNetConnection::Exec, then
//      UPlayer::Exec (0x103ba200, 0x10401c50, 0x103d7ed0).
//   3. 4 s on, the Ping and PacketLoss again, the player's clipboard natives
//      (bugs.md WIN.24: the original's Core's clipboard, appClipboardCopy and
//      appClipboardPaste from APlayerPawn's natives, Engine.dll 0x103df580,
//      0x103df700): "DxSwordsClip" copied and pasted back, then an exit.
//      DXEngine 9519eada's server faults there, having no window: run the
//      build before last.
//
// What the original's gives:
//   1. every word "" (none takes it: no client, render device or viewport on
//      a dedicated server), but GETCURRENTTICKRATE and GETMAXTICKRATE ("%f"),
//      and SAY broadcast: the hearer's "heard 'DxSwordsActorSay' type Event
//      beep True". With CapWordsMatch each of its exec functions of a word's
//      name runs (the game's exec functions come before UEngine::Exec), DxWho
//      given None.
//   2. GETPING the connection's best lag in ms (some 50 on a 20 Hz server,
//      the ack read at its next tick), GETLOSS its loss; every other word "";
//      SAY the player's own PlayerPawn.Say (the server's "Say>" line, every
//      player told); with CapWordsMatch DxWho given the player, and Shot,
//      Flush, Brightness run. The replication info's Ping some 50.
// DXEngine 6d100090: in 1 and 2 alike the window's and device's
// words "" without the exec functions, KEYNAME 65 "A", KEYBINDING Escape
// "ShowMainMenu", GETCOLORDEPTHS "32 16", GETCURRENTCOLORDEPTH "32", GETPING
// and GETLOSS "0", SAY and GETCURRENTTICKRATE "Unknown command"; DxWho
// given None in 2; the replication info's Ping 0.
//=============================================================================
class WordsServer extends Info;

var string ActorWords[40];
var string PlayerWords[40];
var float MapTime, InTime, StepTime;
var int Step;
var DeusExPlayer Joined;
var CapHearer Hearer;

function Ask(string Who, Actor A, string W)
{
	local string R;

	if (W == "")
		return;
	R = A.ConsoleCommand(W);
	Log("DXSWORDS: " $ Who $ " '" $ W $ "' gave '" $ R $ "'");
}

function LogPing(string When)
{
	Log("DXSWORDS: " $ When $ ": " $ Joined.PlayerReplicationInfo.PlayerName $ " ping " $ Joined.PlayerReplicationInfo.Ping $ " loss " $ Joined.PlayerReplicationInfo.PacketLoss);
}

event Tick(float Delta)
{
	local DeusExPlayer P;
	local int i;

	MapTime += Delta;
	if (Step == 0 && MapTime > 3.0)
	{
		Log("DXSWORDS: net mode " $ Level.NetMode $ ", game " $ Level.Game.Class);
		Hearer = Spawn(class'CapHearer');
		Log("DXSWORDS: hearer " $ Hearer);
		for (i = 0; i < ArrayCount(ActorWords); i++)
			Ask("actor", Self, ActorWords[i]);
		Step = 1;
	}
	else if (Step == 1)
	{
		if (Joined == None)
		{
			foreach AllActors(class'DeusExPlayer', P)
			{
				if (P.PlayerReplicationInfo != None)
					Joined = P;
			}
		}
		if (Joined != None)
		{
			InTime += Delta;
			if (InTime > 5.0)
			{
				LogPing("in 5 s");
				for (i = 0; i < ArrayCount(PlayerWords); i++)
					Ask("player", Joined, PlayerWords[i]);
				Step = 2;
				StepTime = 0;
			}
		}
		else if (MapTime > 150.0)
		{
			Log("DXSWORDS: no player came in, exiting");
			ConsoleCommand("exit");
			Step = 9;
		}
	}
	else if (Step == 2)
	{
		StepTime += Delta;
		if (StepTime > 4.0)
		{
			if (Joined != None)
			{
				LogPing("in 9 s");
				Joined.CopyToClipboard("DxSwordsClip");
				Log("DXSWORDS: clipboard '" $ Joined.PasteFromClipboard() $ "'");
			}
			Log("DXSWORDS: exiting");
			ConsoleCommand("exit");
			Step = 9;
		}
	}
}

defaultproperties
{
	ActorWords(0)="GETRES"
	ActorWords(1)="GETCURRENTRES"
	ActorWords(2)="GETCOLORDEPTHS"
	ActorWords(3)="GETCURRENTCOLORDEPTH"
	ActorWords(4)="GETCURRENTRENDERDEVICE"
	ActorWords(5)="SETRES 800x600"
	ActorWords(6)="TOGGLEFULLSCREEN"
	ActorWords(7)="ENDFULLSCREEN"
	ActorWords(8)="SHOT"
	ActorWords(9)="FLUSH"
	ActorWords(10)="STAT FPS"
	ActorWords(11)="BRIGHTNESS"
	ActorWords(12)="LODBIAS 1"
	ActorWords(13)="SHOWPOOLS"
	ActorWords(14)="GETDETAILTEXTURES"
	ActorWords(15)="TOGGLEDETAILTEXTURES"
	ActorWords(16)="GETPING"
	ActorWords(17)="GETLOSS"
	ActorWords(18)="KEYNAME 65"
	ActorWords(19)="KEYBINDING Escape"
	ActorWords(20)="NETSPEED 9000"
	ActorWords(21)="EXEC DxSwordsNone.txt"
	ActorWords(22)="GETCURRENTTICKRATE"
	ActorWords(23)="GETMAXTICKRATE"
	ActorWords(24)="SAY DxSwordsActorSay"
	ActorWords(25)="DxWho"
	PlayerWords(0)="GETPING"
	PlayerWords(1)="GETLOSS"
	PlayerWords(2)="GETRES"
	PlayerWords(3)="GETCURRENTRES"
	PlayerWords(4)="GETCOLORDEPTHS"
	PlayerWords(5)="GETCURRENTCOLORDEPTH"
	PlayerWords(6)="TOGGLEFULLSCREEN"
	PlayerWords(7)="SHOT"
	PlayerWords(8)="FLUSH"
	PlayerWords(9)="BRIGHTNESS"
	PlayerWords(10)="LODBIAS 1"
	PlayerWords(11)="GETDETAILTEXTURES"
	PlayerWords(12)="KEYNAME 65"
	PlayerWords(13)="KEYBINDING Escape"
	PlayerWords(14)="SAY DxSwordsPlayerSay"
	PlayerWords(15)="DxWho"
	RemoteRole=ROLE_None
}
