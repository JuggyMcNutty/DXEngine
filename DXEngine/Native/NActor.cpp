
#include "Precomp.h"
#include "NActor.h"
#include "VM/NativeFunc.h"
#include "VM/ScriptCall.h"
#include "VM/Frame.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/UFunction.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/Resources/USound.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Mesh/UMesh.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/UDecal.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/Info/UZoneInfo.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Package/PackageManager.h"
#include "Engine.h"
#include "Audio/AudioDevice.h"
#include "Packages/Engine/Subsystems/UDXEngineAudioDevice.h"
#include "Utils/StrTools.h"

void NActor::RegisterFunctions()
{
	RegisterVMNativeFunc_3("Actor", "AllActors", &NActor::AllActors, 304);
	RegisterVMNativeFunc_1("Actor", "AutonomousPhysics", &NActor::AutonomousPhysics, 3971);
	RegisterVMNativeFunc_2("Actor", "BasedActors", &NActor::BasedActors, 306);
	RegisterVMNativeFunc_2("Actor", "ChildActors", &NActor::ChildActors, 305);
	RegisterVMNativeFunc_2("Actor", "ConsoleCommand", &NActor::ConsoleCommand, 0);
	RegisterVMNativeFunc_3("Actor", "CycleActors", &NActor::CycleActors, 1002);
	RegisterVMNativeFunc_6("Actor", "DemoPlaySound", &NActor::DemoPlaySound_Deus, 0);
	RegisterVMNativeFunc_1("Actor", "Destroy", &NActor::Destroy, 279);
	RegisterVMNativeFunc_1("Actor", "Error", &NActor::Error, 233);
	RegisterVMNativeFunc_3("Actor", "FastTrace", &NActor::FastTrace, 548);
	RegisterVMNativeFunc_0("Actor", "FinishInterpolation", &NActor::FinishInterpolation, 301);
	RegisterLatentAction(302, LatentRunState::FinishInterpolation);
	RegisterVMNativeFunc_2("Actor", "GetAnimGroup", &NActor::GetAnimGroup, 293);
	RegisterVMNativeFunc_6("Actor", "GetBoundingBox", &NActor::GetBoundingBox, 724);
	RegisterVMNativeFunc_4("Actor", "GetCacheEntry", &NActor::GetCacheEntry, 0);
	RegisterVMNativeFunc_4("Actor", "GetMapName", &NActor::GetMapName, 539);
	RegisterVMNativeFunc_2("Actor", "GetMeshTexture", &NActor::GetMeshTexture, 1013);
	RegisterVMNativeFunc_3("Actor", "GetNextInt", &NActor::GetNextInt, 0);
	RegisterVMNativeFunc_4("Actor", "GetNextIntDesc", &NActor::GetNextIntDesc, 0);
	RegisterVMNativeFunc_5("Actor", "GetNextSkin", &NActor::GetNextSkin, 545);
	RegisterVMNativeFunc_2("Actor", "GetSoundDuration", &NActor::GetSoundDuration, 0);
	RegisterVMNativeFunc_1("Actor", "GetURLMap", &NActor::GetURLMap, 547);
	RegisterVMNativeFunc_2("Actor", "HasAnim", &NActor::HasAnim, 263);
	RegisterVMNativeFunc_2("Actor", "IsOverlapping", &NActor::IsOverlapping, 718);
	RegisterVMNativeFunc_1("Actor", "LastRendered", &NActor::LastRendered, 723);
	RegisterVMNativeFunc_1("Actor", "LinkSkelAnim", &NActor::LinkSkelAnim, 0);
	RegisterVMNativeFunc_1("Actor", "MakeNoise", &NActor::MakeNoise, 512);
	RegisterVMNativeFunc_2("Actor", "Move", &NActor::Move, 266);
	RegisterVMNativeFunc_3("Actor", "MoveCacheEntry", &NActor::MoveCacheEntry, 0);
	RegisterVMNativeFunc_2("Actor", "MoveSmooth", &NActor::MoveSmooth, 3969);
	RegisterVMNativeFunc_6("Actor", "PlayOwnedSound", &NActor::PlayOwnedSound_Deus, 0);
	RegisterVMNativeFunc_7("Actor", "PlaySound", &NActor::PlaySound_Deus, 264);
	RegisterVMNativeFunc_1("Actor", "PlayerCanSeeMe", &NActor::PlayerCanSeeMe, 532);
	RegisterVMNativeFunc_4("Actor", "RadiusActors", &NActor::RadiusActors, 310);
	RegisterVMNativeFunc_1("Actor", "SetBase", &NActor::SetBase, 298);
	RegisterVMNativeFunc_3("Actor", "SetCollision", &NActor::SetCollision, 262);
	RegisterVMNativeFunc_3("Actor", "SetCollisionSize", &NActor::SetCollisionSize, 283);
	RegisterVMNativeFunc_2("Actor", "SetLocation", &NActor::SetLocation, 267);
	RegisterVMNativeFunc_1("Actor", "SetOwner", &NActor::SetOwner, 272);
	RegisterVMNativeFunc_2("Actor", "SetPhysics", &NActor::SetPhysics_Deus, 3970);
	RegisterVMNativeFunc_2("Actor", "SetRotation", &NActor::SetRotation, 299);
	RegisterVMNativeFunc_2("Actor", "SetTimer", &NActor::SetTimer, 280);
	RegisterVMNativeFunc_1("Actor", "Sleep", &NActor::Sleep, 256);
	// A latent action is saved as the number of the native that polls it,
	// as the original's state frame keeps it: execPollSleep is 384 and
	// execPollFinishAnim 385 (Engine.dll registers them so). DXEngine had
	// 257 and 262 (SetCollision's), so the original's load of a DXEngine save
	// ran no native for a sleeping or animating actor and stopped with
	// "Unknown code token", and DXEngine's of an original save cut the
	// sleep or the animation short.
	RegisterLatentAction(384, LatentRunState::Sleep);
	RegisterVMNativeFunc_6("Actor", "Spawn", &NActor::Spawn, 278);
	RegisterVMNativeFunc_2("Actor", "TouchingActors", &NActor::TouchingActors, 307);
	RegisterVMNativeFunc_7("Actor", "Trace", &NActor::Trace, 277);
	RegisterVMNativeFunc_7("Actor", "TraceActors", &NActor::TraceActors, 309);
	RegisterVMNativeFunc_2("Actor", "TweenAnim", &NActor::TweenAnim, 294);
	RegisterVMNativeFunc_4("Actor", "VisibleActors", &NActor::VisibleActors, 311);
	RegisterVMNativeFunc_5("Actor", "VisibleCollidingActors", &NActor::VisibleCollidingActors, 312);
	RegisterVMNativeFunc_1("Actor", "GetPlayerPawn", &NActor::GetPlayerPawn, 720);
	RegisterVMNativeFunc_1("Actor", "AIClearEvent", &NActor::AIClearEvent, 716);
	RegisterVMNativeFunc_1("Actor", "AIClearEventCallback", &NActor::AIClearEventCallback, 711);
	RegisterVMNativeFunc_2("Actor", "AIEndEvent", &NActor::AIEndEvent, 715);
	RegisterVMNativeFunc_2("Actor", "AIGetLightLevel", &NActor::AIGetLightLevel, 700);
	RegisterVMNativeFunc_4("Actor", "AISendEvent", &NActor::AISendEvent, 713);
	RegisterVMNativeFunc_7("Actor", "AISetEventCallback", &NActor::AISetEventCallback, 710);
	RegisterVMNativeFunc_4("Actor", "AIStartEvent", &NActor::AIStartEvent, 714);
	RegisterVMNativeFunc_2("Actor", "AIVisibility", &NActor::AIVisibility, 701);
	RegisterVMNativeFunc_10("Actor", "TraceTexture", &NActor::TraceTexture, 1000);
	RegisterVMNativeFunc_7("Actor", "TraceVisibleActors", &NActor::TraceVisibleActors, 1003);

	// Color operators
	RegisterVMNativeFunc_3("Actor", "Add_ColorColor", &NActor::Add_ColorColor, 551);
	RegisterVMNativeFunc_3("Actor", "Subtract_ColorColor", &NActor::Subtract_ColorColor, 549);
	RegisterVMNativeFunc_3("Actor", "Multiply_ColorFloat", &NActor::Multiply_ColorFloat, 552);
	RegisterVMNativeFunc_3("Actor", "Multiply_FloatColor", &NActor::Multiply_FloatColor, 550);

	RegisterVMNativeFunc_1("Actor", "InStasis", &NActor::InStasis, 721);
	RegisterVMNativeFunc_11("Actor", "ParabolicTrace", &NActor::ParabolicTrace, 722);
	RegisterVMNativeFunc_5("Actor", "RandomBiasedRotation", &NActor::RandomBiasedRotation, 717);
	RegisterVMNativeFunc_1("Actor", "SetInstantMusicVolume", &NActor::SetInstantMusicVolume, 270);
	RegisterVMNativeFunc_1("Actor", "SetInstantSoundVolume", &NActor::SetInstantSoundVolume, 268);
	RegisterVMNativeFunc_1("Actor", "SetInstantSpeechVolume", &NActor::SetInstantSpeechVolume, 269);
	RegisterVMNativeFunc_1("Actor", "StopSound", &NActor::StopSound_Deus, 265);
	RegisterVMNativeFunc_4("Actor", "PlayBlendAnim", &NActor::PlayBlendAnim, 1010);
	RegisterVMNativeFunc_3("Actor", "TweenBlendAnim", &NActor::TweenBlendAnim, 1012);
	RegisterVMNativeFunc_3("Actor", "PlayAnim", &NActor::PlayAnim, 259);
	RegisterVMNativeFunc_4("Actor", "LoopAnim", &NActor::LoopAnim, 260);
	RegisterVMNativeFunc_1("Actor", "IsAnimating", &NActor::IsAnimating, 282);
	RegisterVMNativeFunc_0("Actor", "FinishAnim", &NActor::FinishAnim, 261);
	RegisterLatentAction(385, LatentRunState::FinishAnim);
}

void NActor::Add_ColorColor(const Color& A, const Color& B, Color& ReturnValue)
{
	Color c;
	c.R = std::min(A.R + (int)B.R, 255);
	c.G = std::min(A.G + (int)B.G, 255);
	c.B = std::min(A.B + (int)B.B, 255);
	c.A = std::min(A.A + (int)B.A, 255);
	ReturnValue = c;
}

// None for the class is Actor in every actor iterator, as the original's
// (Engine.dll execAllActors 0x103e43f0, execBasedActors 0x103e4800,
// execRadiusActors 0x103e4fa0, execZoneActors 0x103e5960, execCycleActors
// 0x1036e9d0 and the rest); DXEngine's dereferenced it, CycleActors finding
// nothing
UObject* NActor::ActorClassOrGiven(UObject* Self, UObject* BaseClass)
{
	if (BaseClass)
		return BaseClass;
	for (UStruct* cls = Self->Class; cls; cls = cls->BaseStruct)
	{
		if (cls->Name == "Actor")
			return cls;
	}
	return nullptr;
}

void NActor::AllActors(UObject* Self, UObject* BaseClass, UObject*& Actor, std::optional<NameString> MatchTag)
{
	Frame::CreatedIterator = std::make_unique<AllActorsIterator>(Self, ActorClassOrGiven(Self, BaseClass), &Actor, MatchTag ? *MatchTag : std::string());
}

// The original's (execAutonomousPhysics, Engine.dll 0x103c8f30): the
// acceleration first cut toward zero to a tenth, 0.1 x int(10 x each), in
// doubles as its x87 works them (0x103c8f60-0x103c8fce), physics or not --
// the tenths a client's moves send the server, ServerMove's acceleration
// ten times over in whole numbers --; then the physics, for an actor with
// any (TickPhysics).
void NActor::AutonomousPhysics(UObject* Self, float DeltaSeconds)
{
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	vec3& acceleration = SelfActor->Acceleration();
	acceleration.x = (float)(0.1 * (double)(int)(10.0 * (double)acceleration.x));
	acceleration.y = (float)(0.1 * (double)(int)(10.0 * (double)acceleration.y));
	acceleration.z = (float)(0.1 * (double)(int)(10.0 * (double)acceleration.z));
	SelfActor->TickPhysics(DeltaSeconds);
}

void NActor::BasedActors(UObject* Self, UObject* BaseClass, UObject*& Actor)
{
	Frame::CreatedIterator = std::make_unique<BasedActorsIterator>(UObject::Cast<UActor>(Self), ActorClassOrGiven(Self, BaseClass), &Actor);
}

void NActor::ChildActors(UObject* Self, UObject* BaseClass, UObject*& Actor)
{
	Frame::CreatedIterator = std::make_unique<ChildActorsIterator>(UObject::Cast<UActor>(Self), ActorClassOrGiven(Self, BaseClass), &Actor);
}

void NActor::CycleActors(UObject* Self, UObject* BaseClass, UObject*& Actor, int& index)  
{  
	Frame::CreatedIterator = std::make_unique<CycleActorsIterator>(Self, ActorClassOrGiven(Self, BaseClass), &Actor, &index);  
}

void NActor::ConsoleCommand(UObject* Self, const std::string& Command, std::string& ReturnValue)
{
	// "Execute a console command in the context of the current level and game engine"

	ExpressionValue found = ExpressionValue::BoolValue(false);
	ReturnValue = engine->ConsoleCommand(Self, Command, found.ToType<BitfieldBool&>());
}

void NActor::Destroy(UObject* Self, BitfieldBool& ReturnValue)
{
	ReturnValue = UObject::Cast<UActor>(Self)->Destroy();
}

void NActor::Error(UObject* Self, const std::string& S)
{
	LogMessage("Error: " + S);
	UObject::Cast<UActor>(Self)->Destroy();
}

// The original's (Engine.dll 0x103e35e0): FastLineCheck of the line from
// TraceStart, by default the actor's place, to TraceEnd, walked from its
// start
void NActor::FastTrace(UObject* Self, const vec3& TraceEnd, std::optional<vec3> TraceStart, BitfieldBool& ReturnValue)
{
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	vec3 start = TraceStart ? *TraceStart : SelfActor->Location();
	ReturnValue = SelfActor->FastTrace(TraceEnd, start);
}

void NActor::FinishAnim(UObject* Self)
{
	UObject::Cast<UActor>(Self)->FinishAnim();
}

void NActor::FinishInterpolation(UObject* Self)
{
	if (Self->StateFrame)
		Self->StateFrame->LatentState = LatentRunState::FinishInterpolation;
}

void NActor::GetAnimGroup(UObject* Self, const NameString& Sequence, NameString& ReturnValue)
{
	ReturnValue = UObject::Cast<UActor>(Self)->GetAnimGroup(Sequence);
}

void NActor::GetBoundingBox(UObject* Self, vec3& MinVect, vec3& MaxVect, std::optional<bool> bExact, std::optional<vec3> testLocation, std::optional<Rotator> testRotation, BitfieldBool& ReturnValue)
{
	UActor* actor = UObject::Cast<UActor>(Self);
	if (!actor) { ReturnValue = false; return; }

	// With a test place or rotation, as every script call gives, the
	// original puts the actor there for the moment and takes its
	// primitive's box; moving the world-space box by them instead left it
	// off by the actor's own place.
	if (testLocation || testRotation)
	{
		vec3 keepLocation = actor->Location();
		Rotator keepRotation = actor->Rotation();
		actor->Location() = testLocation ? *testLocation : keepLocation;
		actor->Rotation() = testRotation ? *testRotation : keepRotation;
		actor->UpdateBspInfo();
		BBox bbox = actor->BspInfo.BoundingBox;
		actor->Location() = keepLocation;
		actor->Rotation() = keepRotation;
		actor->UpdateBspInfo();
		MinVect = bbox.min;
		MaxVect = bbox.max;
		ReturnValue = true;
		return;
	}

	// Without, its box where it is now, as the original takes its
	// primitive's: the box the renderer last made is a frame old, and a
	// dedicated server, drawing no frame, makes none.
	actor->UpdateBspInfo();
	BBox bbox = actor->BspInfo.BoundingBox;
	MinVect = bbox.min;
	MaxVect = bbox.max;
	ReturnValue = true;
}

void NActor::GetCacheEntry(UObject* Self, int Num, std::string& Guid, std::string& Filename, BitfieldBool& ReturnValue)
{
	LogUnimplemented("Actor.GetCacheEntry");
	ReturnValue = false;
}

void NActor::GetNextInt(UObject* Self, const std::string& ClassName, int Num, std::string& ReturnValue)
{
	Array<IntObject>& objects = engine->packages->GetIntObjects(ClassName);
	if (Num >= 0 && (size_t)Num < objects.size())
		ReturnValue = objects[Num].Name.ToString();
	else
		ReturnValue = {};
}

void NActor::GetNextIntDesc(UObject* Self, const std::string& ClassName, int Num, std::string& Entry, std::string& Description)
{
	Array<IntObject>& objects = engine->packages->GetIntObjects(ClassName);
	if (Num >= 0 && (size_t)Num < objects.size())
	{
		Entry = objects[Num].Name.ToString();
		Description = objects[Num].Description;
	}
	else
	{
		Entry = {};
		Description = {};
	}
}

void NActor::GetMapName(UObject* Self, const std::string& NameEnding, const std::string& MapName, int Dir, std::string& ReturnValue)
{
	Array<std::string> maps;

	// Filter list to only those with the matching map type
	for (const std::string& name : engine->packages->GetMaps())
	{
		// Case insensitive prefix comparison because Unreal Deathmatch maps start with "Dm" instead of "DM"
		if ( name.size() >= NameEnding.size() && StrTools::equals_ignore_case(name.substr(0, NameEnding.size()), NameEnding) )
			maps.push_back(name);
	}

	// Find the previous or next map relative to the MapName specified
	if (!MapName.empty() && MapName != "None")
	{
		auto it = std::find(maps.begin(), maps.end(), MapName);
		if (it != maps.end())
		{
			int index = (int)std::distance(maps.begin(), it) + Dir;
			if (index < 0)
				index = (int)maps.size() - 1;
			else if (index >= (int)maps.size())
				index = 0;
			ReturnValue = maps[index];
			return;
		}
	}

	// Grab first map if map wasn't found or none was specified
	ReturnValue = !maps.empty() ? maps.front() : std::string();
}

// The original's (Engine.dll AActor::execGetNextSkin 0x103b8f10): over the
// .int files' registered Texture objects in their order, those whose name
// starts with Prefix (without regard to case, over its length: an empty
// prefix takes all). Dir 0 gives the one named CurrentSkin, 1 the one after
// it (the first when it is last), -1 the one before it (none when it is
// first); a CurrentSkin not among them gives the last of them; none at all,
// two empty strings. The game's .int files register no Texture.
void NActor::GetNextSkin(UObject* Self, const std::string& Prefix, const std::string& CurrentSkin, int Dir, std::string& SkinName, std::string& SkinDesc)
{
	const Array<IntObject>& skins = engine->packages->GetIntObjects("Texture");
	int best = -1, first = -1, last = -1;
	bool passedCurrent = false;
	for (int i = 0; i < (int)skins.size(); i++)
	{
		std::string name = skins[i].Name.ToString();
		if (!StrTools::startswith(name, Prefix, true))
			continue;
		if (passedCurrent)
		{
			best = i;
			break;
		}
		if (first == -1)
			first = i;
		if (StrTools::equals_ignore_case(name, CurrentSkin))
		{
			if (Dir == -1)
			{
				best = last;
				break;
			}
			if (Dir == 0)
			{
				best = i;
				break;
			}
			passedCurrent = true;
		}
		last = i;
	}
	if (best == -1 && passedCurrent)
		best = first;
	if (best == -1)
		best = last;
	if (best < 0 || best >= (int)skins.size())
	{
		SkinName.clear();
		SkinDesc.clear();
	}
	else
	{
		SkinName = skins[best].Name.ToString();
		SkinDesc = skins[best].Description;
	}
}

void NActor::GetSoundDuration(UObject* Self, UObject* Sound, float& ReturnValue)
{
	USound* s = UObject::Cast<USound>(Sound);
	ReturnValue = s->GetPeriod();
}

// The game engine's last URL's map, as the original's reads it (Engine.dll
// 0x103b8df0), not the level's URL, which GetLocalURL gives: the map a URL
// with a load option named, ?load= or a restart's ?loadonly, where the level
// keeps its file's Index.dx; a level from Current's or a save's own.
void NActor::GetURLMap(UObject* Self, std::string& ReturnValue)
{
	ReturnValue = engine->LastURL.Map;
}

void NActor::HasAnim(UObject* Self, const NameString& Sequence, BitfieldBool& ReturnValue)
{
	ReturnValue = UObject::Cast<UActor>(Self)->HasAnim(Sequence);
}

void NActor::IsAnimating(UObject* Self, BitfieldBool& ReturnValue)
{
	ReturnValue = UObject::Cast<UActor>(Self)->IsAnimating();
}

void NActor::IsOverlapping(UObject* Self, UObject* checkActor, BitfieldBool& ReturnValue)
{
	UActor* selfActor = UObject::Cast<UActor>(Self);  
	UActor* otherActor = UObject::Cast<UActor>(checkActor);  
	ReturnValue = selfActor && otherActor && selfActor->IsOverlapping(otherActor); 
}

void NActor::LastRendered(UObject *Self, float &ReturnValue)
{
	// The time since the actor was last drawn, never below 0. A decal's own
	// LastRenderedTime is not read -- the original's does not either, so a
	// decal (Deus Ex's Shadow) never counts as drawn.
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	ReturnValue = std::max(0.0f, SelfActor->Level()->TimeSeconds() - SelfActor->LastRenderTime());
}

void NActor::LinkSkelAnim(UObject* Self, UObject* Anim)
{
	LogUnimplemented("Actor.LinkSkelAnim");
}

void NActor::LoopAnim(UObject* Self, const NameString& Sequence, std::optional<float> Rate, std::optional<float> TweenTime, std::optional<float> MinRate)
{
	// Rate 1, TweenTime -1 (a tween of one frame, UActor::TweenIntoAnim) and
	// MinRate 0 by default, as the original's execLoopAnim takes them
	UObject::Cast<UActor>(Self)->LoopAnim(Sequence, Rate ? *Rate : 1.0f, TweenTime ? *TweenTime : -1.0f, MinRate ? *MinRate : 0.0f);
}

void NActor::MakeNoise(UObject* Self, float Loudness)
{
	UObject::Cast<UActor>(Self)->MakeNoise(Loudness);
}

void NActor::Move(UObject* Self, const vec3& Delta, BitfieldBool& ReturnValue)
{
	ReturnValue = UObject::Cast<UActor>(Self)->Move(Delta);
}

void NActor::MoveCacheEntry(UObject* Self, const std::string& Guid, std::optional<std::string> NewFilename, BitfieldBool& ReturnValue)
{
	LogUnimplemented("Actor.MoveCacheEntry");
	ReturnValue = false;
}

void NActor::MoveSmooth(UObject* Self, const vec3& Delta, BitfieldBool& ReturnValue)
{
	ReturnValue = UObject::Cast<UActor>(Self)->MoveSmooth(Delta);
}

void NActor::Multiply_ColorFloat(const Color& A, float B, Color& ReturnValue)
{
	Color c;
	c.R = (int)std::round(clamp(A.R * B, 0.0f, 255.0f));
	c.G = (int)std::round(clamp(A.G * B, 0.0f, 255.0f));
	c.B = (int)std::round(clamp(A.B * B, 0.0f, 255.0f));
	c.A = A.A;
	ReturnValue = c;
}

void NActor::Multiply_FloatColor(float A, const Color& B, Color& ReturnValue)
{
	Color c;
	c.R = (int)std::round(clamp(A * B.R, 0.0f, 255.0f));
	c.G = (int)std::round(clamp(A * B.G, 0.0f, 255.0f));
	c.B = (int)std::round(clamp(A * B.B, 0.0f, 255.0f));
	c.A = B.A;
	ReturnValue = c;
}

void NActor::PlayAnim(UObject* Self, const NameString& Sequence, std::optional<float> Rate, std::optional<float> TweenTime)
{
	// Rate 1 and TweenTime -1 (a tween of one frame, UActor::TweenIntoAnim) by
	// default, as the original's execPlayAnim takes them
	UObject::Cast<UActor>(Self)->PlayAnim(Sequence, Rate ? *Rate : 1.0f, TweenTime ? *TweenTime : -1.0f);
}

// The slot an int, as the script declares it: a byte took 256 to slot 0
void NActor::PlayBlendAnim(UObject* Self, const NameString& Sequence, std::optional<float> Rate, std::optional<float> TweenTime, std::optional<int> BlendSlot)
{
	// Rate 1, TweenTime -1 and slot 0 by default (dx-reverse-info/engine-dll.md)
	UObject::Cast<UActor>(Self)->PlayBlendAnim(Sequence, Rate ? *Rate : 1.0f, TweenTime ? *TweenTime : -1.0f, BlendSlot ? *BlendSlot : 0);
}

// Whether the script function calling the native is marked simulated, as
// Engine.dll's PlaySound asks of the frame it is called from: the native's
// own frame is on top, its caller's under it. State code is no function.
static bool CalledFromSimulated()
{
	if (Frame::Callstack.size() < 2)
		return false;
	UFunction* caller = UObject::TryCast<UFunction>(Frame::Callstack[Frame::Callstack.size() - 2]->Func);
	return caller && AllFlags(caller->FuncFlags, FunctionFlags::Simulated);
}

// Deus Ex's sounds are heard as the original's are (dx-reverse-info
// engine-dll.md, sounds): each hearer the native names is asked
// (UActor::CheckHearSound), and the hearer's ClientHearSound plays it -- in a
// net game on the remote player's client. The ID packs DXEngine's number for
// the actor (UDXEngineAudioDevice::SoundId).
void NActor::PlaySound_Deus(UObject* Self, UObject* Sound, std::optional<uint8_t> Slot, std::optional<float> Volume, std::optional<bool> bNoOverride, std::optional<float> Radius, std::optional<float> Pitch, int& ReturnValue)
{
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	USound* s = UObject::Cast<USound>(Sound);
	if (!s)
	{
		ReturnValue = 0;
		return;
	}
	int slot = Slot ? *Slot : SLOT_Misc;
	int id = engine->audiodev->SoundId(SelfActor, slot);
	if (bNoOverride && *bNoOverride) id |= 1;
	// A radius of 0 or less is 800.
	float radius = Radius ? *Radius : SelfActor->TransientSoundRadius();
	if (radius <= 0.0f)
		radius = 800.0f;
	vec3 parameters((Volume ? *Volume : SelfActor->TransientSoundVolume()) * 100.0f, radius, (Pitch ? *Pitch : 1.0f) * 100.0f);
	// On a client, or from a simulated function, the viewports' players hear
	// it; otherwise every player pawn.
	bool viewportsOnly = SelfActor->Level()->NetMode() == NM_Client || CalledFromSimulated();
	SelfActor->HearSound(id, s, parameters, radius * radius, viewportsOnly);
	ReturnValue = id;
}

// No 800 here: a radius of 0 reaches 1,600. A client's viewports hear it;
// off a client every player pawn but the actor's own remote player, who
// played it already: the actor, a player pawn whose player is not a
// viewport, else its owner the same.
void NActor::PlayOwnedSound_Deus(UObject* Self, UObject* Sound, std::optional<uint8_t> Slot, std::optional<float> Volume, std::optional<bool> bNoOverride, std::optional<float> Radius, std::optional<float> Pitch)
{
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	USound* s = UObject::Cast<USound>(Sound);
	if (!s)
		return;
	int slot = Slot ? *Slot : SLOT_Misc;
	int id = engine->audiodev->SoundId(SelfActor, slot);
	if (bNoOverride && *bNoOverride) id |= 1;
	float radius = Radius ? *Radius : SelfActor->TransientSoundRadius();
	float range = radius == 0.0f ? 1600.0f : radius;
	vec3 parameters((Volume ? *Volume : SelfActor->TransientSoundVolume()) * 100.0f, radius, (Pitch ? *Pitch : 1.0f) * 100.0f);

	uint8_t netMode = SelfActor->Level()->NetMode();
	if (netMode == NM_Client)
	{
		SelfActor->HearSound(id, s, parameters, range * range, true);
		return;
	}
	UPawn* skip = nullptr;
	if (netMode != NM_Standalone)
	{
		auto remotePlayer = [](UActor* actor) -> UPawn* {
			UPlayerPawn* player = UObject::TryCast<UPlayerPawn>(actor);
			return player && player->Player() && !UObject::TryCast<UViewport>(player->Player()) ? player : nullptr;
		};
		if (UObject::TryCast<UPlayerPawn>(SelfActor))
			skip = remotePlayer(SelfActor);
		else if (SelfActor->Owner())
			skip = remotePlayer(SelfActor->Owner());
	}
	SelfActor->HearSound(id, s, parameters, range * range, false, skip);
}

// The viewports' players; a radius of 0 reaches 1,600.
void NActor::DemoPlaySound_Deus(UObject* Self, UObject* Sound, std::optional<uint8_t> Slot, std::optional<float> Volume, std::optional<bool> bNoOverride, std::optional<float> Radius, std::optional<float> Pitch)
{
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	USound* s = UObject::Cast<USound>(Sound);
	if (!s)
		return;
	int slot = Slot ? *Slot : SLOT_Misc;
	int id = engine->audiodev->SoundId(SelfActor, slot);
	if (bNoOverride && *bNoOverride) id |= 1;
	float radius = Radius ? *Radius : SelfActor->TransientSoundRadius();
	float range = radius == 0.0f ? 1600.0f : radius;
	vec3 parameters((Volume ? *Volume : SelfActor->TransientSoundVolume()) * 100.0f, radius, (Pitch ? *Pitch : 1.0f) * 100.0f);
	SelfActor->HearSound(id, s, parameters, range * range, true);
}


void NActor::PlayerCanSeeMe(UObject* Self, BitfieldBool& ReturnValue)
{
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	ReturnValue = SelfActor->PlayerCanSeeMe();
}

void NActor::RadiusActors(UObject* Self, UObject* BaseClass, UObject*& Actor, float Radius, std::optional<vec3> Loc)
{
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	Frame::CreatedIterator = std::make_unique<RadiusActorsIterator>(SelfActor, ActorClassOrGiven(Self, BaseClass), &Actor, Radius, Loc ? *Loc : SelfActor->Location());
}

void NActor::SetBase(UObject* Self, UObject* NewBase)
{
	UObject::Cast<UActor>(Self)->SetBase(UObject::Cast<UActor>(NewBase), true);
}

void NActor::SetCollision(UObject* Self, std::optional<bool> NewColActors, std::optional<bool> NewBlockActors, std::optional<bool> NewBlockPlayers)
{
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	SelfActor->SetCollision(
		NewColActors ? *NewColActors : SelfActor->bCollideActors(),
		NewBlockActors ? *NewBlockActors : SelfActor->bBlockActors(),
		NewBlockPlayers ? *NewBlockPlayers : SelfActor->bBlockPlayers());
}

void NActor::SetCollisionSize(UObject* Self, float NewRadius, float NewHeight, BitfieldBool& ReturnValue)
{
	ReturnValue = UObject::Cast<UActor>(Self)->SetCollisionSize(NewRadius, NewHeight);
}

void NActor::SetLocation(UObject* Self, const vec3& NewLocation, BitfieldBool& ReturnValue)
{
	ReturnValue = UObject::Cast<UActor>(Self)->SetLocation(NewLocation);
}

void NActor::SetOwner(UObject* Self, UObject* NewOwner)
{
	UObject::Cast<UActor>(Self)->SetOwner(UObject::Cast<UActor>(NewOwner));
}

void NActor::SetPhysics_Deus(UObject* Self, uint8_t newPhysics, std::optional<UObject*> newFloor)
{
	// The original's setPhysics (UActor::SetPhysics with a floor). The
	// scripts pass the wall hit as grenades, pool balls, basketballs and
	// fragments come to rest.
	UObject::Cast<UActor>(Self)->SetPhysics(newPhysics, newFloor ? UObject::TryCast<UActor>(*newFloor) : nullptr);
}

void NActor::SetRotation(UObject* Self, const Rotator& NewRotation, BitfieldBool& ReturnValue)
{
	ReturnValue = UObject::Cast<UActor>(Self)->SetRotation(NewRotation);
}

void NActor::SetTimer(UObject* Self, float NewTimerRate, bool bLoop)
{
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	SelfActor->TimerCounter() = 0.0f;
	SelfActor->TimerRate() = NewTimerRate > 0.0f ? NewTimerRate : 0.0f;
	SelfActor->bTimerLoop() = bLoop;
}

// The original's (AActor::execSleep, Engine.dll 0x103dfef0): the time kept in
// the actor's LatentFloat, which a save keeps with it (UActor::PollLatentAction)
void NActor::Sleep(UObject* Self, float Seconds)
{
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	SelfActor->LatentFloat() = Seconds;
	if (Self->StateFrame)
		Self->StateFrame->LatentState = LatentRunState::Sleep;
}

void NActor::Spawn(UObject* Self, UObject* SpawnClass, std::optional<UObject*> SpawnOwner, std::optional<NameString> SpawnTag, std::optional<vec3> SpawnLocation, std::optional<Rotator> SpawnRotation, UObject*& ReturnValue)
{
	ReturnValue = UObject::Cast<UActor>(Self)->Spawn(
		UObject::Cast<UClass>(SpawnClass),
		SpawnOwner ? std::optional<UActor*>(UObject::Cast<UActor>(*SpawnOwner)) : std::optional<UActor*>(),
		SpawnTag,
		SpawnLocation,
		SpawnRotation);
}

void NActor::Subtract_ColorColor(const Color& A, const Color& B, Color& ReturnValue)
{
	Color c;
	c.R = std::max(A.R - (int)B.R, 0);
	c.G = std::max(A.G - (int)B.G, 0);
	c.B = std::max(A.B - (int)B.B, 0);
	c.A = std::max(A.A - (int)B.A, 0);
	ReturnValue = c;
}

void NActor::TouchingActors(UObject* Self, UObject* BaseClass, UObject*& Actor)
{
	Frame::CreatedIterator = std::make_unique<TouchingActorsIterator>(UObject::TryCast<UActor>(Self), ActorClassOrGiven(Self, BaseClass), &Actor);
}

void NActor::Trace(UObject* Self, vec3& HitLocation, vec3& HitNormal, const vec3& TraceEnd, std::optional<vec3> TraceStart, std::optional<bool> bTraceActors, std::optional<vec3> Extent, UObject*& ReturnValue)
{
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	ReturnValue = SelfActor->Trace(
		HitLocation, HitNormal, TraceEnd,
		TraceStart ? *TraceStart : SelfActor->Location(),
		bTraceActors ? *bTraceActors : false,
		Extent ? *Extent : vec3(0, 0, 0));
}

void NActor::TraceActors(UObject* Self, UObject* BaseClass, UObject*& Actor, vec3& HitLoc, vec3& HitNorm, const vec3& End, std::optional<vec3> Start, std::optional<vec3> Extent)
{
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	Frame::CreatedIterator = std::make_unique<TraceActorsIterator>(
		SelfActor, BaseClass, &Actor, &HitLoc, &HitNorm, End,
		Start ? *Start : SelfActor->Location(),
		Extent ? *Extent : vec3(0, 0, 0)); // a line by default, as the original's
}

void NActor::TweenAnim(UObject* Self, const NameString& Sequence, float Time)
{
	UObject::Cast<UActor>(Self)->TweenAnim(Sequence, Time);
}

void NActor::VisibleActors(UObject* Self, UObject* BaseClass, UObject*& Actor, std::optional<float> Radius, std::optional<vec3> Loc)
{
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	// Deus Ex's radius defaults to 0, as the original's: no limit here, 1000
	// for VisibleCollidingActors (the iterators).
	float defaultRadius = 0.0f;
	Frame::CreatedIterator = std::make_unique<VisibleActorsIterator>(
		SelfActor, ActorClassOrGiven(Self, BaseClass), &Actor,
		Radius ? *Radius : defaultRadius,
		Loc ? *Loc : SelfActor->Location());
}

void NActor::VisibleCollidingActors(UObject* Self, UObject* BaseClass, UObject*& Actor, std::optional<float> Radius, std::optional<vec3> Loc, std::optional<bool> bIgnoreHidden)
{
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	float defaultRadius = 0.0f;
	Frame::CreatedIterator = std::make_unique<VisibleCollidingActorsIterator>(
		Self, ActorClassOrGiven(Self, BaseClass), &Actor,
		Radius ? *Radius : defaultRadius,
		Loc ? *Loc : SelfActor->Location(),
		bIgnoreHidden ? *bIgnoreHidden : false);
}

void NActor::GetMeshTexture(UObject* Self, std::optional<int> texnum, UObject*& ReturnValue)
{
	// The original's (Engine.dll 0x103e17c0): the actor's skin of that
	// number (MultiSkins 0 to 7); else, for a number above 0, the mesh's
	// texture of that number; else the actor's Skin; else the mesh's texture
	// of that number, None past its textures; with no mesh a warning and
	// None. DefaultTexture is never its answer.
	UActor* actor = UObject::Cast<UActor>(Self);
	UMesh* mesh = actor ? actor->Mesh() : nullptr;
	int index = texnum ? *texnum : 0;
	ReturnValue = nullptr;
	if (!mesh)
	{
		LogMessage("GetMeshTexture: No mesh");
		return;
	}
	int count = (int)mesh->Textures.size();
	UTexture* tex = actor->GetMultiskin(index);
	if (!tex && index > 0 && index < count)
		tex = mesh->Textures[index];
	if (!tex)
		tex = actor->Skin();
	if (!tex && index >= 0 && index < count)
		tex = mesh->Textures[index];
	ReturnValue = tex;
}

// The original's (Engine.dll AActor::execGetPlayerPawn 0x1036d2a0): the
// first viewport's actor, None with no client (a dedicated server), no
// viewport or no actor in it. DXEngine's fell back to the first player pawn
// with a Player, handing a dedicated server's scripts a remote player's: the
// weapons then took that player for the local one.
void NActor::GetPlayerPawn(UObject* Self, UObject*& ReturnValue)
{
	ReturnValue = engine && engine->viewport ? engine->viewport->Actor() : nullptr;
}

void NActor::AIClearEvent(UObject* Self, const NameString& eventName)
{
	UActor* actor = UObject::Cast<UActor>(Self);
	actor->AIClearEvent(eventName);
}

void NActor::AIClearEventCallback(UObject* Self, const NameString& eventName)
{
	UActor* actor = UObject::Cast<UActor>(Self);
	actor->AIClearEventCallback(eventName);
}

void NActor::AIEndEvent(UObject* Self, const NameString& eventName, uint8_t eventType)
{
	UActor* actor = UObject::Cast<UActor>(Self);
	actor->AIEndEvent(eventName, eventType);
}

void NActor::AIGetLightLevel(UObject* Self, const vec3& Location, float& ReturnValue)
{
	UActor* actor = UObject::Cast<UActor>(Self);
	ReturnValue = actor->AIGetLightLevel(Location);
}

void NActor::AISendEvent(UObject* Self, const NameString& eventName, uint8_t eventType, std::optional<float> Value, std::optional<float> Radius)
{
	UActor* actor = UObject::Cast<UActor>(Self);
	actor->AISendEvent(eventName, eventType, Value, Radius);
}

void NActor::AISetEventCallback(UObject* Self, const NameString& eventName, const NameString& callback, std::optional<NameString> scoreCallback, std::optional<bool> bCheckVisibility, std::optional<bool> bCheckDir, std::optional<bool> bCheckCylinder, std::optional<bool> bCheckLOS)
{
	UActor* actor = UObject::Cast<UActor>(Self);
	actor->AISetEventCallback(eventName, callback, scoreCallback, bCheckVisibility, bCheckDir, bCheckCylinder, bCheckLOS);
}

void NActor::AIStartEvent(UObject* Self, const NameString& eventName, uint8_t eventType, std::optional<float> Value, std::optional<float> Radius)
{
	UActor* actor = UObject::Cast<UActor>(Self);
	actor->AIStartEvent(eventName, eventType, Value, Radius);
}

void NActor::AIVisibility(UObject* Self, std::optional<bool> bIncludeVelocity, float& ReturnValue)
{
	UActor* actor = UObject::Cast<UActor>(Self);
	ReturnValue = actor->AIVisibility(bIncludeVelocity);
}

void NActor::TraceTexture(UObject* Self, UObject* BaseClass, UObject*& Actor, NameString& texName, NameString& texGroup, int& flags, vec3& HitLoc, vec3& HitNorm, const vec3& End, std::optional<vec3> Start, std::optional<vec3> Extent)
{
	// Deus Ex
	auto SelfActor = UObject::Cast<UActor>(Self);
	vec3 realStart = Start ? *Start : SelfActor->Location();
	vec3 realExtent = Extent ? *Extent : vec3(0, 0, 0);

	Frame::CreatedIterator = std::make_unique<TraceTextureIterator>(BaseClass, &Actor, &texName, &texGroup, &flags, &HitLoc, &HitNorm, End, realStart, realExtent);
}

void NActor::TraceVisibleActors(UObject* Self, UObject* BaseClass, UObject*& Actor, vec3& HitLoc, vec3& HitNorm, const vec3& End, std::optional<vec3> Start, std::optional<vec3> Extent)
{
	// Deus Ex
	auto SelfActor = UObject::Cast<UActor>(Self);
	vec3 realStart = Start ? *Start : SelfActor->Location();
	vec3 realExtent = Extent ? *Extent : vec3(0, 0, 0);

	Frame::CreatedIterator = std::make_unique<TraceVisibleActorsIterator>(BaseClass, &Actor, &HitLoc, &HitNorm, End, realStart, realExtent);
}

void NActor::InStasis(UObject* Self, BitfieldBool& ReturnValue)
{
	// Whether the actor is in stasis, not whether stasis is allowed.
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	ReturnValue = SelfActor->InStasis();
}

void NActor::ParabolicTrace(UObject* Self, vec3& finalLocation, std::optional<vec3> startVelocity, std::optional<vec3> startLocation, std::optional<bool> bCheckActors, std::optional<vec3> Cylinder, std::optional<float> maxTime, std::optional<float> elasticity, std::optional<bool> bBounce, std::optional<float> landingSpeed, std::optional<float> granularity, float& ReturnValue)
{
	// The original's (Engine.dll AActor::ParabolicTrace 0x1036c0a0;
	// engine-dll.md, traces): a thrown thing's flight in steps. By default
	// the actor's velocity, location, bCollideActors, collision cylinder and
	// bBounce, 5 s, an elasticity of 0.9, a landing speed of 60, and steps
	// of 0.025 s held between 0.005 and 1. Each step adds the zone's gravity
	// to the velocity and moves by it and the zone's velocity, the speed
	// held to the zone's terminal velocity after (not after a bouncing
	// step's hit). On a hit it lands if the surface is a floor (normal Z
	// above 0.7) and it is not bouncing or is slower than the landing speed;
	// else it bounces: the velocity mirrored about the surface and times the
	// elasticity, the rest of the step slid along it; a second wall met by
	// the slide mirrors it again and turns the slide (TwoWallAdjust) for one
	// more move, a floor met by either landing. It fails -- 0, the start its
	// final location -- on leaving the world, a step longer than about 1,580
	// units, or running out of time, the landing step counted, and on
	// entering water with one step's time. NPCs use it for where a falling grenade lands and whether a
	// throw is safe.
	//
	// Each move is the original's (0x1036ca90), MoveActor's: the step's line
	// traced 2 units past its end with the cylinder, the first hit the
	// level's or a blocking actor's, none of the thrower or what stands on
	// it; a hit ends the move 2 units short of it along the line (no move
	// within 2 units), its time that of the move made over the step. So the
	// thing lands 2 units and the trace's own tenth short of what it meets,
	// as the original's does (ParabolicConsole: once 2.2 units nearer the
	// floor, and a bouncing throw once failed). A move too short to trace
	// keeps the last move's hit, as the original keeps it.
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	ULevel* level = SelfActor->XLevel();
	vec3 start = startLocation.value_or(SelfActor->Location());
	vec3 velocity = startVelocity.value_or(SelfActor->Velocity());
	bool checkActors = bCheckActors.value_or(SelfActor->bCollideActors());
	vec3 cylinder = Cylinder.value_or(vec3(SelfActor->CollisionRadius(), SelfActor->CollisionRadius(), SelfActor->CollisionHeight()));
	float timeLimit = maxTime.value_or(5.0f);
	float bounciness = elasticity.value_or(0.9f);
	bool bouncing = bBounce.value_or(SelfActor->bBounce());
	float landSpeed = landingSpeed.value_or(60.0f);
	float step = std::clamp(granularity.value_or(0.025f), 0.005f, 1.0f);

	vec3 pos = start;
	PointRegion region = engine->LevelInfo->GetLocZone(pos);
	vec3 hitNormal(0.0f);
	float hitTime = 1.0f;

	auto move = [&](const vec3& delta)
	{
		// A move of nothing, each axis under the double 0.0001
		// (X87::NearlyZero3; 0x1036caae-0x1036cb1d), is not made
		if (X87::NearlyZero3(delta))
			return;
		float dist = length(delta);
		vec3 dir = delta * (1.0f / dist);
		vec3 traced = delta + dir * 2.0f;
		hitTime = 1.0f;
		vec3 moved = delta;
		for (const CollisionHit& hit : level->Collision.Trace(pos, pos + traced, cylinder.z, cylinder.x, checkActors, true, false))
		{
			// IsBasedOn: whether the hit actor stands on the thrower, or is it
			if (hit.Actor && (SelfActor->IsBasedOn(hit.Actor) || !hit.Actor->bBlockActors()))
				continue;
			hitNormal = hit.Normal;
			float hitDist = (dist + 2.0f) * hit.Fraction;
			if (hitDist > 2.0f)
			{
				moved = traced * hit.Fraction - dir * 2.0f;
				hitTime = (hitDist - 2.0f) / dist;
			}
			else
			{
				moved = vec3(0.0f);
				hitTime = 0.0f;
			}
			break;
		}
		pos += moved;
		region = engine->LevelInfo->GetLocZone(pos);
	};
	auto landing = [&](const vec3& normal, float speedSquared)
	{
		return normal.z > 0.7f && (!bouncing || landSpeed * landSpeed > speedSquared);
	};

	// The original's loop to the letter, its failures' times with it: on
	// water the step count is set to 0 and then counted, so the time is one
	// step's; at the end's checks it stays 0
	int steps = 0;
	bool done = false;
	while (!done)
	{
		UZoneInfo* zone = region.Zone ? region.Zone : engine->LevelInfo;
		velocity += zone->ZoneGravity() * step;
		vec3 delta = (velocity + zone->ZoneVelocity()) * step;
		move(delta);
		zone = region.Zone ? region.Zone : engine->LevelInfo;
		bool hitThisStep = false;
		if (zone->bWaterZone())
		{
			steps = 0;
			pos = start;
			done = true;
		}
		else if (hitTime < 1.0f)
		{
			hitThisStep = true;
			if (landing(hitNormal, dot(velocity, velocity)))
			{
				done = true;
			}
			else
			{
				vec3 firstNormal = hitNormal;
				velocity = (velocity - hitNormal * (2.0f * dot(velocity, hitNormal))) * bounciness;
				vec3 slide = (delta - hitNormal * dot(delta, hitNormal)) * (1.0f - hitTime);
				if (dot(slide, delta) >= 0.0f)
				{
					move(slide);
					if (hitTime < 1.0f)
					{
						velocity = (velocity - hitNormal * (2.0f * dot(velocity, hitNormal))) * bounciness;
						if (landing(hitNormal, dot(velocity, velocity)))
							done = true;
						vec3 secondNormal = hitNormal;
						float deltaLength = length(delta);
						vec3 desiredDir = deltaLength > 0.0f ? delta * (1.0f / deltaLength) : vec3(0.0f);
						UActor::TwoWallAdjust(desiredDir, slide, secondNormal, firstNormal, hitTime);
						move(slide);
						// The original holds the landing speed against the turned
						// slide's length, a distance of a few units: a floor met
						// by this move lands
						if (!done && hitTime < 1.0f && landing(hitNormal, dot(slide, slide)))
							done = true;
					}
				}
			}
		}
		if (!bouncing || !hitThisStep)
		{
			float terminal = zone->ZoneTerminalVelocity();
			float speedSquared = dot(velocity, velocity);
			if (speedSquared > terminal * terminal)
				velocity = speedSquared > 0.0f ? velocity * (terminal / std::sqrt(speedSquared)) : vec3(0.0f);
		}
		steps++;
		if (region.ZoneNumber == 0 || (float)steps * step > timeLimit || dot(delta, delta) > 2500000.0f)
		{
			steps = 0;
			pos = start;
			done = true;
		}
	}
	finalLocation = pos;
	ReturnValue = (float)steps * step;
}


void NActor::RandomBiasedRotation(
	UObject* Self,
	int centralYaw,
	float yawDistribution,
	int centralPitch,
	float pitchDistribution,
	Rotator& ReturnValue)
{
	ReturnValue = UActor::RandomBiasedRotation(centralYaw, yawDistribution, centralPitch, pitchDistribution);
}

// Deus Ex's menu sliders as they move. The subsystem's own field is what
// changes: its Update() writes the sliders to the device every frame, so a
// value set on the device alone would last one frame.
void NActor::SetInstantMusicVolume(UObject* Self, uint8_t newMusicVolume)
{
	engine->audiodev->MusicVolume = newMusicVolume;
}

void NActor::SetInstantSoundVolume(UObject* Self, uint8_t newSoundVolume)
{
	engine->audiodev->SoundVolume = newSoundVolume;
}

void NActor::SetInstantSpeechVolume(UObject* Self, uint8_t newSpeechVolume)
{
	engine->audiodev->SpeechVolume = newSpeechVolume;
}

void NActor::StopSound_Deus(UObject* Self, int Id)
{
	UActor* SelfActor = UObject::Cast<UActor>(Self);
	engine->audiodev->StopSound(SelfActor, Id);
}

void NActor::TweenBlendAnim(UObject* Self, const NameString& Sequence, float Time, std::optional<int> BlendSlot)
{
	UObject::Cast<UActor>(Self)->TweenBlendAnim(Sequence, Time, BlendSlot ? *BlendSlot : 0);
}
