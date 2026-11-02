
#include "Precomp.h"
#include "UViewportWindow.h"
#include "UGC.h"
#include "Engine.h"
#include "Render/RenderSubsystem.h"
#include "VM/ScriptCall.h"
#include "Packages/Core/UFunction.h"
#include "Packages/Core/Properties/UStructProperty.h"
#include "Packages/Core/Properties/UObjectProperty.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"

void UViewportWindow::ClearZBuffer(std::optional<bool> bClear)
{
	bClearZ() = bClear.has_value() ? bClear.value() : true;
}

void UViewportWindow::EnableViewport(std::optional<bool> bEnable)
{
	bEnableViewport() = bEnable.has_value() ? bEnable.value() : true;
}

// Both set, as the original's (Extension.dll 0x1004a730, its native
// 0x1004b930): no texture and white unless given
void UViewportWindow::SetDefaultTexture(std::optional<UObject*> NewTexture, std::optional<Color> NewColor)
{
	DefaultTexture() = UObject::Cast<UTexture>(NewTexture.value_or(nullptr));
	DefaultColor() = NewColor.value_or(Color{ 255, 255, 255, 255 });
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

// The rotation given, none unless given; relRotation cleared and the watch
// actor dropped, its reference let go; the last turn the new one -- as the
// original's (0x1004a570)
void UViewportWindow::SetRotation(std::optional<Rotator> NewRotation)
{
	Rotation() = NewRotation.value_or(Rotator(0, 0, 0));
	relRotation() = Rotator(0, 0, 0);
	if (watchActor())
	{
		RemoveActorRef(watchActor());
		watchActor() = nullptr;
	}
	lastRotation() = Rotation();
}

// Eye level and enabled unless told otherwise, the window's own location
// cleared, as the original's (Extension.dll 0x1004a170). A new origin actor
// takes a reference in the player's list (AddActorRef) and the old one's is
// let go; the new one becomes one of the player's AdditionalViews unless it
// is the player, and the old one no longer is (APlayerPawn's
// AddAdditionalView and RemoveAdditionalView, Engine.dll 0x103ba540 and
// 0x103ba4c0). The last place is the actor's, or the window's own.
void UViewportWindow::SetViewportActor(std::optional<UObject*> newOriginActor, std::optional<bool> bEyeLevel, std::optional<bool> bEnable)
{
	UActor* actor = UObject::Cast<UActor>(newOriginActor.value_or(nullptr));
	bUseEyeHeight() = bEyeLevel.value_or(true);
	bEnableViewport() = bEnable.value_or(true);
	Location() = vec3(0.0f);
	relLocation() = vec3(0.0f);
	if (originActor() != actor)
	{
		if (UPlayerPawn* player = UObject::TryCast<UPlayerPawn>(GetPlayerPawn()))
		{
			player->RemoveAdditionalView(originActor());
			if (player != actor)
				player->AddAdditionalView(actor);
		}
		AddActorRef(actor);
		RemoveActorRef(originActor());
		originActor() = actor;
	}
	lastLocation() = originActor() ? originActor()->Location() : Location();
}

// Enabled unless told otherwise; the origin actor dropped, its reference let
// go -- it stays one of the player's AdditionalViews, which only
// SetViewportActor changes --; the last place the new one (0x1004a300)
void UViewportWindow::SetViewportLocation(const vec3& NewLocation, std::optional<bool> bEnable)
{
	bEnableViewport() = bEnable.value_or(true);
	Location() = NewLocation;
	relLocation() = vec3(0.0f);
	if (originActor())
	{
		RemoveActorRef(originActor());
		originActor() = nullptr;
	}
	lastLocation() = Location();
}

// At eye level unless told otherwise; the window's own rotation cleared; a
// new watch actor takes a reference and the old one's is let go; the last
// turn is to the watch actor from the last place, or the window's own
// (0x1004a400)
void UViewportWindow::SetWatchActor(std::optional<UObject*> newWatchActor, std::optional<bool> bEyeLevel)
{
	UActor* actor = UObject::Cast<UActor>(newWatchActor.value_or(nullptr));
	bWatchEyeHeight() = bEyeLevel.value_or(true);
	Rotation() = Rotator(0, 0, 0);
	relRotation() = Rotator(0, 0, 0);
	if (watchActor() != actor)
	{
		AddActorRef(actor);
		RemoveActorRef(watchActor());
		watchActor() = actor;
	}
	lastRotation() = watchActor() ? Rotator::FromVector(watchActor()->Location() - lastLocation()) : Rotation();
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

// The original's CleanUp (Extension.dll 0x1004a070): no origin actor, its
// reference and its additional view let go. The watch actor's reference is
// kept, as the original keeps it.
void UViewportWindow::DestroyWindow()
{
	SetViewportActor(nullptr, true, true);
	UWindow::DestroyWindow();
}

// The original's Draw (Extension.dll 0x1004a770): the frame first
// (RenderSubsystem::DrawViewport); with none drawn -- the window turned off,
// or no pixel of it in view -- the window is tiled with its default texture
// from its corner, in its default colour and the normal style, the GC put
// back after (0x1004ae80); none tiles nothing, as the GC's DrawPattern
// draws nothing without a texture (0x10028a60). Then the script's
// DrawWindow, over either.
void UViewportWindow::DrawWindow(UGC* gc)
{
	if (!engine->render->DrawViewport(this))
	{
		engine->render->PushWindowGC(this);
		gc->SetStyle(EDrawStyle::Normal);
		gc->SetTileColor(DefaultColor());
		gc->DrawPattern(0.0f, 0.0f, Width(), Height(), 0.0f, 0.0f, DefaultTexture());
		engine->render->PopWindowGC();
	}

	UWindow::DrawWindow(gc);
}

// The original's CalcView (Extension.dll 0x1004b180): the script's CalcView
// event, given the origin and watch actors and the frame's place and turn,
// which it may change. ViewportWindow's own does nothing.
void UViewportWindow::CalcView(vec3& location, Rotator& rotation)
{
	UFunction* func = FindEventFunction(this, "CalcView");
	if (!func || func->Properties.size() < 4)
		return;
	if (!engine->vecprop->Struct)
		if (auto structProp = UObject::TryCast<UStructProperty>(func->Properties[2]))
			engine->vecprop->Struct = structProp->Struct;
	if (!engine->rotprop->Struct)
		if (auto structProp = UObject::TryCast<UStructProperty>(func->Properties[3]))
			engine->rotprop->Struct = structProp->Struct;
	CallEvent(this, "CalcView", {
		ExpressionValue::ObjectValue(originActor()),
		ExpressionValue::ObjectValue(watchActor()),
		ExpressionValue::Variable(&location, engine->vecprop),
		ExpressionValue::Variable(&rotation, engine->rotprop)
		});
}

// The original's ConvertVectorToCoordinates (Extension.dll 0x1004afc0):
// through the frame the window draws, in that frame's pixels from its
// corner -- neither over the root's multipliers nor from the window's own
// corner, where its clip rectangle starts further in --, each truncated; a
// point at a depth of 1 or less at the frame's middle, and false.
bool UViewportWindow::ProjectThroughFrame(const vec3& location, float& x, float& y) const
{
	vec4 viewSpaceLocation = LastFrame.WorldToView * vec4(location, 1.0f);
	if (viewSpaceLocation.z <= 1.0f)
	{
		x = std::trunc(LastFrame.FX * 0.5f);
		y = std::trunc(LastFrame.FY * 0.5f);
		return false;
	}
	vec4 projLocation = LastFrame.Projection * viewSpaceLocation;
	float rcpW = 1.0f / projLocation.w;
	x = std::trunc((projLocation.x * rcpW + 1.0f) * LastFrame.FX * 0.5f);
	y = std::trunc((projLocation.y * rcpW + 1.0f) * LastFrame.FY * 0.5f);
	return true;
}
