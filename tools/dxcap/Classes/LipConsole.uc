//=============================================================================
// LipConsole: lip sync's mouth shapes, logged in both engines
// (docs/DEVELOPMENT.md, scripted runs). From wherever the run starts, 2 s
// in: the player made to speak three of mission 1's conversation lines in
// turn, as ConPlay plays a line (bIsSpeaking set, the sound on its talk
// slot with a radius of 65536), each for 8 s: the original's
// GetSoundDuration is 0 for an MP3.
// Every shape the audio writes to the player's nextPhoneme is logged, in
// order, with the time since its line began; the console empties
// nextPhoneme after each read, so a shape written again shows again.
// "DXLIP:" lines; a line's shapes are compared by their order. With Beep
// (DXCAP_CONFIG="Beep=True"), a beep on the same tick as each line, so that
// a recording (DXCAP_RECORD=1) gives each line's start against it. With
// Speech, a sound's full name, that line first, then the three.
//=============================================================================
class LipConsole extends Console;

// The harness's own sounds (tools/dxcap/testsounds.py), for Speech
#exec AUDIO IMPORT FILE="Sounds\CapPingPong.wav" NAME="CapPingPong"
#exec AUDIO IMPORT FILE="Sounds\CapLoop.wav" NAME="CapLoop"
#exec AUDIO IMPORT FILE="Sounds\CapEarlyLoop.wav" NAME="CapEarlyLoop"
#exec AUDIO IMPORT FILE="Sounds\Cap8Bit.wav" NAME="Cap8Bit"
#exec AUDIO IMPORT FILE="Sounds\CapStereo.wav" NAME="CapStereo"

var config bool Beep;
var config string Speech;

var float RunTime;
var float LineTime;
var int Phase;
var int Line;
var int Shapes;

function string LineName(int i)
{
	if (Speech != "")
	{
		if (i == 0)
			return Speech;
		i--;
	}
	switch (i)
	{
		case 0: return "DeusExConAudioMission01.ConAudioMission01_3";
		case 1: return "DeusExConAudioMission01.ConAudioMission01_10";
		case 2: return "DeusExConAudioMission01.ConAudioMission01_25";
	}
	return "";
}

function StartLine(PlayerPawn P)
{
	local Sound Speech;

	Speech = Sound(DynamicLoadObject(LineName(Line), class'Sound'));
	Log("DXLIP: line " $ Line $ " " $ LineName(Line));
	P.nextPhoneme = "";
	P.bIsSpeaking = True;
	P.bWasSpeaking = True;
	if (Beep)
		P.PlaySound(Sound'DeusExSounds.Generic.Beep4', SLOT_Misc,,, 65536.0);
	P.PlaySound(Speech, SLOT_Talk,,, 65536.0);
	LineTime = 0.0;
	Shapes = 0;
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		StartLine(P);
		Phase = 1;
	}
	else if (Phase == 1)
	{
		LineTime += Delta;
		if (P.nextPhoneme != "")
		{
			Log("DXLIP: line " $ Line $ " shape " $ Shapes $ " at " $ LineTime $ ": " $ P.nextPhoneme);
			P.nextPhoneme = "";
			Shapes++;
		}
		if (LineTime > 8.0)
		{
			P.bIsSpeaking = False;
			Log("DXLIP: line " $ Line $ " done, " $ Shapes $ " shapes");
			Line++;
			if (LineName(Line) != "")
			{
				StartLine(P);
			}
			else
			{
				Log("DXLIP: done, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
			}
		}
	}
}
