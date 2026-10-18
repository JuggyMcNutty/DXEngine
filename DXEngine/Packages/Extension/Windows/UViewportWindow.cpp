
#include "Precomp.h"
#include "UViewportWindow.h"
#include "Engine.h"
#include "Render/RenderSubsystem.h"

void UViewportWindow::ClearZBuffer(std::optional<bool> bClear)
{
	bClearZ() = bClear.has_value() ? bClear.value() : true;
}

void UViewportWindow::EnableViewport(std::optional<bool> bEnable)
{
	bEnableViewport() = bEnable.has_value() ? bEnable.value() : true;
}

void UViewportWindow::SetDefaultTexture(std::optional<UObject*> NewTexture, std::optional<Color> NewColor)
{
	if (NewTexture)
		DefaultTexture() = UObject::Cast<UTexture>(*NewTexture);
	if (NewColor)
		DefaultColor() = *NewColor;
}

// 90 unless given, from 0.1 to 179, as the original's (Extension.dll
// 0x1004a660)
void UViewportWindow::SetFOVAngle(std::optional<float> newAngle)
{
	FOV() = std::clamp(newAngle.value_or(90.0f), 0.1f, 179.0f);
}

void UViewportWindow::SetRelativeLocation(std::optional<vec3> relLoc)
{
	if (relLoc.has_value())
		relLocation() = relLoc.value();
}

void UViewportWindow::SetRelativeRotation(std::optional<Rotator> relRot)
{
	if (relRot.has_value())
		relRotation() = relRot.value();
}

void UViewportWindow::SetRotation(std::optional<Rotator> NewRotation)
{
	if (NewRotation.has_value())
		Rotation() = NewRotation.value();
}

// Eye level and enabled unless told otherwise, the window's own location
// cleared, as the original's (Extension.dll 0x1004a170)
void UViewportWindow::SetViewportActor(std::optional<UObject*> newOriginActor, std::optional<bool> bEyeLevel, std::optional<bool> bEnable)
{
	originActor() = UObject::Cast<UActor>(newOriginActor.value_or(nullptr));
	bUseEyeHeight() = bEyeLevel.value_or(true);
	bEnableViewport() = bEnable.value_or(true);
	Location() = vec3(0.0f);
	relLocation() = vec3(0.0f);
}

// Enabled unless told otherwise; the origin actor dropped (0x1004a300)
void UViewportWindow::SetViewportLocation(const vec3& NewLocation, std::optional<bool> bEnable)
{
	Location() = NewLocation;
	relLocation() = vec3(0.0f);
	originActor() = nullptr;
	bEnableViewport() = bEnable.value_or(true);
}

// At eye level unless told otherwise; the window's own rotation cleared
// (0x1004a400)
void UViewportWindow::SetWatchActor(std::optional<UObject*> newWatchActor, std::optional<bool> bEyeLevel)
{
	watchActor() = UObject::Cast<UActor>(newWatchActor.value_or(nullptr));
	bWatchEyeHeight() = bEyeLevel.value_or(true);
	Rotation() = Rotator(0, 0, 0);
	relRotation() = Rotator(0, 0, 0);
}

void UViewportWindow::ShowViewportActor(std::optional<bool> bShow)
{
	bShowActor() = !bShow || *bShow;
}

void UViewportWindow::ShowWeapons(std::optional<bool> bShow)
{
	bShowWeapons() = !bShow || *bShow;
}

// The original's Init (Extension.dll 0x10049f10): enabled, the depth
// cleared, a player's view rotation and eye heights used, a field of view
// of 90, black, and the player's own view at eye level. The script gives a
// ViewportWindow no defaults.
void UViewportWindow::InitDefaults()
{
	UWindow::InitDefaults();
	bEnableViewport() = true;
	bClearZ() = true;
	bUseViewRotation() = true;
	bUseEyeHeight() = true;
	bWatchEyeHeight() = true;
	FOV() = 90.0f;
	DefaultColor() = { 0, 0, 0, 255 };
	SetViewportActor(GetPlayerPawn(), true, true);
}

void UViewportWindow::DrawWindow(UGC* gc)
{
	if (bEnableViewport())
	{
		engine->render->DrawViewport(this);
	}

	UWindow::DrawWindow(gc);
}
