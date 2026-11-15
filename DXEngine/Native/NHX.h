#pragma once

#include "Packages/Core/UObject.h"

// HX's natives (HX.dll), for the player's own HX.u: registered on HX's
// classes by its script's numbers, and found only when HX's package is
// loaded (dx-reverse-info hx-dll.md).
class NHX
{
public:
	static void RegisterFunctions();

	// Actors (HXActor, HXDecoration, HXPlayerPawn, HXScriptedPawn, HXMutator)
	static void SpawnEx(UObject* Self, UObject* SpawnClass, std::optional<UObject*> SpawnOwner, std::optional<NameString> SpawnTag, std::optional<vec3> SpawnLocation, std::optional<Rotator> SpawnRotation, std::optional<bool> bSpawnNoCollisionFail, std::optional<uint8_t> SpawnPhysics, UObject*& ReturnValue);
	static void TraceTextures(UObject* Self, UObject* BaseClass, UObject*& HitActor, UObject*& Texture, NameString& TextureName, NameString& TextureGroup, int& TextureFlags, vec3& HitLocation, vec3& HitNormal, const vec3& TraceEnd, std::optional<vec3> TraceStart, std::optional<vec3> TraceExtent);
	static void TraceSingleTexture(UObject* Self, UObject* BaseClass, UObject*& Texture, NameString& TextureName, NameString& TextureGroup, int& TextureFlags, vec3& HitLocation, vec3& HitNormal, const vec3& TraceEnd, std::optional<vec3> TraceStart, std::optional<bool> bTraceActors, std::optional<vec3> TraceExtent, UObject*& ReturnValue);
	static void StaticTraceTextures(UObject* Self, UObject* Other, UObject* BaseClass, UObject*& HitActor, UObject*& Texture, NameString& TextureName, NameString& TextureGroup, int& TextureFlags, vec3& HitLocation, vec3& HitNormal, const vec3& TraceEnd, std::optional<vec3> TraceStart, std::optional<vec3> TraceExtent);
	static void StaticTraceSingleTexture(UObject* Self, UObject* Other, UObject* BaseClass, UObject*& Texture, NameString& TextureName, NameString& TextureGroup, int& TextureFlags, vec3& HitLocation, vec3& HitNormal, const vec3& TraceEnd, std::optional<vec3> TraceStart, std::optional<bool> bTraceActors, std::optional<vec3> TraceExtent, UObject*& ReturnValue);
	static void FastRepTrace(UObject* Self, const vec3& TraceEnd, std::optional<vec3> TraceStart, BitfieldBool& ReturnValue);
	static void StaticFastRepTrace(UObject* Self, UObject* LevelInfo, const vec3& TraceEnd, const vec3& TraceStart, BitfieldBool& ReturnValue);
	static void StringToName(UObject* Self, const std::string& S, NameString& ReturnValue);
	static void Strihash(UObject* Self, const std::string& S, int& ReturnValue);
	static void BindConversationList(UObject* Self, const std::string& ConvListClass, UObject* Target, BitfieldBool& ReturnValue);
	static void BindConsersationEvents(UObject* Self, UObject* Con, UObject*& ConBoundActors, UObject* InvokeActor, UObject* Scope);
	static void StopSoundSlot(UObject* Self, uint8_t Slot);
	static void StopAllSoundSlots(UObject* Self);
	static void DemoStopSoundSlot(UObject* Self, uint8_t Slot);
	static void DemoStopAllSoundSlots(UObject* Self);

	// HXPlayerPawn
	static void ExportTravel(UObject* Self, BitfieldBool& ReturnValue);
	static void CompareGameEngine(UObject* Self, UObject* EngineClass, BitfieldBool& ReturnValue);
	static void AddAdditionalView(UObject* Self, UObject* AddMe, BitfieldBool& ReturnValue);
	static void RemoveAdditionalView(UObject* Self, UObject* RemoveMe);
	static void RemoveAllAdditionalViews(UObject* Self);
	static void DuckCallCanBeDropped(UObject* Self, UObject* Item, std::optional<bool> bSilent, BitfieldBool& ReturnValue);
	static void ClientStopSound(UObject* Self, int ActorIndex, int SlotMask);

	// HXMutator
	static void DuckCallSucceeds(UObject* Self, UObject* Successor, UObject* Other, BitfieldBool& ReturnValue);

	// HXFlagReplicationInfo
	static void GetBoolFlagCPP(UObject* Self, const NameString& FlagName, std::optional<bool> bSkipClientFlags, BitfieldBool& ReturnValue);
	static void SetBoolFlagByStringCPP(UObject* Self, const std::string& FlagName, int Value, BitfieldBool& ReturnValue);
	static void SetClientBoolFlagByStringCPP(UObject* Self, const std::string& FlagName, int Value, BitfieldBool& ReturnValue);

	// The windows
	static void WindowScale(UObject* Self, int& ReturnValue);
	static void AddLine(UObject* Self, const std::string& Text, std::optional<Color> TextColor, std::optional<float> Timeout);
	static void ClearLines(UObject* Self);
	static void NumLines(UObject* Self, int& ReturnValue);

	// HXScriptedRenderIterator
	static void CopyRenderProperties(UObject* Self, UObject* Dest, UObject* Source);

	// HXTcpLink, HXUdpLink
	static void Validate(UObject* Self, const std::string& ValidationString, const std::string& BeaconProduct, std::string& ReturnValue);
};
