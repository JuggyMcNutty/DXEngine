
#include "Precomp.h"
#include "UHX.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UClass.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Extension/Windows/TabGroup/URootWindow.h"
#include "VM/ScriptCall.h"
#include "Math/coords.h"
#include "Utils/Logger.h"

const HXPropertyOffsets& HXProps()
{
	static HXPropertyOffsets offsets;
	static bool initialized = false;
	if (!initialized)
	{
		Package* hx = engine->packages->GetPackage("HX");
		auto offsetOf = [&](const char* className, const char* property) {
			UClass* cls = hx ? hx->GetClass(className) : nullptr;
			return cls ? cls->GetPropertyDataOffset(property) : PropertyDataOffset();
		};
		offsets.DecorationBurnTime = offsetOf("HXDecoration", "BurnTime");
		offsets.DecorationPushTime = offsetOf("HXDecoration", "PushTime");
		offsets.DecorationRenderProxy = offsetOf("HXDecoration", "RenderProxy");
		offsets.DecorationSwimPhi = offsetOf("HXDecoration", "SwimPhi");
		offsets.DecorationSwimAlpha = offsetOf("HXDecoration", "SwimAlpha");
		offsets.DecorationSwimOmega = offsetOf("HXDecoration", "SwimOmega");
		offsets.DecorationSwimAmp = offsetOf("HXDecoration", "SwimAmp");
		offsets.PlayerLipSynchTime = offsetOf("HXPlayerPawn", "LipSynchTime");
		offsets.PlayerNanoVirusTime = offsetOf("HXPlayerPawn", "NanoVirusTime");
		offsets.PlayerWindowScale = offsetOf("HXPlayerPawn", "WindowScale");
		offsets.PawnLipSynchTime = offsetOf("HXScriptedPawn", "LipSynchTime");
		offsets.EffectsTime = offsetOf("HXEffects", "EffectsTime");
		for (int i = 0; i < 16; i++)
			offsets.FlagCollections[i] = offsetOf("HXFlagReplicationInfo", ("FlagCollections" + std::to_string(i)).c_str());
		offsets.ClientFlagCollections = offsetOf("HXFlagReplicationInfo", "ClientFlagCollections");
		offsets.WindowPlayerPawn = offsetOf("HXWindow", "PlayerPawn");
		offsets.WindowRootWindow = offsetOf("HXWindow", "RootWindow");
		offsets.TextLogTextColor = offsetOf("HXTextLogWindow", "TextColor");
		offsets.IteratorSavedLocation = offsetOf("HXDecorationRenderIterator", "SavedLocation");
		offsets.IteratorSavedRotation = offsetOf("HXDecorationRenderIterator", "SavedRotation");
		initialized = true;
	}
	return offsets;
}

bool IsHXClass(UObject* obj, const char* className)
{
	NameString name(className);
	for (UClass* cls = obj ? obj->Class : nullptr; cls; cls = static_cast<UClass*>(cls->BaseStruct))
	{
		if (cls->Name == name && cls->package && cls->package->GetPackageName() == NameString("HX"))
			return true;
	}
	return false;
}

void AddHXRuntimeClasses(PackageManager* packages, Package* hx)
{
	if (hx->GetUObject("Class", "HXGameEngine"))
		return;
	UClass* base = UObject::TryCast<UClass>(packages->GetPackage("Engine")->GetUObject("Class", "GameEngine"));
	UClass* classClass = UObject::TryCast<UClass>(packages->GetPackage("Core")->GetUObject("Class", "Class"));
	if (!base || !classClass)
		return;
	// Laid out as its base, loaded first, as a class's load copies its
	// base's properties (UObject::LoadNow)
	base->LoadNow();
	UClass* cls = GC::Alloc<UClass>(NameString("HXGameEngine"), base, ObjectFlags::Native | ObjectFlags::Public | ObjectFlags::Standalone);
	cls->Class = classClass;
	cls->Properties = base->Properties;
	cls->StructSize = base->StructSize;
	cls->StructAlignment = base->StructAlignment;
	cls->PropertyData.Init(cls);
	// Its defaults', as UClass::Load sets them
	cls->SetObject("Class", cls);
	cls->SetName("Name", NameString());
	cls->SetInt("ObjectFlags", (int)cls->Flags);
	hx->AddRuntimeExport(cls);
}

// A countdown of HX's actors' ticks: a time above 0 less the tick's, held
// as the x87 holds it; under 0.001 it is 0 and the event runs.
static bool CountDown(float& time, float elapsed)
{
	if (!(time > 0.0f))
		return false;
	double left = (double)time - elapsed;
	time = (float)left;
	if (left < 0.001)
	{
		time = 0.0f;
		return true;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////

void UHXDecoration::Tick(float elapsed)
{
	UDeusExDecoration::Tick(elapsed);
	ULevel* level = XLevel();
	if (!level)
		return;

	// Every full tick, deferred or not: the float on water in and out at 1 a
	// second (AHXDecoration::Tick, HX.dll 0x1001e6f0)
	if (level->FullTick())
	{
		float& alpha = Value<float>(HXProps().DecorationSwimAlpha);
		if (BoolValue(PropOffsets_DeusExDecoration.bFloating))
		{
			double value = (double)elapsed + alpha;
			alpha = (float)value;
			if (value > 1.0)
				alpha = 1.0f;
		}
		else
		{
			double value = (double)alpha - elapsed;
			alpha = (float)value;
			if (value < 0.0)
				alpha = 0.0f;
		}
	}

	if (level->LastTickStart() == ULevel::TickStart::Waiting || bDeleteMe())
		return;
	if (CountDown(Value<float>(HXProps().DecorationBurnTime), elapsed))
		CallEvent(this, NameString("burnTimer"));
	if (!bDeleteMe() && CountDown(Value<float>(HXProps().DecorationPushTime), elapsed))
		CallEvent(this, NameString("pushTimer"));
}

/////////////////////////////////////////////////////////////////////////////

// HX's constructor drops the flag base the PlayerPawnExt's made (HX.dll
// 0x1001ea30): none is made.
void UHXPlayerPawn::InitNativeDefaults()
{
}

void UHXPlayerPawn::Tick(float elapsed)
{
	UDeusExPlayer::Tick(elapsed);
	ULevel* level = XLevel();
	if (!level || bDeleteMe() || level->LastTickStart() == ULevel::TickStart::Waiting || !level->FullTick())
		return;

	// TickStuff for any role, on a level not the Entry (AHXPlayerPawn::Tick,
	// HX.dll 0x10020080)
	if (level != engine->EntryLevel)
		CallEvent(this, NameString("TickStuff"), { ExpressionValue::FloatValue(elapsed) });
	if (bDeleteMe())
		return;
	const HXPropertyOffsets& props = HXProps();
	if (CountDown(Value<float>(props.PlayerLipSynchTime), elapsed))
		CallEvent(this, NameString("LipSynchTimer"));
	if (bDeleteMe())
		return;
	float& nanoVirusTime = Value<float>(props.PlayerNanoVirusTime);
	if (nanoVirusTime > 0.0f)
	{
		double left = (double)nanoVirusTime - elapsed;
		nanoVirusTime = (float)left;
		if (left < 0.001)
		{
			// HX zeroes the lip-sync time here, not the nano-virus time
			// (0x1002019a): NanoVirusTimer sets its own again, and nothing
			// sets the other
			Value<float>(props.PlayerLipSynchTime) = 0.0f;
			CallEvent(this, NameString("NanoVirusTimer"));
		}
	}
}

// HX's (HX.dll 0x10020460): every one of the three views that is the actor
// emptied, and the rest moved to the front in their order, where the game's
// empties the first alone and leaves a copy in the last
void UHXPlayerPawn::RemoveAdditionalView(UActor* actor)
{
	if (!actor || PropOffsets_PlayerPawn.AdditionalViews.DataOffset == ~(size_t)0)
		return;
	auto views = AdditionalViews();
	UActor* kept[3] = {};
	size_t count = 0;
	for (size_t i = 0; i < views.size(); i++)
	{
		if (views[i] && views[i] != actor)
			kept[count++] = views[i];
	}
	for (size_t i = 0; i < views.size(); i++)
		views[i] = i < count ? kept[i] : nullptr;
}

/////////////////////////////////////////////////////////////////////////////

void UHXScriptedPawn::Tick(float elapsed)
{
	UScriptedPawn::Tick(elapsed);
	ULevel* level = XLevel();
	if (!level || bDeleteMe() || level->LastTickStart() == ULevel::TickStart::Waiting || !level->FullTick())
		return;
	if (CountDown(Value<float>(HXProps().PawnLipSynchTime), elapsed))
		CallEvent(this, NameString("LipSynchTimer"));
}

/////////////////////////////////////////////////////////////////////////////

void UHXEffects::Tick(float elapsed)
{
	UActor::Tick(elapsed);
	ULevel* level = XLevel();
	if (!level || bDeleteMe() || level->LastTickStart() == ULevel::TickStart::Waiting || !level->FullTick())
		return;
	if (CountDown(Value<float>(HXProps().EffectsTime), elapsed))
		CallEvent(this, NameString("EffectsTimer"));
}

/////////////////////////////////////////////////////////////////////////////

// What of an actor's look HX copies onto another (UHXRenderIterator::
// CopyRenderProperties, HX.dll 0x10051680): the animation and its flags, the
// blend channels, place and rotation, how it is drawn, its lighting and its
// simulated animation; not bHidden, collision or sound.
void UHXRenderIterator::CopyRenderProperties(UActor* dest, UActor* source)
{
	if (!dest || !source)
		return;
	dest->bAnimFinished() = (bool)source->bAnimFinished();
	dest->bAnimLoop() = (bool)source->bAnimLoop();
	dest->bAnimNotify() = (bool)source->bAnimNotify();
	dest->bAnimByOwner() = (bool)source->bAnimByOwner();
	dest->bLightChanged() = (bool)source->bLightChanged();
	dest->bDynamicLight() = (bool)source->bDynamicLight();
	dest->AnimSequence() = source->AnimSequence();
	dest->AnimFrame() = source->AnimFrame();
	dest->AnimRate() = source->AnimRate();
	dest->TweenRate() = source->TweenRate();
	for (int i = 0; i < 4; i++)
	{
		dest->BlendAnimSequence()[i] = source->BlendAnimSequence()[i];
		dest->BlendAnimFrame()[i] = source->BlendAnimFrame()[i];
		dest->BlendAnimRate()[i] = source->BlendAnimRate()[i];
		dest->BlendTweenRate()[i] = source->BlendTweenRate()[i];
	}
	dest->Location() = source->Location();
	dest->Rotation() = source->Rotation();
	dest->DrawType() = source->DrawType();
	dest->Value<uint8_t>(PropOffsets_Actor.Style) = source->Value<uint8_t>(PropOffsets_Actor.Style);
	dest->Sprite() = source->Sprite();
	dest->Texture() = source->Texture();
	dest->Skin() = source->Skin();
	dest->Mesh() = source->Mesh();
	dest->DrawScale() = source->DrawScale();
	dest->PrePivot() = source->PrePivot();
	dest->ScaleGlow() = source->ScaleGlow();
	dest->AmbientGlow() = source->AmbientGlow();
	dest->Fatness() = source->Fatness();
	dest->bUnlit() = (bool)source->bUnlit();
	dest->bNoSmooth() = (bool)source->bNoSmooth();
	dest->bParticles() = (bool)source->bParticles();
	dest->bRandomFrame() = (bool)source->bRandomFrame();
	dest->bMeshEnviroMap() = (bool)source->bMeshEnviroMap();
	dest->bMeshCurvy() = (bool)source->bMeshCurvy();
	dest->VisibilityRadius() = source->VisibilityRadius();
	dest->VisibilityHeight() = source->VisibilityHeight();
	dest->bShadowCast() = (bool)source->bShadowCast();
	for (int i = 0; i < 8; i++)
		dest->MultiSkins()[i] = source->MultiSkins()[i];
	dest->LightType() = source->LightType();
	dest->LightEffect() = source->LightEffect();
	dest->LightBrightness() = source->LightBrightness();
	dest->LightHue() = source->LightHue();
	dest->LightSaturation() = source->LightSaturation();
	dest->LightRadius() = source->LightRadius();
	dest->LightPeriod() = source->LightPeriod();
	dest->LightPhase() = source->LightPhase();
	dest->LightCone() = source->LightCone();
	dest->VolumeBrightness() = source->VolumeBrightness();
	dest->VolumeRadius() = source->VolumeRadius();
	dest->VolumeFog() = source->VolumeFog();
	dest->bSpecialLit() = (bool)source->bSpecialLit();
	dest->bActorShadows() = (bool)source->bActorShadows();
	dest->bCorona() = (bool)source->bCorona();
	dest->bLensFlare() = (bool)source->bLensFlare();
	dest->AnimLast() = source->AnimLast();
	dest->AnimMinRate() = source->AnimMinRate();
	dest->OldAnimRate() = source->OldAnimRate();
	dest->SimAnim() = source->SimAnim();
	for (int i = 0; i < 4; i++)
	{
		dest->BlendAnimLast()[i] = source->BlendAnimLast()[i];
		dest->BlendAnimMinRate()[i] = source->BlendAnimMinRate()[i];
		dest->OldBlendAnimRate()[i] = source->OldBlendAnimRate()[i];
		dest->SimBlendAnim()[i] = source->SimBlendAnim()[i];
	}
}

/////////////////////////////////////////////////////////////////////////////

// Each step its script's function (FindFunctionChecked): one missing is
// fatal, as the original's
static ExpressionValue CallIteratorScript(UObject* iterator, const char* name, Array<ExpressionValue> args = {})
{
	if (!FindEventFunction(iterator, NameString(name)))
		Exception::Throw(std::string("Failed to find function ") + name + " in " + iterator->Class->Name.ToString());
	return CallEvent(iterator, NameString(name), std::move(args));
}

void UHXScriptedRenderIterator::Init(UPlayerPawn* camera)
{
	CallIteratorScript(this, "Init", { ExpressionValue::ObjectValue(camera) });
}

void UHXScriptedRenderIterator::First()
{
	CallIteratorScript(this, "First");
}

bool UHXScriptedRenderIterator::IsDone()
{
	return CallIteratorScript(this, "IsDone").ToBool();
}

UActor* UHXScriptedRenderIterator::CurrentItem()
{
	return UObject::TryCast<UActor>(CallIteratorScript(this, "CurrentItem").ToObject());
}

void UHXScriptedRenderIterator::Next()
{
	CallIteratorScript(this, "Next");
}

/////////////////////////////////////////////////////////////////////////////

void UHXDecorationRenderIterator::Init(UPlayerPawn* camera)
{
	Observer() = camera;
	if (UActor* decoration = UObject::TryCast<UActor>(Outer()))
	{
		Value<vec3>(HXProps().IteratorSavedLocation) = decoration->Location();
		Value<Rotator>(HXProps().IteratorSavedRotation) = decoration->Rotation();
	}
}

void UHXDecorationRenderIterator::First()
{
	Index() = 0;
}

void UHXDecorationRenderIterator::Next()
{
	Index()++;
}

// One item a frame: once drawn, the decoration's place and rotation as they
// were (HX.dll 0x10051b90)
bool UHXDecorationRenderIterator::IsDone()
{
	if (Index() == 0)
		return false;
	if (UActor* decoration = UObject::TryCast<UActor>(Outer()))
	{
		decoration->Location() = Value<vec3>(HXProps().IteratorSavedLocation);
		decoration->Rotation() = Value<Rotator>(HXProps().IteratorSavedRotation);
	}
	return true;
}

// HX.dll 0x10051be0: a decoration carried -- no physics, its base a pawn
// carrying it -- is drawn before its carrier, half the carrier's height up
// and half its radius and the decoration's own out along its facing; the
// local carrier sees a translucent unlit stand-in, the render proxy, in its
// place. One not carried bobs on water by its SwimAlpha.
UActor* UHXDecorationRenderIterator::CurrentItem()
{
	UActor* decoration = UObject::TryCast<UActor>(Outer());
	if (!decoration)
		return nullptr;

	const HXPropertyOffsets& props = HXProps();
	UPawn* carrier = UObject::TryCast<UPawn>(decoration->ActorBase());
	bool carried = decoration->Physics() == PHYS_None && carrier && carrier->bIsPawn() && (UActor*)carrier->carriedDecoration() == decoration;
	if (!carried)
	{
		float alpha = decoration->Value<float>(props.DecorationSwimAlpha);
		if (alpha > 0.0f && decoration->Level())
		{
			double angle = (double)decoration->Level()->TimeSeconds() * decoration->Value<float>(props.DecorationSwimOmega) + decoration->Value<float>(props.DecorationSwimPhi);
			double c = std::cos(angle);
			double s = std::sin(angle);
			const Rotator& amp = decoration->Value<Rotator>(props.DecorationSwimAmp);
			Rotator& rotation = decoration->Rotation();
			rotation.Pitch = (int)(int64_t)((double)amp.Pitch * alpha * s + (double)rotation.Pitch);
			rotation.Yaw = (int)(int64_t)((double)amp.Yaw * alpha * s + (double)rotation.Yaw);
			rotation.Roll = (int)(int64_t)((double)amp.Roll * alpha * c + (double)rotation.Roll);
		}
		return decoration;
	}

	UActor* item = decoration;
	bool localCarrier = carrier->bIsPlayer() && engine->viewport && (UActor*)engine->viewport->Actor() == (UActor*)carrier;
	if (localCarrier)
	{
		UActor*& proxy = decoration->Value<UActor*>(props.DecorationRenderProxy);
		if (proxy && !proxy->bDeleteMe())
		{
			proxy->LifeSpan() = proxy->Class->GetDefaultObject<UActor>()->LifeSpan();
		}
		else
		{
			UClass* proxyClass = engine->packages->FindClass("HX.HXRenderProxy");
			proxy = proxyClass ? decoration->Spawn(proxyClass, decoration, {}, vec3(0.0f), Rotator(0, 0, 0), true) : nullptr;
			if (!proxy)
				LogMessage("Failed to spawn RenderProxy for " + decoration->GetPathName() + ".");
		}
		if (proxy)
		{
			if (decoration->Level())
				decoration->LastRenderTime() = decoration->Level()->TimeSeconds();
			CopyRenderProperties(proxy, decoration);
			proxy->Value<uint8_t>(PropOffsets_Actor.Style) = (uint8_t)STY_Translucent;
			proxy->ScaleGlow() = 1.0f;
			proxy->bUnlit() = true;
			item = proxy;
		}
	}

	// The carrier's facing, flat and made a unit, as GMath's unit axes turned
	// by its rotation give it
	vec3 x, y, z;
	Coords::Rotation(carrier->Rotation()).GetAxes(x, y, z);
	float fx = x.x, fy = x.y;
	double lengthSq = (double)fy * fy + (double)fx * fx;
	if (lengthSq >= 0.00000001)
	{
		double length = std::sqrt(lengthSq);
		fx = (float)(fx * (1.0 / length));
		fy = (float)(fy * (1.0 / length));
	}
	float out = (float)((double)carrier->CollisionRadius() * 0.5 + decoration->CollisionRadius());
	float up = carrier->CollisionHeight() * 0.5f;
	item->Location() = vec3(carrier->Location().x + out * fx, carrier->Location().y + out * fy, carrier->Location().z + up);
	return item;
}

/////////////////////////////////////////////////////////////////////////////

// HX.dll 0x10014e30: the root, and its player, which must be HX's
void UHXWindow::InitDefaults()
{
	UWindow::InitDefaults();
	URootWindow* root = GetRootWindow();
	if (!root)
		Exception::Throw("Assertion failed: RootWindow [File:HXWindow.cpp] [Line: 29]");
	Value<URootWindow*>(HXProps().WindowRootWindow) = root;
	UObject* pawn = root->parentPawn();
	if (!UObject::TryCast<UHXPlayerPawn>(pawn))
		Exception::Throw("Cast of " + (pawn ? pawn->GetPathName() : std::string("NULL")) + " to HXPlayerPawn failed");
	Value<UObject*>(HXProps().WindowPlayerPawn) = pawn;
}

/////////////////////////////////////////////////////////////////////////////

// HX.dll 0x100146d0: a line of its own colour, TextColor by default, and its
// own time, the log's timeout by default
void UHXTextLogWindow::AddLine(const std::string& text, std::optional<Color> color, std::optional<float> timeout)
{
	LogEntry entry;
	entry.text = text;
	entry.color = color ? *color : Value<Color>(HXProps().TextLogTextColor);
	entry.timeLeft = GetTickOffset() + (timeout ? *timeout : textTimeout());
	logs.push_back(entry);
	RegenerateText();
}

// HX.dll 0x10014460: unless paused, every line's time counts down, and each
// line whose time is up leaves, the rest kept in order
void UHXTextLogWindow::Tick(float timeElapsed)
{
	if (bPaused())
		return;
	bool expired = false;
	for (LogEntry& entry : logs)
	{
		entry.timeLeft -= timeElapsed;
		if (entry.timeLeft <= 0.0f)
			expired = true;
	}
	if (!expired)
		return;
	Array<LogEntry> kept;
	for (LogEntry& entry : logs)
	{
		if (entry.timeLeft > 0.0f)
			kept.push_back(entry);
	}
	logs.swap(kept);
	RegenerateText();
}
