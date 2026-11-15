
#include "Precomp.h"
#include "NHX.h"
#include "VM/NativeFunc.h"
#include "VM/Frame.h"
#include "VM/Iterator.h"
#include "VM/ScriptCall.h"
#include "Engine.h"
#include "ObjectTravelInfo.h"
#include "Package/PackageManager.h"
#include "Packages/HX/UHX.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/UFunction.h"
#include "Packages/Core/Properties/UProperty.h"
#include "Packages/Core/Properties/UBoolProperty.h"
#include "Packages/Core/Properties/UObjectProperty.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Info/UPlayerReplicationInfo.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "Packages/Engine/Subsystems/UDXEngineAudioDevice.h"
#include "Network/NetDemo.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/ConSys/UConversation.h"
#include "Packages/ConSys/UConversationList.h"
#include "Packages/IpDrv/UInternetLink.h"
#include "Packages/Extension/Windows/TabGroup/URootWindow.h"
#include "Collision/BottomLevel/TraceRayModel.h"
#include "Utils/StrTools.h"
#include "Utils/Logger.h"

void NHX::RegisterFunctions()
{
	// One code for each actor class's own numbers, as HX.dll's jumps share it
	struct ActorNatives { const char* cls; int traceTextures, traceSingleTexture, fastRepTrace, stopSoundSlot, stopAllSoundSlots, stringToName; };
	static const ActorNatives actors[] =
	{
		{ "HXActor", 3250, 3251, 3254, 3281, 3282, 3260 },
		{ "HXDecoration", 3340, 3341, 3343, 3244, 3245, 0 },
		{ "HXPlayerPawn", 3300, 3301, 3303, 3217, 3218, 3313 },
		{ "HXScriptedPawn", 3321, 3322, 3324, 3225, 3226, 3320 },
	};
	for (const ActorNatives& a : actors)
	{
		RegisterVMNativeFunc_11(a.cls, "TraceTextures", &NHX::TraceTextures, a.traceTextures);
		RegisterVMNativeFunc_12(a.cls, "TraceSingleTexture", &NHX::TraceSingleTexture, a.traceSingleTexture);
		RegisterVMNativeFunc_3(a.cls, "FastRepTrace", &NHX::FastRepTrace, a.fastRepTrace);
		RegisterVMNativeFunc_1(a.cls, "StopSoundSlot", &NHX::StopSoundSlot, a.stopSoundSlot);
		RegisterVMNativeFunc_0(a.cls, "StopAllSoundSlots", &NHX::StopAllSoundSlots, a.stopAllSoundSlots);
		RegisterVMNativeFunc_2(a.cls, "StringToName", &NHX::StringToName, a.stringToName);
		RegisterVMNativeFunc_1(a.cls, "DemoStopSoundSlot", &NHX::DemoStopSoundSlot, 0);
		RegisterVMNativeFunc_0(a.cls, "DemoStopAllSoundSlots", &NHX::DemoStopAllSoundSlots, 0);
	}
	RegisterVMNativeFunc_8("HXActor", "SpawnEx", &NHX::SpawnEx, 3200);
	RegisterVMNativeFunc_12("HXActor", "StaticTraceTextures", &NHX::StaticTraceTextures, 3252);
	RegisterVMNativeFunc_13("HXActor", "StaticTraceSingleTexture", &NHX::StaticTraceSingleTexture, 3253);
	RegisterVMNativeFunc_4("HXActor", "StaticFastRepTrace", &NHX::StaticFastRepTrace, 3256);
	RegisterVMNativeFunc_2("HXActor", "Strihash", &NHX::Strihash, 3261);
	RegisterVMNativeFunc_3("HXActor", "BindConversationList", &NHX::BindConversationList, 3270);
	RegisterVMNativeFunc_4("HXActor", "BindConsersationEvents", &NHX::BindConsersationEvents, 3271);

	RegisterVMNativeFunc_1("HXPlayerPawn", "ExportTravel", &NHX::ExportTravel, 3311);
	RegisterVMNativeFunc_2("HXPlayerPawn", "CompareGameEngine", &NHX::CompareGameEngine, 3312);
	RegisterVMNativeFunc_2("HXPlayerPawn", "AddAdditionalView", &NHX::AddAdditionalView, 3314);
	RegisterVMNativeFunc_1("HXPlayerPawn", "RemoveAdditionalView", &NHX::RemoveAdditionalView, 3315);
	RegisterVMNativeFunc_0("HXPlayerPawn", "RemoveAllAdditionalViews", &NHX::RemoveAllAdditionalViews, 3316);
	RegisterVMNativeFunc_3("HXPlayerPawn", "DuckCallCanBeDropped", &NHX::DuckCallCanBeDropped, 3317);
	RegisterVMNativeFunc_2("HXPlayerPawn", "ClientStopSound", &NHX::ClientStopSound, 0);

	RegisterVMNativeFunc_2("HXMutator", "StringToName", &NHX::StringToName, 3360);
	RegisterVMNativeFunc_8("HXMutator", "SpawnEx", &NHX::SpawnEx, 3361);
	RegisterVMNativeFunc_3("HXMutator", "DuckCallSucceeds", &NHX::DuckCallSucceeds, 3362);

	RegisterVMNativeFunc_3("HXFlagReplicationInfo", "GetBoolFlagCPP", &NHX::GetBoolFlagCPP, 3410);
	RegisterVMNativeFunc_3("HXFlagReplicationInfo", "SetBoolFlagByStringCPP", &NHX::SetBoolFlagByStringCPP, 3411);
	RegisterVMNativeFunc_3("HXFlagReplicationInfo", "SetClientBoolFlagByStringCPP", &NHX::SetClientBoolFlagByStringCPP, 3412);

	RegisterVMNativeFunc_1("HXWindow", "WindowScale", &NHX::WindowScale, 3500);
	RegisterVMNativeFunc_1("HXButtonWindow", "WindowScale", &NHX::WindowScale, 3501);
	RegisterVMNativeFunc_1("HXToggleWindow", "WindowScale", &NHX::WindowScale, 3502);
	RegisterVMNativeFunc_1("HXTextLogWindow", "WindowScale", &NHX::WindowScale, 0);
	RegisterVMNativeFunc_3("HXTextLogWindow", "AddLine", &NHX::AddLine, 0);
	RegisterVMNativeFunc_0("HXTextLogWindow", "ClearLines", &NHX::ClearLines, 0);
	RegisterVMNativeFunc_1("HXTextLogWindow", "NumLines", &NHX::NumLines, 0);

	RegisterVMNativeFunc_2("HXScriptedRenderIterator", "CopyRenderProperties", &NHX::CopyRenderProperties, 3100);

	RegisterVMNativeFunc_3("HXTcpLink", "Validate", &NHX::Validate, 0);
	RegisterVMNativeFunc_3("HXUdpLink", "Validate", &NHX::Validate, 0);
}

/////////////////////////////////////////////////////////////////////////////
// Spawning

// HX.dll 0x1001b850: Actor.Spawn with no fit test when asked, the class's
// defaults starting in the physics asked for (its default's own, else none)
// for the spawn's events, and the tag given set after them, None when none
// was.
void NHX::SpawnEx(UObject* Self, UObject* SpawnClass, std::optional<UObject*> SpawnOwner, std::optional<NameString> SpawnTag, std::optional<vec3> SpawnLocation, std::optional<Rotator> SpawnRotation, std::optional<bool> bSpawnNoCollisionFail, std::optional<uint8_t> SpawnPhysics, UObject*& ReturnValue)
{
	UActor* self = UObject::Cast<UActor>(Self);
	UClass* cls = UObject::TryCast<UClass>(SpawnClass);
	UActor* defaults = cls ? UObject::TryCast<UActor>(cls->GetDefaultObject<UObject>()) : nullptr;
	if (cls && !defaults)
	{
		ReturnValue = nullptr;
		return;
	}
	uint8_t physics = SpawnPhysics ? *SpawnPhysics : (defaults ? defaults->Physics() : (uint8_t)PHYS_None);
	uint8_t savedPhysics = defaults ? defaults->Physics() : (uint8_t)PHYS_None;
	if (defaults)
		defaults->Physics() = physics;
	UActor* owner = SpawnOwner ? UObject::TryCast<UActor>(*SpawnOwner) : nullptr;
	UActor* spawned = self->Spawn(cls, owner, std::nullopt,
		SpawnLocation ? *SpawnLocation : self->Location(),
		SpawnRotation ? *SpawnRotation : self->Rotation(),
		bSpawnNoCollisionFail && *bSpawnNoCollisionFail);
	if (spawned)
		spawned->Tag() = SpawnTag ? *SpawnTag : NameString();
	if (defaults)
		defaults->Physics() = savedPhysics;
	ReturnValue = spawned;
}

/////////////////////////////////////////////////////////////////////////////
// Traces

// HX's MultiLineCheck (as Deus Ex's): the level's one hit, then the actors
// along the line, up to 64; the level checked only with a LevelInfo given,
// its hit given as that LevelInfo
static CollisionHitList HXMultiLineCheck(ULevel* level, ULevelInfo* levelInfo, const vec3& start, const vec3& end, const vec3& extent)
{
	CollisionHitList result;
	if (!level)
		return result;
	float radius = std::abs(extent.x);
	float height = std::abs(extent.z);
	int count = 0;
	for (CollisionHit hit : level->Collision.Trace(start, end, height, radius, true, levelInfo != nullptr, false))
	{
		if (count++ >= 64)
			break;
		if (!hit.Actor)
			hit.Actor = levelInfo;
		result.push_back(hit);
	}
	return result;
}

// UHXLevel::FindTexture (HX.dll 0x100529a0): a level's hit gives the surface
// of the polygon the line crossed, a mover's the surface of its brush's node
// it crossed; anything else no texture and flags 0
static UTexture* HXFindTexture(ULevel* level, ULevelInfo* levelInfo, const CollisionHit& hit, int& flags)
{
	flags = 0;
	if (!hit.Node || hit.Node->Surf < 0)
		return nullptr;
	if (levelInfo && hit.Actor == levelInfo)
	{
		const BspSurface& surface = level->Collision.LineSurface(hit.Node->Surf);
		flags = (int)surface.PolyFlags;
		return surface.Material;
	}
	UMover* mover = UObject::TryCast<UMover>(hit.Actor);
	UModel* brush = mover ? mover->Brush() : nullptr;
	if (brush && (size_t)hit.Node->Surf < brush->Surfaces.size())
	{
		const BspSurface& surface = brush->Surfaces[hit.Node->Surf];
		flags = (int)surface.PolyFlags;
		return surface.Material;
	}
	return nullptr;
}

// The texture's group: its outer's name when it is in a group, None when it
// is in its package alone, where the game's names the package
static NameString HXTextureGroup(UTexture* texture)
{
	return texture && texture->Outer() ? texture->Outer()->Name : NameString();
}

class HXTraceTexturesIterator : public Iterator
{
public:
	HXTraceTexturesIterator(ULevel* level, ULevelInfo* levelInfo, UObject** hitActor, UObject** texture, NameString* textureName, NameString* textureGroup, int* textureFlags, vec3* hitLocation, vec3* hitNormal, const vec3& start, const vec3& end, const vec3& extent)
		: level(level), levelInfo(levelInfo), hitActor(hitActor), texture(texture), textureName(textureName), textureGroup(textureGroup), textureFlags(textureFlags), hitLocation(hitLocation), hitNormal(hitNormal)
	{
		hits = HXMultiLineCheck(level, levelInfo, start, end, extent);
		it = hits.begin();
	}

	bool Next() override
	{
		if (it == hits.end())
		{
			// The actor and the four texture outputs cleared at the end
			*hitActor = nullptr;
			*texture = nullptr;
			*textureName = NameString();
			*textureGroup = NameString();
			*textureFlags = 0;
			return false;
		}
		const CollisionHit& hit = *it;
		++it;
		*hitActor = hit.Actor;
		*hitLocation = hit.Location;
		*hitNormal = hit.Normal;
		UTexture* found = HXFindTexture(level, levelInfo, hit, *textureFlags);
		*texture = found;
		*textureName = found ? found->Name : NameString();
		*textureGroup = HXTextureGroup(found);
		return true;
	}

	void Mark(GCMarker& marker) override
	{
		for (const CollisionHit& hit : hits)
			marker.MarkConst(hit.Actor);
	}

private:
	ULevel* level;
	ULevelInfo* levelInfo;
	UObject** hitActor;
	UObject** texture;
	NameString* textureName;
	NameString* textureGroup;
	int* textureFlags;
	vec3* hitLocation;
	vec3* hitNormal;
	CollisionHitList hits;
	CollisionHitList::const_iterator it;
};

// HX.dll 0x1001a900: TraceTexture's walk of every hit, with the texture
// object too, the polygon's surface and a mover's
void NHX::TraceTextures(UObject* Self, UObject* BaseClass, UObject*& HitActor, UObject*& Texture, NameString& TextureName, NameString& TextureGroup, int& TextureFlags, vec3& HitLocation, vec3& HitNormal, const vec3& TraceEnd, std::optional<vec3> TraceStart, std::optional<vec3> TraceExtent)
{
	UActor* self = UObject::Cast<UActor>(Self);
	vec3 start = TraceStart ? *TraceStart : self->Location();
	vec3 extent = TraceExtent ? *TraceExtent : vec3(0.0f);
	Frame::CreatedIterator = std::make_unique<HXTraceTexturesIterator>(self->XLevel(), self->Level(), &HitActor, &Texture, &TextureName, &TextureGroup, &TextureFlags, &HitLocation, &HitNormal, start, TraceEnd, extent);
}

// HX.dll 0x1001acf0: the same through Other's level, but with the calling
// context's LevelInfo: called through HXActor's class, as HX's callers do,
// that is the defaults' None, and the walls are not checked
void NHX::StaticTraceTextures(UObject* Self, UObject* Other, UObject* BaseClass, UObject*& HitActor, UObject*& Texture, NameString& TextureName, NameString& TextureGroup, int& TextureFlags, vec3& HitLocation, vec3& HitNormal, const vec3& TraceEnd, std::optional<vec3> TraceStart, std::optional<vec3> TraceExtent)
{
	UActor* other = UObject::TryCast<UActor>(Other);
	if (!other)
	{
		LogMessage("StaticTraceTextures() failed: Other is None.");
		Frame::CreatedIterator = std::make_unique<EmptyIterator>();
		return;
	}
	UActor* self = UObject::TryCast<UActor>(Self);
	vec3 start = TraceStart ? *TraceStart : other->Location();
	vec3 extent = TraceExtent ? *TraceExtent : vec3(0.0f);
	Frame::CreatedIterator = std::make_unique<HXTraceTexturesIterator>(other->XLevel(), self ? self->Level() : nullptr, &HitActor, &Texture, &TextureName, &TextureGroup, &TextureFlags, &HitLocation, &HitNormal, start, TraceEnd, extent);
}

// The first hit and its texture, from the tracer
static UActor* HXTraceSingle(UActor* tracer, UObject*& Texture, NameString& TextureName, NameString& TextureGroup, int& TextureFlags, vec3& HitLocation, vec3& HitNormal, const vec3& start, const vec3& end, bool traceActors, const vec3& extent)
{
	Texture = nullptr;
	TextureName = NameString();
	TextureGroup = NameString();
	TextureFlags = 0;
	ULevel* level = tracer->XLevel();
	TraceFlags flags;
	flags.movers = true;
	flags.world = true;
	if (traceActors)
	{
		flags.pawns = true;
		flags.others = true;
		flags.onlyProjectiles = true;
	}
	CollisionHit hit = level->Collision.TraceFirstHit(start, end, tracer, extent, flags);
	if (!hit.Actor && !hit.Node)
	{
		HitLocation = vec3(0.0f);
		HitNormal = vec3(0.0f);
		return nullptr;
	}
	if (!hit.Actor)
		hit.Actor = tracer->Level();
	HitLocation = hit.Location;
	HitNormal = hit.Normal;
	UTexture* found = HXFindTexture(level, tracer->Level(), hit, TextureFlags);
	Texture = found;
	TextureName = found ? found->Name : NameString();
	TextureGroup = HXTextureGroup(found);
	return hit.Actor;
}

// HX.dll 0x1001b170: Actor.Trace's first hit, actors by default as the
// actor collides with them, with its texture
void NHX::TraceSingleTexture(UObject* Self, UObject* BaseClass, UObject*& Texture, NameString& TextureName, NameString& TextureGroup, int& TextureFlags, vec3& HitLocation, vec3& HitNormal, const vec3& TraceEnd, std::optional<vec3> TraceStart, std::optional<bool> bTraceActors, std::optional<vec3> TraceExtent, UObject*& ReturnValue)
{
	UActor* self = UObject::Cast<UActor>(Self);
	ReturnValue = HXTraceSingle(self, Texture, TextureName, TextureGroup, TextureFlags, HitLocation, HitNormal,
		TraceStart ? *TraceStart : self->Location(), TraceEnd,
		bTraceActors ? *bTraceActors : (bool)self->bCollideActors(),
		TraceExtent ? *TraceExtent : vec3(0.0f));
}

// HX.dll 0x1001b4a0: the same with Other the tracer
void NHX::StaticTraceSingleTexture(UObject* Self, UObject* Other, UObject* BaseClass, UObject*& Texture, NameString& TextureName, NameString& TextureGroup, int& TextureFlags, vec3& HitLocation, vec3& HitNormal, const vec3& TraceEnd, std::optional<vec3> TraceStart, std::optional<bool> bTraceActors, std::optional<vec3> TraceExtent, UObject*& ReturnValue)
{
	UActor* other = UObject::TryCast<UActor>(Other);
	if (!other)
	{
		LogMessage("StaticTraceSingleTexture() failed: Other is None.");
		ReturnValue = nullptr;
		return;
	}
	if (!other->XLevel())
		Exception::Throw("Assertion failed: Other->GetLevel() [File:HXActor.cpp] [Line: 338]");
	ReturnValue = HXTraceSingle(other, Texture, TextureName, TextureGroup, TextureFlags, HitLocation, HitNormal,
		TraceStart ? *TraceStart : other->Location(), TraceEnd,
		bTraceActors ? *bTraceActors : (bool)other->bCollideActors(),
		TraceExtent ? *TraceExtent : vec3(0.0f));
}

// UHXModel::FastRepLineCheck (HX.dll 0x100524b0) down a level's BSP, the
// movers filed in: whether the line is clear, seeing through invisible,
// masked, translucent and modulated surfaces
static bool HXFastRepLineCheck(ULevel* level, const vec3& end, const vec3& start)
{
	if (!level)
		return true;
	CollisionSystem& collision = level->Collision;
	return TraceRayModel::FastWalkSeeThrough(collision.LineNodes(), collision.LineRootOutside(),
		[&](int surf) { return collision.LineSurface(surf).PolyFlags; }, start, end);
}

void NHX::FastRepTrace(UObject* Self, const vec3& TraceEnd, std::optional<vec3> TraceStart, BitfieldBool& ReturnValue)
{
	UActor* self = UObject::Cast<UActor>(Self);
	ReturnValue = HXFastRepLineCheck(self->XLevel(), TraceEnd, TraceStart ? *TraceStart : self->Location());
}

void NHX::StaticFastRepTrace(UObject* Self, UObject* LevelInfo, const vec3& TraceEnd, const vec3& TraceStart, BitfieldBool& ReturnValue)
{
	ULevelInfo* info = UObject::TryCast<ULevelInfo>(LevelInfo);
	if (!info)
	{
		LogMessage("StaticFastRepTrace() failed: Level is None.");
		return;
	}
	ReturnValue = HXFastRepLineCheck(info->XLevel(), TraceEnd, TraceStart);
}

/////////////////////////////////////////////////////////////////////////////
// Strings

// Extension's StringToName, as HX's jumps there (HX.dll 0x1001ea10)
void NHX::StringToName(UObject* Self, const std::string& S, NameString& ReturnValue)
{
	ReturnValue = NameString(S);
}

// Core's appStrihash (HX.dll 0x1001c4d0)
void NHX::Strihash(UObject* Self, const std::string& S, int& ReturnValue)
{
	ReturnValue = (int)StrTools::ue1_strihash(S);
}

/////////////////////////////////////////////////////////////////////////////
// Conversations

// HX.dll 0x1001ba40: the list it names, loaded without a warning, bound to
// the actor as the game binds a level's list
void NHX::BindConversationList(UObject* Self, const std::string& ConvListClass, UObject* Target, BitfieldBool& ReturnValue)
{
	ReturnValue = false;
	UActor* target = UObject::TryCast<UActor>(Target);
	UConversationList* list = nullptr;
	size_t dot = ConvListClass.find('.');
	if (dot != std::string::npos && dot != 0)
	{
		try
		{
			UClass* listClass = engine->packages->FindClass("ConSys.ConversationList");
			list = UObject::TryCast<UConversationList>(engine->packages->GetPackage(ConvListClass.substr(0, dot))->LoadObjectAnyGroup(listClass, ConvListClass.substr(ConvListClass.rfind('.') + 1)));
		}
		catch (...)
		{
		}
	}
	if (!list || !target)
		return;
	target->BindConversations(list);
	ReturnValue = true;
}

// HX.dll 0x1001bbc0: the conversation's events bound in the level given
void NHX::BindConsersationEvents(UObject* Self, UObject* Con, UObject*& ConBoundActors, UObject* InvokeActor, UObject* Scope)
{
	UConversation* con = UObject::TryCast<UConversation>(Con);
	ULevel* scope = UObject::TryCast<ULevel>(Scope);
	if (!con)
		Exception::Throw("Assertion failed: Con [File:HXActor.cpp] [Line: 431]");
	if (!scope)
		Exception::Throw("Assertion failed: Scope [File:HXActor.cpp] [Line: 432]");
	con->BindEvents(&ConBoundActors, InvokeActor, scope);
}

/////////////////////////////////////////////////////////////////////////////
// Sound slots

static bool HXCalledFromSimulated()
{
	if (Frame::Callstack.size() < 2)
		return false;
	UFunction* caller = UObject::TryCast<UFunction>(Frame::Callstack[Frame::Callstack.size() - 2]->Func);
	return caller && AllFlags(caller->FuncFlags, FunctionFlags::Simulated);
}

static bool HXRecordsDemo(UActor* actor)
{
	return engine->LevelDemoDriver && !engine->LevelDemoDriver->ServerConnection && actor->XLevel() == engine->Level;
}

static void HXClientStopSound(UPawn* player, UActor* actor, int mask)
{
	int index = engine->audiodev->SoundId(actor, 0) / 16;
	CallEvent(player, NameString("ClientStopSound"), { ExpressionValue::IntValue(index), ExpressionValue::IntValue(mask) });
}

// HX.dll 0x1001bdd0: the demo the level records first, then the slots
// stopped for every HX player that hears the actor's sounds -- on a client,
// or from a simulated function, the viewport's player in the actor's level;
// otherwise every player, a remote one's stop sent to its client
static void HXStopSlots(UActor* self, int mask, std::optional<uint8_t> slot)
{
	ULevelInfo* info = self->Level();
	if (!info)
		return;
	if (HXRecordsDemo(self) && info->NetMode() != NM_Client)
	{
		if (slot)
			CallEvent(self, NameString("DemoStopSoundSlot"), { ExpressionValue::ByteValue(*slot) });
		else
			CallEvent(self, NameString("DemoStopAllSoundSlots"));
	}
	if (info->NetMode() == NM_Client || HXCalledFromSimulated())
	{
		UPlayerPawn* player = engine->viewport ? engine->viewport->Actor() : nullptr;
		if (player && player->XLevel() == self->XLevel() && UObject::TryCast<UHXPlayerPawn>(player))
			HXClientStopSound(player, self, mask);
		return;
	}
	for (UPawn* pawn = info->PawnList(); pawn; pawn = pawn->nextPawn())
	{
		if (pawn->bIsPlayer() && UObject::TryCast<UHXPlayerPawn>(pawn))
			HXClientStopSound(pawn, self, mask);
	}
}

void NHX::StopSoundSlot(UObject* Self, uint8_t Slot)
{
	HXStopSlots(UObject::Cast<UActor>(Self), 1 << (Slot & 31), Slot);
}

void NHX::StopAllSoundSlots(UObject* Self)
{
	HXStopSlots(UObject::Cast<UActor>(Self), -1, std::nullopt);
}

// HX.dll 0x1001c230, 0x1001c390: in a demo, the viewport's HX player stops
// them, as DemoPlaySound plays them
void NHX::DemoStopSoundSlot(UObject* Self, uint8_t Slot)
{
	UActor* self = UObject::Cast<UActor>(Self);
	UPlayerPawn* player = engine->viewport ? engine->viewport->Actor() : nullptr;
	if (player && player->XLevel() == self->XLevel() && UObject::TryCast<UHXPlayerPawn>(player))
		HXClientStopSound(player, self, 1 << (Slot & 31));
}

void NHX::DemoStopAllSoundSlots(UObject* Self)
{
	UActor* self = UObject::Cast<UActor>(Self);
	UPlayerPawn* player = engine->viewport ? engine->viewport->Actor() : nullptr;
	if (player && player->XLevel() == self->XLevel() && UObject::TryCast<UHXPlayerPawn>(player))
		HXClientStopSound(player, self, -1);
}

// HX.dll 0x100211b0: the viewport's own player stops the sounds of each slot
// of the mask, by the ID the server played them with
void NHX::ClientStopSound(UObject* Self, int ActorIndex, int SlotMask)
{
	UPlayerPawn* player = UObject::TryCast<UPlayerPawn>(Self);
	if (!player || !engine->viewport || engine->viewport->Actor() != player || !engine->audiodev || engine->NoAudioSubsystem)
		return;
	for (int slot = 0; slot < 7; slot++)
	{
		if (SlotMask & (1 << slot))
			engine->audiodev->StopSoundIdAnyOverride(ActorIndex * 16 + slot * 2);
	}
}

/////////////////////////////////////////////////////////////////////////////
// The player

// HX.dll 0x10020d90: a player leaving keeps its items in the level's travel,
// under its name, for a later login by that name; nothing returned
void NHX::ExportTravel(UObject* Self, BitfieldBool& ReturnValue)
{
	UPlayerPawn* self = UObject::Cast<UPlayerPawn>(Self);
	ULevel* level = self->XLevel();
	UPlayerReplicationInfo* pri = self->PlayerReplicationInfo();
	if (!level || !pri)
		return;
	std::string name = ScriptToUTF8(pri->PlayerName());
	if (name.empty())
		return;
	LogMessage("Export travel for: " + name);
	std::string text = ActorTravelInfo::Create(self, true, true);
	for (auto it = level->TravelInfo.begin(); it != level->TravelInfo.end();)
	{
		if (StrTools::equals_ignore_case(it->first, name))
			it = level->TravelInfo.erase(it);
		else
			++it;
	}
	level->TravelInfo[name] = text;
}

// HX.dll 0x10020cb0: whether the class is the engine's own. DXEngine's is
// the ini's GameEngine= class, whose part it plays.
void NHX::CompareGameEngine(UObject* Self, UObject* EngineClass, BitfieldBool& ReturnValue)
{
	UClass* cls = UObject::TryCast<UClass>(EngineClass);
	std::string gameEngine = engine->packages->GetIniValue("system", "Engine.Engine", "GameEngine", "Engine.GameEngine");
	ReturnValue = cls && StrTools::equals_ignore_case(cls->GetPathName(), gameEngine);
}

void NHX::AddAdditionalView(UObject* Self, UObject* AddMe, BitfieldBool& ReturnValue)
{
	ReturnValue = UObject::Cast<UPlayerPawn>(Self)->AddAdditionalView(UObject::TryCast<UActor>(AddMe));
}

void NHX::RemoveAdditionalView(UObject* Self, UObject* RemoveMe)
{
	UObject::Cast<UPlayerPawn>(Self)->RemoveAdditionalView(UObject::TryCast<UActor>(RemoveMe));
}

void NHX::RemoveAllAdditionalViews(UObject* Self)
{
	UPlayerPawn* self = UObject::Cast<UPlayerPawn>(Self);
	if (PropOffsets_PlayerPawn.AdditionalViews.DataOffset == ~(size_t)0)
		return;
	for (UActor*& view : self->AdditionalViews())
		view = nullptr;
}

// Whether a property is a bool, as a duck call's signature test asks
static bool IsBoolProperty(UProperty* property)
{
	return property && UObject::TryCast<UBoolProperty>(property) != nullptr;
}

// HX.dll 0x10021360: the item's CanBeDropped asked, with False, when it has
// one returning a bool from two parameters, the first a bool; true otherwise
void NHX::DuckCallCanBeDropped(UObject* Self, UObject* Item, std::optional<bool> bSilent, BitfieldBool& ReturnValue)
{
	ReturnValue = true;
	if (!Item)
		return;
	UFunction* func = FindEventFunction(Item, NameString("CanBeDropped"));
	if (!func)
		return;
	Array<UProperty*> params;
	UProperty* returnProperty = nullptr;
	for (UProperty* property : func->Properties)
	{
		if (AllFlags(property->PropFlags, PropertyFlags::ReturnParm))
			returnProperty = property;
		else if (AllFlags(property->PropFlags, PropertyFlags::Parm))
			params.push_back(property);
	}
	if (!IsBoolProperty(returnProperty) || params.size() != 2 || !IsBoolProperty(params[0]))
		return;
	ReturnValue = CallEvent(Item, NameString("CanBeDropped"), { ExpressionValue::BoolValue(false) }).ToBool();
}

/////////////////////////////////////////////////////////////////////////////
// The mutator

// HX.dll 0x1001e8b0: the successor's Succeeds(Other), when it has one of no
// return and one parameter of exactly class Actor; its result never set
void NHX::DuckCallSucceeds(UObject* Self, UObject* Successor, UObject* Other, BitfieldBool& ReturnValue)
{
	if (!Successor)
		return;
	UFunction* func = FindEventFunction(Successor, NameString("Succeeds"));
	if (!func)
		return;
	Array<UProperty*> params;
	bool returns = false;
	for (UProperty* property : func->Properties)
	{
		if (AllFlags(property->PropFlags, PropertyFlags::ReturnParm))
			returns = true;
		else if (AllFlags(property->PropFlags, PropertyFlags::Parm))
			params.push_back(property);
	}
	if (returns || params.size() != 1)
		return;
	UObjectProperty* param = UObject::TryCast<UObjectProperty>(params[0]);
	if (!param || !param->ObjectClass || param->ObjectClass->Name != NameString("Actor"))
		return;
	CallEvent(Successor, NameString("Succeeds"), { ExpressionValue::ObjectValue(Other) });
}

/////////////////////////////////////////////////////////////////////////////
// HX's flags

// A flag is its name's appStrihash alone. The server's 16 collections of 9
// ints hold eight flags each, the hashes in 1..8 and their values the bits
// 1..8 of the first; the client's 16 of 17 ints nine, the hashes in 0..8 and
// their values the bits 0..8 of the last (HX.dll 0x10025020).
static int* HXServerCollection(UObject* self, int collection)
{
	return &self->Value<int>(HXProps().FlagCollections[collection]);
}

static int* HXClientCollection(UObject* self, int collection)
{
	return &self->Value<int>(HXProps().ClientFlagCollections) + collection * 17;
}

void NHX::GetBoolFlagCPP(UObject* Self, const NameString& FlagName, std::optional<bool> bSkipClientFlags, BitfieldBool& ReturnValue)
{
	int hash = (int)StrTools::ue1_strihash(FlagName.ToString());
	for (int c = 0; c < 16; c++)
	{
		int* ints = HXServerCollection(Self, c);
		for (int j = 1; j < 9; j++)
		{
			if (ints[j] == hash)
			{
				ReturnValue = ((1 << j) & ints[0]) != 0;
				return;
			}
		}
	}
	if (!bSkipClientFlags || !*bSkipClientFlags)
	{
		for (int c = 0; c < 16; c++)
		{
			int* ints = HXClientCollection(Self, c);
			for (int m = 0; m < 9; m++)
			{
				if (ints[m] == hash)
				{
					ReturnValue = ((1 << m) & ints[16]) != 0;
					return;
				}
			}
		}
	}
	ReturnValue = false;
}

// HX.dll 0x100252e0: its bit set or cleared where the flag is, else in the
// first empty slot; none left is logged and false
void NHX::SetBoolFlagByStringCPP(UObject* Self, const std::string& FlagName, int Value, BitfieldBool& ReturnValue)
{
	int hash = (int)StrTools::ue1_strihash(FlagName);
	for (int pass = 0; pass < 2; pass++)
	{
		for (int c = 0; c < 16; c++)
		{
			int* ints = HXServerCollection(Self, c);
			for (int j = 1; j < 9; j++)
			{
				if (ints[j] == (pass == 0 ? hash : 0))
				{
					ints[j] = hash;
					ints[0] = Value != 0 ? (ints[0] | (1 << j)) : (ints[0] & ~(1 << j));
					ReturnValue = true;
					return;
				}
			}
		}
	}
	LogMessage("ERROR: No empty flag slot in HHXFlagReplicationInfo. REPORT THIS AS A BUG.");
	ReturnValue = false;
}

// HX.dll 0x100255a0: the same on the client's collections, unlogged
void NHX::SetClientBoolFlagByStringCPP(UObject* Self, const std::string& FlagName, int Value, BitfieldBool& ReturnValue)
{
	int hash = (int)StrTools::ue1_strihash(FlagName);
	for (int pass = 0; pass < 2; pass++)
	{
		for (int c = 0; c < 16; c++)
		{
			int* ints = HXClientCollection(Self, c);
			for (int m = 0; m < 9; m++)
			{
				if (ints[m] == (pass == 0 ? hash : 0))
				{
					ints[m] = hash;
					ints[16] = Value != 0 ? (ints[16] | (1 << m)) : (ints[16] & ~(1 << m));
					ReturnValue = true;
					return;
				}
			}
		}
	}
	ReturnValue = false;
}

/////////////////////////////////////////////////////////////////////////////
// The windows

// HX.dll 0x10014fe0: the root's multiplier, 1 without a root
void NHX::WindowScale(UObject* Self, int& ReturnValue)
{
	URootWindow* root = UObject::Cast<UWindow>(Self)->GetRootWindow();
	ReturnValue = root ? root->hMultiplier() : 1;
}

void NHX::AddLine(UObject* Self, const std::string& Text, std::optional<Color> TextColor, std::optional<float> Timeout)
{
	UObject::Cast<UHXTextLogWindow>(Self)->AddLine(Text, TextColor, Timeout);
}

void NHX::ClearLines(UObject* Self)
{
	UObject::Cast<UHXTextLogWindow>(Self)->ClearLog();
}

void NHX::NumLines(UObject* Self, int& ReturnValue)
{
	ReturnValue = UObject::Cast<UHXTextLogWindow>(Self)->NumLines();
}

/////////////////////////////////////////////////////////////////////////////

void NHX::CopyRenderProperties(UObject* Self, UObject* Dest, UObject* Source)
{
	UHXRenderIterator::CopyRenderProperties(UObject::TryCast<UActor>(Dest), UObject::TryCast<UActor>(Source));
}

/////////////////////////////////////////////////////////////////////////////

// HX.dll 0x10017610: a challenge of exactly six characters answered with
// HX's secret, "hx" (the beacon's product) and GameSpy's test products by
// name in any case, six spaces for any other
void NHX::Validate(UObject* Self, const std::string& ValidationString, const std::string& BeaconProduct, std::string& ReturnValue)
{
	ReturnValue.clear();
	if (ValidationString.size() != 6)
		return;
	std::string key = "      ";
	if (StrTools::equals_ignore_case(BeaconProduct, "hx"))
		key = "5YKJVt";
	else if (StrTools::equals_ignore_case(BeaconProduct, "gmtest") || StrTools::equals_ignore_case(BeaconProduct, "gmtestam") || StrTools::equals_ignore_case(BeaconProduct, "gmtestcd") || StrTools::equals_ignore_case(BeaconProduct, "gmtestcdam"))
		key = "HA6zkS";
	ReturnValue = UInternetLink::GameSpyAnswer(ValidationString, key);
}
