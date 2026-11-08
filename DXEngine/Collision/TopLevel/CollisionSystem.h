#pragma once

#include "Math/vec.h"
#include "Math/rotator.h"
#include "BrushTracker.h"
#include "Math/floating.h"
#include <functional>
#include <memory>

class ULevel;
class UActor;
class UMover;
struct Poly;
class CollisionHitList;
class CollisionHit;

// What reaches each 256-unit cell, in the order it was added. An
// open-addressed table keyed by bucket id (linear probing, a power-of-two
// size, at most half full): finding a cell is a multiply and, for an empty
// one, usually one cache line of ids, where a std::unordered_map of
// std::lists took a division and a chain of pointers, and a list node
// allocated for every cell of every move. A cell keeps its slot once used,
// so the table only ever grows.
template<typename T>
class CellTable
{
public:
	const Array<T>& Find(uint32_t id) const
	{
		size_t slot = FindSlot(id);
		return slot != NoSlot ? Items[slot] : None;
	}

	Array<T>& Get(uint32_t id) // made if new
	{
		if ((Count + 1) * 2 > Ids.size())
			Grow();
		size_t mask = Ids.size() - 1;
		size_t slot = Hash(id) & mask;
		while (Ids[slot] != id && Ids[slot] != NoCell)
			slot = (slot + 1) & mask;
		if (Ids[slot] == NoCell)
		{
			Ids[slot] = id;
			Count++;
		}
		return Items[slot];
	}

	// Every entry, keeping the others in order, as std::list::remove did
	void Remove(uint32_t id, const T& item)
	{
		size_t slot = FindSlot(id);
		if (slot != NoSlot)
		{
			Array<T>& cell = Items[slot];
			cell.erase(std::remove(cell.begin(), cell.end(), item), cell.end());
		}
	}

	void Clear()
	{
		Ids.clear();
		Items.clear();
		Count = 0;
	}

private:
	size_t FindSlot(uint32_t id) const
	{
		if (Ids.empty())
			return NoSlot;
		size_t mask = Ids.size() - 1;
		for (size_t slot = Hash(id) & mask; ; slot = (slot + 1) & mask)
		{
			if (Ids[slot] == id)
				return slot;
			if (Ids[slot] == NoCell)
				return NoSlot;
		}
	}

	void Grow()
	{
		size_t capacity = std::max<size_t>(4096, Ids.size() * 2);
		Array<uint32_t> ids(capacity, NoCell);
		Array<Array<T>> items(capacity);
		size_t mask = capacity - 1;
		for (size_t i = 0; i < Ids.size(); i++)
		{
			if (Ids[i] == NoCell)
				continue;
			size_t slot = Hash(Ids[i]) & mask;
			while (ids[slot] != NoCell)
				slot = (slot + 1) & mask;
			ids[slot] = Ids[i];
			items[slot] = std::move(Items[i]);
		}
		Ids = std::move(ids);
		Items = std::move(items);
	}

	static uint32_t Hash(uint32_t id) { uint32_t h = id * 0x9E3779B1u; return h ^ (h >> 16); }
	static constexpr uint32_t NoCell = 0xffffffff; // never a bucket id: those have 30 bits
	static constexpr size_t NoSlot = ~(size_t)0;

	Array<uint32_t> Ids; // NoCell where a slot is free
	Array<Array<T>> Items;
	size_t Count = 0;
	inline static const Array<T> None;
};

class GCMarker;

struct TraceFlags
{
	bool pawns = false;
	bool movers = false;
	bool others = false;
	bool world = false;
	bool zoneChanges = false;
	bool onlyProjectiles = false;

	bool traceActors() const { return pawns || movers || others || zoneChanges || onlyProjectiles; }
	bool traceWorld() const { return world; }
};

class CollisionSystem
{
public:
	// The filed polygons' textures, for the collector (ULevel::Mark)
	void Mark(GCMarker& marker);

	bool IsOverlapping(UActor* actor1, UActor* actor2);
	CollisionHitList OverlapTest(UActor* actor);
	CollisionHitList OverlapTest(const vec3& location, float height, float radius, bool testActors, bool testWorld, bool visibilityOnly);

	bool TraceAnyHit(vec3 from, vec3 to, UActor* tracingActor, bool traceActors, bool traceWorld, bool visibilityOnly);
	CollisionHitList Trace(const vec3& from, const vec3& to, float height, float radius, bool traceActors, bool traceWorld, bool visibilityOnly);
	CollisionHitList TraceDecal(const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, bool visibilityOnly);
	CollisionHit TraceFirstHit(const vec3& from, const vec3& to, UActor* tracingActor, const vec3& extents, const TraceFlags& flags);

	// Whether sight along a segment is blocked: by world and mover surfaces
	// that block visibility, or by an actor on it that blocksSight says blocks
	// (Deus Ex's AI line of sight).
	bool SightBlocked(const vec3& from, const vec3& to, const std::function<bool(UActor* actor)>& blocksSight);

	// As the original's FCollisionHash::ActorRadiusCheck: every colliding
	// actor, movers too, whose location lies within radius of origin.
	Array<UActor*> ActorRadiusCheck(const vec3& origin, float radius);

	Array<UActor*> CollidingActors(const vec3& origin, float radius);
	Array<UActor*> CollidingActors(const vec3& origin, float height, float radius);
	Array<UActor*> EncroachingActors(UActor* actor, const vec3& location, const Rotator& rotation);
	Array<UActor*> EncroachedActors(const vec3& location, float height, float radius);

	void SetLevel(ULevel* level);
	void AddToCollision(UActor* actor);
	void RemoveFromCollision(UActor* actor);

	// Deus Ex's movers in the BSP (BrushTracker): filed afresh at a map's
	// load, but on a dedicated server, and after a save (all, where they
	// are), each drawn frame again where a shown one has moved or turned
	// since, in the actor list's order; a destroyed one stays filed, the
	// tracker not told, as the original's.
	void FileMovers();
	void RefileMovers(const std::function<bool(UMover*)>& shown);
	// The BSP a line walks: the level's with the movers filed into it, or
	// the level's own with no tracker (a dedicated server); and its root's
	// side
	const Array<BspNode>& LineNodes() const;
	bool LineRootOutside() const;
	// A node's surface in that BSP: the level's own, or a filed polygon's
	const BspSurface& LineSurface(int index) const;
	// For each of a mover's polygons, the fragments the tracker last filed
	// (BrushTracker::Fragments): each one's node in that BSP, its plane and
	// zones, and its corners in its brush's own space; none with no tracker
	// or for a mover it does not file
	const Array<Array<BrushTracker::FiledFragment>>* FiledFragments(const UMover* mover) const { return Tracker.Active() ? Tracker.Fragments(mover) : nullptr; }
	const BspNode& FiledNode(int node) const { return Tracker.Nodes()[node]; }
	const vec3* FiledCorners(int node) const { return Tracker.Corners(node); }
	// The decals on a filed polygon's surface; null for any other
	Array<LevelDecal>* FiledDecals(int surface) { return Tracker.Active() ? Tracker.Decals(surface) : nullptr; }
	// The flags of the level surface the original files a mover's polygon
	// under: a hit on it gives them, and the polygon draws with them
	static uint32_t FiledFlags(UMover* mover, const Poly& poly);

	// The original's hash cell of a coordinate (FCollisionHash::GetHashIndices,
	// Engine.dll 0x10357590): the coordinate plus 32768 over 256, a float,
	// made whole by the x87's fistp, to the nearest and a half to even, held
	// to 0 to 16383 -- so a cell's middle is a multiple of 256 and its bounds
	// lie 128 off one (HashOrderConsole: marks 20 units either side of x
	// -4736, a cell's bound, in two cells); one that is no number or past an
	// int's range the x87's integer indefinite, held to 0. v as the x87 holds
	// the coordinate before the 32768 is added.
	static int CellCoord(double v)
	{
		float c = (float)X87::Round(X87::Round(v + 32768.0) * 0.00390625);
		if (!(c >= -2147483648.0f && c < 2147483648.0f))
			return 0;
		double whole = std::floor((double)c);
		double part = (double)c - whole;
		int64_t cell = (int64_t)whole;
		if (part > 0.5 || (part == 0.5 && (cell & 1)))
			cell++;
		return (int)std::clamp<int64_t>(cell, 0, 0x3fff);
	}

	// A box's cells, from its first to past its last, as the hash files an
	// actor and an encroachment check takes the actor at its new place
	// (FCollisionHash::GetActorExtent, 0x10357e30): each bound a float
	static void BoxCells(const vec3& lo, const vec3& hi, ivec3& start, ivec3& end)
	{
		start = ivec3(CellCoord(lo.x), CellCoord(lo.y), CellCoord(lo.z));
		end = ivec3(CellCoord(hi.x) + 1, CellCoord(hi.y) + 1, CellCoord(hi.z) + 1);
	}

	// The cells a check reaches, lo less reach to hi plus it, as the radius
	// and line checks take them (ActorRadiusCheck 0x103589e0, ActorLineCheck
	// 0x10359160): Y's and Z's bounds stored as floats, X's as the x87 holds
	// it
	static void QueryCells(const vec3& lo, const vec3& hi, const vec3& reach, ivec3& start, ivec3& end)
	{
		auto R = X87::Round;
		start = ivec3(
			CellCoord(R((double)lo.x - reach.x)),
			CellCoord((float)R((double)lo.y - reach.y)),
			CellCoord((float)R((double)lo.z - reach.z)));
		end = ivec3(
			CellCoord(R((double)reach.x + hi.x)) + 1,
			CellCoord((float)R((double)hi.y + reach.y)) + 1,
			CellCoord((float)R((double)hi.z + reach.z)) + 1);
	}

	// An actor's box in the hash were it to stand at location, turned to
	// rotation, as its primitive gives it (GetCollisionBoundingBox): a
	// brush's, its model's box's corners through its ToWorld coordinates
	// (UModel's, 0x103f0e70); any other's, its cylinder a unit wider and
	// taller each way (UPrimitive's, 0x103d8460)
	static void HashBox(UActor* actor, const vec3& location, const Rotator& rotation, vec3& lo, vec3& hi);
	static void CylinderBox(const vec3& location, float radius, float height, vec3& lo, vec3& hi);

	// A line's box, its ends' least and most on each axis, as FBox takes them
	static vec3 Least(const vec3& a, const vec3& b) { return vec3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)); }
	static vec3 Most(const vec3& a, const vec3& b) { return vec3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)); }

private:
	static ivec3 GetStartExtents(const vec3& location, const vec3& extents)
	{
		ivec3 start, end;
		QueryCells(location, location, extents, start, end);
		return start;
	}

	static ivec3 GetEndExtents(const vec3& location, const vec3& extents)
	{
		ivec3 start, end;
		QueryCells(location, location, extents, start, end);
		return end;
	}

	static ivec3 GetSweepStartExtents(const vec3& from, const vec3& to, const vec3& extents)
	{
		ivec3 start, end;
		QueryCells(Least(from, to), Most(from, to), extents, start, end);
		return start;
	}

	static ivec3 GetSweepEndExtents(const vec3& from, const vec3& to, const vec3& extents)
	{
		ivec3 start, end;
		QueryCells(Least(from, to), Most(from, to), extents, start, end);
		return end;
	}

	static ivec3 GetRayStartExtents(const vec3& from, const vec3& to) { return GetSweepStartExtents(from, to, vec3(0.0f)); }
	static ivec3 GetRayEndExtents(const vec3& from, const vec3& to) { return GetSweepEndExtents(from, to, vec3(0.0f)); }

	static uint32_t GetBucketId(int x, int y, int z)
	{
		return ((x & 0x3ff) << 20) | ((y & 0x3ff) << 10) | (z & 0x3ff);
	}

	// The actors whose collision boxes reach each cell, the newest filed last
	const Array<UActor*>& FindCell(uint32_t id) const { return ActorCells.Find(id); }

	ULevel* Level = nullptr;
	CellTable<UActor*> ActorCells;
	BrushTracker Tracker;
	uint64_t FiledCount = 0; // the filings so far, each actor's its filing's

	friend class CollisionTester;
};
