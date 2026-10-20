#pragma once

#include "VisibleNode.h"
#include "BspClipper.h"

class BspNode;
class USkyZoneInfo;
class UWarpZoneInfo;
class UMover;
struct Poly;

class VisiblePortal
{
public:
	Array<VisibleNode> Nodes;
	Array<PortalSpan> Spans;
	USkyZoneInfo* SkyZone = nullptr;
	UWarpZoneInfo* WarpZone = nullptr;

	// A mover's mirror: its polygon, and its plane in the world
	UMover* Mover = nullptr;
	const Poly* MoverPoly = nullptr;
	vec3 MirrorPoint = vec3(0.0f);
	vec3 MirrorNormal = vec3(0.0f);
};
