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
// none, its walk taking the level's own BSP. The renderer takes the zones a
// mover's polygons are drawn in from the fragments its last filing left,
// and draws the fragments of one across zones (VisibleBrush); its walk
// takes the copy, the fragments filling the spans as the level's own
// surfaces (VisibleFrame::ProcessNode), and a decal's lines walk it,
// meeting a mover's polygon to lay a decal on (UDecal::AttachDecal); the
// zones of the rest keep the level's BSP.
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

	// A fragment of a mover's polygon as its last filing left it: its node
	// in the BSP a line walks, which holds its plane and its zones -- its
	// parent's on its side, or, in the plane of the node it hangs after,
	// that node's, turned as it faces -- and whether it hangs in the plane
	// chain of a node of the level's own, after that node, whose surface the
	// original's walk draws first
	struct FiledFragment
	{
		int Node = -1;
		bool InLevelPlane = false;
	};

	// For each of the mover's polygons, in its brush's order, the fragments
	// its last filing left in empty space: none for one dropped whole, in
	// solid or for want of room. Null for a mover the tracker does not file.
	const Array<Array<FiledFragment>>* Fragments(const UMover* mover) const;

	// A filed node's corners, as many as its NumVertices, in its brush's
	// own space: the polygon's own where no plane split it
	const vec3* Corners(int node) const;

	// The decals laid on a filed polygon's surface (UDecal::AttachDecal), as
	// the original's surfaces hold theirs, the tracker's start making each
	// surface anew without any; null for a surface it does not make
	Array<LevelDecal>* Decals(int surface);

private:
	struct Fragment
	{
		Array<vec3> Vertices; // in the world, as the filing tests them
		Array<vec3> Corners; // the same corners in the brush's own space
		vec3 Base, Normal;
		uint32_t PolyFlags = 0;
	};

	struct Tracked
	{
		UMover* Mover = nullptr;
		int FirstSurface = 0; // the surface of its brush's first polygon
		Array<int> Nodes; // its fragments' nodes, in filing order
		Array<Array<FiledFragment>> Polys; // by polygon (Fragments above)
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
	Array<vec3> VertCorners; // a vertex slot's corner, in its brush's own space
	Array<int> SlotVerts; // each node slot's first vertex slot, -1 for none
	Array<BspSurface> FiledSurfaces;
	Array<Array<LevelDecal>> FiledDecals; // by filed surface
	int FirstFiledSurface = 0;
	Array<std::unique_ptr<Tracked>> Movers;
	Tracked* Filing = nullptr;
};
