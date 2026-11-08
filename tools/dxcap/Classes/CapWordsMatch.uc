//=============================================================================
// CapWordsMatch: CapDeathMatch with exec functions named as the window's, the
// render device's, the viewport's and the client's words, for WordsServer
// (DXMP_Cathedral?game=DXCapture.CapWordsMatch). Each logs that it ran, with
// "DXSWORDS:". The original offers a command to the game's exec functions
// before the engine's own words -- an actor's ConsoleCommand in
// UGameEngine::Exec (Engine.dll 0x1038a030, after its OPEN to CANCEL and the
// level's words), a remote player's in UPlayer::Exec (0x103d7ed0, after its
// connection's GETPING and GETLOSS and the level's words), the executor the
// player there, none in the first -- so on a dedicated server, which has no
// viewport, client or render device to take a word first, each of these
// runs. DxWho names its executor.
//=============================================================================
class CapWordsMatch extends CapDeathMatch;

exec function Shot()
{
	Log("DXSWORDS: the game's exec Shot ran");
}

exec function Flush()
{
	Log("DXSWORDS: the game's exec Flush ran");
}

exec function Brightness()
{
	Log("DXSWORDS: the game's exec Brightness ran");
}

exec function ToggleFullscreen()
{
	Log("DXSWORDS: the game's exec ToggleFullscreen ran");
}

exec function GetRes()
{
	Log("DXSWORDS: the game's exec GetRes ran");
}

exec function GetCurrentRes()
{
	Log("DXSWORDS: the game's exec GetCurrentRes ran");
}

exec function GetColorDepths()
{
	Log("DXSWORDS: the game's exec GetColorDepths ran");
}

exec function GetDetailTextures()
{
	Log("DXSWORDS: the game's exec GetDetailTextures ran");
}

exec function KeyName(optional int I)
{
	Log("DXSWORDS: the game's exec KeyName ran, " $ I);
}

exec function GetPing()
{
	Log("DXSWORDS: the game's exec GetPing ran");
}

exec function DxWho(optional PlayerPawn Who)
{
	Log("DXSWORDS: the game's exec DxWho ran, executor " $ Who);
}
