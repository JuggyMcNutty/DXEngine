#pragma once

#include "Packages/Engine/Resources/Level/UModel.h"

class ULevel;
class UMover;
class GCMarker;
struct Poly;

// Deus Ex's movers in the BSP: the original's moving-brush tracker
// (Engine.dll's, dx-reverse-info engine-dll.md, traces: the movers in the
// BSP). Each mover's polygons are filed into a copy of the level's BSP where
// the mover is: split down the tree, each fragment in empty space a node of
// its own, one in solid dropped, one in a node's plane hung at the end of
// that plane's chain. A line walk down the copy meets them as the level's
// own planes, the space behind them solid within their leaves; a box meets
// none, its walk taking the level's own BSP. The renderer, the zones and the
// decals keep the level's BSP.
class BrushTracker
{
public:
	// A tracker made (UGameEngine::LoadMap's with a client, and Deus Ex's
	// after a save): the level's nodes copied, a surface for each polygon of
	// each moving brush, and every one filed, in the original's order
	void Start(ULevel* level);
	void Clear();

	// The mover filed again where it is, when it has moved or turned since
	// its filing, as the original's renderer hands it each frame drawn: its
	// nodes taken out, and the movers filed under them filed again too
	void Update(UMover* mover);

	// The original's DestroyActor leaves the tracker alone: the mover's
	// polygons stay filed. DXEngine keeps the mover and its textures for the
	// filed surfaces to name, where the original's are freed under them.
	void Mark(GCMarker& marker);

	// The BSP a line walks: the level's own, the movers filed into it; empty
	// with no tracker (a dedicated server), when the level's is walked
	const Array<BspNode>& Nodes() const { return LineNodes; }
	bool Active() const { return Level != nullptr; }

	// A node's surface, the level's own or a filed polygon's
	const BspSurface& Surface(int index) const;

private:
	struct Fragment
	{
		Array<vec3> Vertices;
		vec3 Base, Normal;
		uint32_t PolyFlags = 0;
	};

	struct Tracked
	{
		UMover* Mover = nullptr;
		int FirstSurface = 0; // the surface of its brush's first polygon
		Array<int> Nodes; // its fragments' nodes, in filing order
		bool Queued = false;
	};

	struct Slot
	{
		Tracked* Owner = nullptr;
		int Parent = -1;
	};

	Tracked* Find(UMover* mover);
	void Queue(Tracked* tracked, Array<Tracked*>& queue);
	void Refile(Array<Tracked*>& queue);
	void TakeOut(int node, Array<Tracked*>& queue);
	void Free(Tracked* tracked);
	void File(Tracked* tracked);
	void PrecomputeSphereFilter(const vec4& sphere);
	void PrecomputeSphereFilterBack(int node, const vec4& sphere);
	void FilterPoly(int surface, int node, int coplanarParent, Fragment& poly, bool outside);
	void AddPolyFragment(int surface, int parent, int coplanarParent, int side, const Fragment& poly);
	int AllocNode();
	int AllocVerts(int count);

	ULevel* Level = nullptr;
	Array<BspNode> LineNodes;
	int FirstNode = 0, NextNode = 0;
	Array<Slot> Slots; // a slot for each node past the level's own
	int VertPoolSize = 0, NextVert = 0;
	Array<Tracked*> VertOwners; // a vertex slot's fragment's mover, none where free
	Array<int> SlotVerts; // each node slot's first vertex slot, -1 for none
	Array<BspSurface> FiledSurfaces;
	int FirstFiledSurface = 0;
	Array<std::unique_ptr<Tracked>> Movers;
	Tracked* Filing = nullptr;
};
