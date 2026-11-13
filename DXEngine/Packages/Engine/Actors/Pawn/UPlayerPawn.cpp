
#include "Precomp.h"
#include "UPlayerPawn.h"
#include "UCamera.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Engine.h"
#include "Network/NetDemo.h"
#include "Package/PackageManager.h"
#include "Package/IniProperty.h"
#include "VM/ScriptCall.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/Properties/UFloatProperty.h"

// The pawn's Player's input read, as the original's Player->ReadInput reads
// it: a viewport's keys and mouse (UInput::ReadInput, Engine.dll
// 0x10394580); a net connection's nothing (UNetConnection::ReadInput,
// 0x1030262b), a server's copy of a remote player keeping its input as it is.
void UPlayerPawn::ReadPlayerInput(float elapsed)
{
	if (Player() == engine->viewport)
		engine->ReadInput(this, elapsed);
}

// A paused level's tick of a player pawn with a Player, as the original's
// (ULevel::Tick, Engine.dll 0x103a57e6-0x103a58a6), whatever its roles or
// class: its Player's input read, PlayerInput, its input floats cleared.
void UPlayerPawn::PausedInput(float elapsed)
{
	ReadPlayerInput(elapsed);
	CallEvent(this, EventName::PlayerInput, { ExpressionValue::FloatValue(elapsed) });
	ClearInput();
}

// Every input float (aBaseX, aTurn, aMouseX...) back to 0 once the script has
// read them, as the original's Player->ReadInput(-1.0) after PlayerTick
// (Engine.dll AActor::Tick 0x103a1aa0, UInput::ReadInput 0x10394580), and
// ULevel::Tick's own loop while paused (0x103a51a0). Its input bytes stay.
void UPlayerPawn::ClearInput()
{
	for (UProperty* prop : Class->Properties)
	{
		if (AllFlags(prop->PropFlags, PropertyFlags::Input) && UObject::IsType<UFloatProperty>(prop))
		{
			for (int i = 0; i < prop->ArrayDimension; i++)
				*reinterpret_cast<float*>(static_cast<uint8_t*>(PropertyData.Ptr(prop)) + i * prop->ElementPitch()) = 0.0f;
		}
	}
}

// Every input float times the scale, as the original's ReadInput scales them
// at each read with the tick's time
void UPlayerPawn::ScaleInput(float scale)
{
	for (UProperty* prop : Class->Properties)
	{
		if (AllFlags(prop->PropFlags, PropertyFlags::Input) && UObject::IsType<UFloatProperty>(prop))
		{
			for (int i = 0; i < prop->ArrayDimension; i++)
				*reinterpret_cast<float*>(static_cast<uint8_t*>(PropertyData.Ptr(prop)) + i * prop->ElementPitch()) *= scale;
		}
	}
}

// Deus Ex's (Engine.dll APlayerPawn::AddAdditionalView 0x103ba540): the
// actor into the first empty one of the three AdditionalViews, whether it
// is there already or not; whether it went in, None counting as in.
bool UPlayerPawn::AddAdditionalView(UActor* actor)
{
	if (!actor)
		return true;
	if (PropOffsets_PlayerPawn.AdditionalViews.DataOffset == ~(size_t)0)
		return false;
	for (UActor*& view : AdditionalViews())
	{
		if (!view)
		{
			view = actor;
			return true;
		}
	}
	return false;
}

// Deus Ex's (RemoveAdditionalView 0x103ba4c0): the first of the three that
// is the actor emptied and those after it moved down a place, the last
// left as it was.
void UPlayerPawn::RemoveAdditionalView(UActor* actor)
{
	if (!actor || PropOffsets_PlayerPawn.AdditionalViews.DataOffset == ~(size_t)0)
		return;
	auto views = AdditionalViews();
	for (size_t i = 0; i < views.size(); i++)
	{
		if (views[i] == actor)
		{
			views[i] = nullptr;
			for (size_t j = i; j + 1 < views.size(); j++)
				views[j] = views[j + 1];
			return;
		}
	}
}

bool UPlayerPawn::TickInput(float elapsed)
{
	// A server's copy of a client's pawn reads no input here; the client
	// does, and sends its moves. Told by its roles alone, as the original's
	// actor tick tells it (Engine.dll 0x103a23ce).
	bool clientsPawn = Role() == ROLE_Authority && RemoteRole() == ROLE_AutonomousProxy;
	if (Role() < ROLE_SimulatedProxy || clientsPawn || !Player() || UObject::TryCast<UCamera>(this))
		return false;

	ReadPlayerInput(elapsed);
	CallEvent(this, EventName::PlayerInput, { ExpressionValue::FloatValue(elapsed) });
	CallEvent(this, EventName::PlayerTick, { ExpressionValue::FloatValue(elapsed) });
	ClearInput();

	// A recording takes the view's pitch and yaw, which its playback gives
	// the player's view (AActor::Tick, Engine.dll 0x103a2616-0x103a263b).
	if (engine->LevelDemoDriver && !engine->LevelDemoDriver->ServerConnection && XLevel() == engine->Level)
	{
		DemoViewPitch() = ViewRotation().Pitch;
		DemoViewYaw() = ViewRotation().Yaw;
	}
	return true;
}

void UPlayerPawn::Tick(float elapsed)
{
	// Deus Ex's input is read in the actor's own tick, before its physics
	// (UActor::Tick).
	UPawn::Tick(elapsed);
}

void UPlayerPawn::TickRotating(float elapsed)
{
	if (Physics() == PHYS_Spider)
		return;

	// Deus Ex's, the original's APawn::physicsRotation for a player
	// (Engine.dll 0x103c9f30), which its performPhysics calls every tick
	// (0x103c9a70): only its roll is set to 0.
	if (Rotation().Roll != 0)
		SetRotation(Rotator(Rotation().Pitch, Rotation().Yaw, 0));
}

void UPlayerPawn::SaveConfig()
{
	UPawn::SaveConfig();

	// UObject::SaveConfig has saved every config and globalconfig property,
	// WeaponPriority's entries among them; these are written again as before.
	engine->packages->SetIniValue("user", "Engine.PlayerPawn", "bInvertMouse", IniPropertyConverter<bool>::ToString(bInvertMouse()));
	engine->packages->SetIniValue("user", "Engine.PlayerPawn", "MouseSensitivity", IniPropertyConverter<float>::ToString(MouseSensitivity()));
	engine->packages->SetIniValue("user", "Engine.PlayerPawn", "DodgeClickTime", IniPropertyConverter<float>::ToString(DodgeClickTime()));
	engine->packages->SetIniValue("user", "Engine.PlayerPawn", "Bob", IniPropertyConverter<float>::ToString(Bob()));
	engine->packages->SetIniValue("user", "Engine.PlayerPawn", "Handedness", IniPropertyConverter<float>::ToString(Handedness()));
	engine->packages->SetIniValue("user", "Engine.PlayerPawn", "bLookUpStairs", IniPropertyConverter<bool>::ToString(bLookUpStairs()));
	engine->packages->SetIniValue("user", "Engine.PlayerPawn", "bSnapToLevel", IniPropertyConverter<bool>::ToString(bSnapToLevel()));
	engine->packages->SetIniValue("user", "Engine.PlayerPawn", "bAlwaysMouseLook", IniPropertyConverter<bool>::ToString(bAlwaysMouseLook()));
	engine->packages->SetIniValue("user", "Engine.PlayerPawn", "bKeyboardLook", IniPropertyConverter<bool>::ToString(bKeyboardLook()));
	engine->packages->SetIniValue("user", "Engine.PlayerPawn", "bMaxMouseSmoothing", IniPropertyConverter<bool>::ToString(bMaxMouseSmoothing()));
	engine->packages->SetIniValue("user", "Engine.PlayerPawn", "bNoVoices", IniPropertyConverter<bool>::ToString(bNoVoices()));
	engine->packages->SetIniValue("user", "Engine.PlayerPawn", "bMessageBeep", IniPropertyConverter<bool>::ToString(bMessageBeep()));
	engine->packages->SetIniValue("user", "Engine.PlayerPawn", "MouseSmoothThreshold", IniPropertyConverter<float>::ToString(MouseSmoothThreshold()));
	engine->packages->SetIniValue("user", "Engine.PlayerPawn", "ngWorldSecret", ScriptToUTF8(ngWorldSecret()));
}
