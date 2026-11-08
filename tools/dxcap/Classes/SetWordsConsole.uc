//=============================================================================
// SetWordsConsole: SET's edges on the client and the devices, in both
// engines: on the map the run starts on, 3 s in, each SET's text, what the
// SET answered and the GET's answer after it, each line between bars with
// "DXSETW:" in front, then an exit. With DXCAP_KILL=1 the run's
// ini-after.ini shows what each SET wrote. Run the original with
// DXCAP_RENDERER=D3D, so both engines read [D3DDrv.D3DRenderDevice].
//
// bugs.md D.55, D.62, ENG.29, ENG.32. Galaxy's OutputRate is an enum of
// seven names, read by the first token in any case, never from a number
// (Galaxy.dll 0x10604be0, Core.dll 0x10165aa0); an enum's other text, a
// bool's or an int's word, is refused and the value kept; a float's is
// appAtof's, 0 for a word; a name the class has not answers "Unrecognized
// property" (Core.dll 0x101531b0).
//=============================================================================
class SetWordsConsole extends Console;

var float RunTime;
var int Phase;

function Try(PlayerPawn P, string Setting, string Key, string Text)
{
	local string Answer;

	Answer = P.ConsoleCommand("set ini:Engine.Engine." $ Setting $ " " $ Key $ " " $ Text);
	Log("DXSETW: " $ Key $ " set |" $ Text $ "| said |" $ Answer $ "| get |" $ P.ConsoleCommand("get ini:Engine.Engine." $ Setting $ " " $ Key) $ "|");
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Phase != 0 || RunTime < 3.0)
		return;

	Log("DXSETW: OutputRate start |" $ P.ConsoleCommand("get ini:Engine.Engine.AudioDevice OutputRate") $ "|");
	Try(P, "AudioDevice", "OutputRate", "22050hz");
	Try(P, "AudioDevice", "OutputRate", "12345Hz");
	Try(P, "AudioDevice", "OutputRate", "44100");
	Try(P, "AudioDevice", "OutputRate", "48000Hz junk");
	Try(P, "AudioDevice", "OutputRate", "\"16000Hz\"");
	Try(P, "AudioDevice", "OutputRate", "11025Hz,");
	Try(P, "AudioDevice", "OutputRate", "8000HZ");
	Try(P, "AudioDevice", "OutputRate", "44100Hz");
	Try(P, "AudioDevice", "Latency", "abc");
	Try(P, "AudioDevice", "UseStereo", "Yes");
	Try(P, "AudioDevice", "UseDirectSound", "False");
	Try(P, "AudioDevice", "UseSpatial", "False");
	Try(P, "AudioDevice", "Bogus", "1");
	Try(P, "AudioDevice", "MusicVolume", "153");
	Try(P, "ViewportManager", "Decals", "1");
	Try(P, "ViewportManager", "UseJoystick", "Yes");
	Try(P, "ViewportManager", "MinDesiredFrameRate", "1.5");
	Try(P, "ViewportManager", "MipFactor", "2.5");
	Try(P, "ViewportManager", "CaptureMouse", "False");
	Try(P, "ViewportManager", "ScaleXYZ", "abc");
	Try(P, "ViewportManager", "TextureDetail", "0");
	Try(P, "ViewportManager", "TextureDetail", "medium");
	Try(P, "ViewportManager", "TextureDetail", "High");
	Try(P, "ViewportManager", "SkinDetail", "UltraLow junk");
	Try(P, "ViewportManager", "SkinDetail", "High");
	Try(P, "ViewportManager", "Brightness", "abc");
	Try(P, "ViewportManager", "Brightness", "0.6");
	Try(P, "ViewportManager", "Bogus", "1");
	Try(P, "GameRenderDevice", "UseVSync", "True");
	Try(P, "GameRenderDevice", "Translucency", "False");
	Try(P, "GameRenderDevice", "Coronas", "No");
	Try(P, "GameRenderDevice", "Description", "Words");

	Log("DXSETW: done, exiting");
	P.ConsoleCommand("exit");
	Phase = 99;
}
