#pragma once

enum class RenderDeviceType
{
	Vulkan,
	D3D11,
	OpenGL,
	GLES,
	Software
};

enum class AntialiasMode
{
	Off,
	MSAA2x,
	MSAA4x
};

enum class LightMode
{
	Normal,
	OneX,
	BrighterActors
};

enum class GammaMode
{
	D3D9,
	XOpenGL
};

class LauncherSettings
{
public:
	static LauncherSettings& Get();

	struct
	{
		RenderDeviceType Type = RenderDeviceType::Vulkan;
		bool TypeNamed = false; // Settings.json names Type: else the game's ini chooses (Engine::ChooseRenderDevice)
		bool CPUDriver = false; // the Vulkan or GL device on Mesa's CPU driver, lavapipe or llvmpipe
		bool UseVSync = true;
		AntialiasMode Antialias = AntialiasMode::MSAA4x;
		LightMode Light = LightMode::Normal;
		GammaMode Gamma = GammaMode::D3D9;
		bool GammaCorrectScreenshots = false;
		bool Hdr = false;
		int HdrScale = 128;
		bool Bloom = false;
		int BloomAmount = 128;
		bool UseDebugLayer = false;
		// The present's colour correction, 1 each its picture unchanged, and
		// its gamma's offsets, all of them and each channel's; the textures'
		// mip bias
		float Contrast = 1.0f;
		float Saturation = 1.0f;
		float GammaOffset = 0.0f;
		float GammaOffsetRed = 0.0f;
		float GammaOffsetGreen = 0.0f;
		float GammaOffsetBlue = 0.0f;
		float LODBias = -0.5f;
	} RenderDevice;

	// Controller support (Engine::UpdateGamepad).
	struct
	{
		bool Enabled = true;
		float DeadZone = 0.2f;
		float LookSensitivityX = 1.0f;
		float LookSensitivityY = 1.0f;
		bool InvertY = false;
		float CursorSpeed = 1.0f;
	} Gamepad;

	// Speed for slow hardware.
	struct
	{
		float RenderScale = 1.0f; // the scene's size as a fraction of the window's (RenderDevice::GetRenderScale)
	} Performance;

private:
	LauncherSettings();
};
