//=============================================================================
// FallbackConsole: the client's and the audio and render devices' settings
// as their classes hold them, by GET, in both engines: what each takes for a
// key the ini lacks (DXCAP_INI_DROP, a switch to add) or holds text its type
// does not read (DXCAP_INI with "x"). On the map the run starts on, 3 s in,
// every config property of the original's UClient and UWindowsClient
// (ini:Engine.Engine.ViewportManager), UGalaxyAudioSubsystem
// (ini:Engine.Engine.AudioDevice) and URenderDevice and UD3DRenderDevice
// (ini:Engine.Engine.GameRenderDevice), and UseSpatial and Translucency,
// which the game's ini names and no class has, each answer logged between bars with
// "DXFALL:" in front, then an exit. Run the original with
// DXCAP_RENDERER=D3D, so both engines read [D3DDrv.D3DRenderDevice].
//
// bugs.md ENG.30, ENG.32, D.50, D.62. The original's classes set no default
// for any of these (their StaticConstructors and constructors set none; the
// class default object is zero-filled past its base's, Core.dll
// InitProperties 0x101520d0), so a key not there, or an int, byte, enum or
// bool it cannot read, leaves 0, False, High or 8000Hz; DXEngine's leave its
// own member defaults.
//=============================================================================
class FallbackConsole extends Console;

var float RunTime;
var int Phase;

function Ask(PlayerPawn P, string Setting, string Key)
{
	Log("DXFALL: " $ Key $ " |" $ P.ConsoleCommand("get ini:Engine.Engine." $ Setting $ " " $ Key) $ "|");
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

	// UClient (Engine.dll 0x10375010) and UWindowsClient (WinDrv.dll 0x111015f0)
	Ask(P, "ViewportManager", "WindowedViewportX");
	Ask(P, "ViewportManager", "WindowedViewportY");
	Ask(P, "ViewportManager", "WindowedColorBits");
	Ask(P, "ViewportManager", "FullscreenViewportX");
	Ask(P, "ViewportManager", "FullscreenViewportY");
	Ask(P, "ViewportManager", "FullscreenColorBits");
	Ask(P, "ViewportManager", "MipFactor");
	Ask(P, "ViewportManager", "Brightness");
	Ask(P, "ViewportManager", "CaptureMouse");
	Ask(P, "ViewportManager", "CurvedSurfaces");
	Ask(P, "ViewportManager", "TextureDetail");
	Ask(P, "ViewportManager", "SkinDetail");
	Ask(P, "ViewportManager", "ScreenFlashes");
	Ask(P, "ViewportManager", "NoLighting");
	Ask(P, "ViewportManager", "MinDesiredFrameRate");
	Ask(P, "ViewportManager", "Decals");
	Ask(P, "ViewportManager", "NoDynamicLights");
	Ask(P, "ViewportManager", "UseDirectDraw");
	Ask(P, "ViewportManager", "UseDirectInput");
	Ask(P, "ViewportManager", "UseJoystick");
	Ask(P, "ViewportManager", "StartupFullscreen");
	Ask(P, "ViewportManager", "SlowVideoBuffering");
	Ask(P, "ViewportManager", "DeadZoneXYZ");
	Ask(P, "ViewportManager", "DeadZoneRUV");
	Ask(P, "ViewportManager", "InvertVertical");
	Ask(P, "ViewportManager", "ScaleXYZ");
	Ask(P, "ViewportManager", "ScaleRUV");

	// UGalaxyAudioSubsystem (Galaxy.dll 0x10604bb0), and UseSpatial, which it has not
	Ask(P, "AudioDevice", "UseDirectSound");
	Ask(P, "AudioDevice", "UseFilter");
	Ask(P, "AudioDevice", "UseSurround");
	Ask(P, "AudioDevice", "UseStereo");
	Ask(P, "AudioDevice", "UseCDMusic");
	Ask(P, "AudioDevice", "UseDigitalMusic");
	Ask(P, "AudioDevice", "UseReverb");
	Ask(P, "AudioDevice", "Use3dHardware");
	Ask(P, "AudioDevice", "ReverseStereo");
	Ask(P, "AudioDevice", "LowSoundQuality");
	Ask(P, "AudioDevice", "Latency");
	Ask(P, "AudioDevice", "OutputRate");
	Ask(P, "AudioDevice", "EffectsChannels");
	Ask(P, "AudioDevice", "MusicVolume");
	Ask(P, "AudioDevice", "SoundVolume");
	Ask(P, "AudioDevice", "SpeechVolume");
	Ask(P, "AudioDevice", "AmbientFactor");
	Ask(P, "AudioDevice", "DopplerSpeed");
	Ask(P, "AudioDevice", "UseSpatial");

	// URenderDevice (Engine.dll 0x103713c0) and UD3DRenderDevice (D3DDrv.dll
	// 0x10001880), and Translucency, which neither has
	Ask(P, "GameRenderDevice", "VolumetricLighting");
	Ask(P, "GameRenderDevice", "ShinySurfaces");
	Ask(P, "GameRenderDevice", "Coronas");
	Ask(P, "GameRenderDevice", "HighDetailActors");
	Ask(P, "GameRenderDevice", "DetailTextures");
	Ask(P, "GameRenderDevice", "UsePrecache");
	Ask(P, "GameRenderDevice", "UseVSync");
	Ask(P, "GameRenderDevice", "UseTrilinear");
	Ask(P, "GameRenderDevice", "Translucency");

	Log("DXFALL: done, exiting");
	P.ConsoleCommand("exit");
	Phase = 99;
}
