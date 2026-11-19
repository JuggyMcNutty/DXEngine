//=============================================================================
// SpeechMemConsole: the memory distinct speech takes (bugs.md AUD.26). From
// wherever the run starts, 2 s in: mission 1's conversation lines,
// ConAudioMission01_0 to ConAudioMission01_<Lines - 1>, played one after
// another on the player's talk slot, a quarter of a second each, each the
// one before cut short; then the first 20 again; 5 s after, OBJ GARBAGE,
// and an exit 5 s after that. With DXCAP_MEMLOG=1, the run's resident
// memory over it.
// "DXSPEECHMEM:" lines: every 50th line, each phase's start.
//=============================================================================
class SpeechMemConsole extends Console;

var config int Lines;

var float RunTime;
var float LineTime;
var int Phase;
var int Line;

function PlayLine(PlayerPawn P, int n)
{
	local Sound Speech;

	Speech = Sound(DynamicLoadObject("DeusExConAudioMission01.ConAudioMission01_" $ n, class'Sound'));
	if (n % 50 == 0)
		Log("DXSPEECHMEM: line " $ n $ " at " $ RunTime);
	if (Speech != None)
		P.PlaySound(Speech, SLOT_Talk,,, 65536.0);
	LineTime = 0.0;
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	LineTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		Log("DXSPEECHMEM: first pass of " $ Lines $ " lines at " $ RunTime);
		Line = 0;
		PlayLine(P, Line);
		Phase = 1;
	}
	else if ((Phase == 1 || Phase == 2) && LineTime > 0.25)
	{
		Line++;
		if (Phase == 1 && Line >= Lines)
		{
			Log("DXSPEECHMEM: the first 20 again at " $ RunTime);
			Line = 0;
			Phase = 2;
		}
		else if (Phase == 2 && Line >= 20)
		{
			Phase = 3;
			LineTime = 0.0;
			return;
		}
		PlayLine(P, Line);
	}
	else if (Phase == 3 && LineTime > 5.0)
	{
		Log("DXSPEECHMEM: obj garbage at " $ RunTime);
		P.ConsoleCommand("obj garbage");
		Phase = 4;
		LineTime = 0.0;
	}
	else if (Phase == 4 && LineTime > 5.0)
	{
		Log("DXSPEECHMEM: done, exiting at " $ RunTime);
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}

defaultproperties
{
	Lines=300
}
