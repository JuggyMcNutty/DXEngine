#pragma once

#include "UZoneInfo.h"

class UWarpZoneInfo : public UZoneInfo
{
public:
	using UZoneInfo::UZoneInfo;

	void Warp(vec3& Loc, vec3& Vel, Rotator& R);
	void UnWarp(vec3& Loc, vec3& Vel, Rotator& R); // Warp but in reverse?

	ScriptString& Destinations() { return Value<ScriptString>(PropOffsets_WarpZoneInfo.Destinations); }
	UWarpZoneInfo*& OtherSideActor() { return Value<UWarpZoneInfo*>(PropOffsets_WarpZoneInfo.OtherSideActor); }
	UObject*& OtherSideLevel() { return Value<UObject*>(PropOffsets_WarpZoneInfo.OtherSideLevel); }
	ScriptString& OtherSideURL() { return Value<ScriptString>(PropOffsets_WarpZoneInfo.OtherSideURL); }
	NameString& ThisTag() { return Value<NameString>(PropOffsets_WarpZoneInfo.ThisTag); }
	Coords& WarpCoords() { return Value<Coords>(PropOffsets_WarpZoneInfo.WarpCoords); }
	BitfieldBool bNoTeleFrag() { return BoolValue(PropOffsets_WarpZoneInfo.bNoTeleFrag); }
	int& iWarpZone() { return Value<int>(PropOffsets_WarpZoneInfo.iWarpZone); }
	int& numDestinations() { return Value<int>(PropOffsets_WarpZoneInfo.numDestinations); }
};
