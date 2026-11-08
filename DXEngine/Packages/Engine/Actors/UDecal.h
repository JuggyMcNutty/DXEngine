#pragma once

#include "UActor.h"

class BspNode;
class ULevel;
class UMover;
struct LevelDecal;

class UDecal : public UActor
{
public:
	using UActor::UActor;

	UObject* AttachDecal(float traceDistance, vec3 decalDir);
	void DetachDecal();

	// A surface's base and normal in the world, a level surface's or a
	// mover's filed polygon's, with the polygon's mover
	static bool SurfaceFrame(ULevel* level, int surf, vec3& base, vec3& normal, UMover** mover = nullptr);

	// Off the BSP nodes it is drawn on.
	void OnGCDestroy() override;

	float& LastRenderedTime() { return Value<float>(PropOffsets_Decal.LastRenderedTime); }
	int& MultiDecalLevel() { return Value<int>(PropOffsets_Decal.MultiDecalLevel); }
	TypedScriptArray<void*> SurfList() { return DynamicArray<void*>(PropOffsets_Decal.SurfList); }

	// The level's nodes it lies on, the movers' polygons' surfaces it lies
	// on, and every surface it was laid on, the original's SurfList
	Array<BspNode*> Nodes;
	Array<int> FiledSurfaces;
	Array<int> Surfaces;

private:
	void AttachTo(int surf, LevelDecal& decal, const vec3* world, const vec3& base, const vec3& normal);
};
