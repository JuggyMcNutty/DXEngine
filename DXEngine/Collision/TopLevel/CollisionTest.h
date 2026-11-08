#pragma once

#include "CollisionSystem.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Actors/UActor.h"
#include <algorithm>

class CollisionTester
{
public:
	CollisionTester(CollisionSystem* collision) : Collision(collision) {}

	// A query's stamp, the actors it has met marked with it. Before the
	// counter would overflow (some hours at a hundred thousand queries a
	// second) every actor's stamp goes back to never visited and it starts
	// again: a wrapped counter would be undefined, and at -1 would skip a
	// never-visited actor.
	int NextCheckCounter()
	{
		if (CheckCounter == INT32_MAX)
		{
			for (UActor* actor : GetLevel()->Actors)
			{
				if (actor)
					actor->Collision.CheckCounter = -1;
			}
			CheckCounter = 0;
		}
		return CheckCounter++;
	}

	static ivec3 GetStartExtents(const vec3& location, const vec3& extents) { return CollisionSystem::GetStartExtents(location, extents); }
	static ivec3 GetRayStartExtents(const vec3& from, const vec3& to) { return CollisionSystem::GetRayStartExtents(from, to); }
	static ivec3 GetSweepStartExtents(const vec3& from, const vec3& to, const vec3& extents) { return CollisionSystem::GetSweepStartExtents(from, to, extents); }

	static ivec3 GetEndExtents(const vec3& location, const vec3& extents) { return CollisionSystem::GetEndExtents(location, extents); }
	static ivec3 GetRayEndExtents(const vec3& from, const vec3& to) { return CollisionSystem::GetRayEndExtents(from, to); }
	static ivec3 GetSweepEndExtents(const vec3& from, const vec3& to, const vec3& extents) { return CollisionSystem::GetSweepEndExtents(from, to, extents); }

	const Array<UActor*>& GetActors(int x, int y, int z) const
	{
		return Collision->FindCell(CollisionSystem::GetBucketId(x, y, z));
	}

	ULevel* GetLevel() const { return Collision->Level; }

	// Calls visit once for each actor in the cells from start to end (end
	// excluded), until it returns true; then so does this. In the original's
	// order (FCollisionHash's checks, Engine.dll 0x103589e0, 0x10358d50,
	// 0x10359160): the cells X outermost, then Y, then Z, each cell's actors
	// the newest filed first, as its AddActor puts each at its bucket's head
	// (0x103580b0), each actor at the first cell it is found in. A box of 100
	// cells or more on an axis takes every actor in the hash instead, there
	// being fewer of them than its cells, in the same order (SortAsFound): a
	// test that long (a rifle's 48000 units) still meets every actor on its
	// way.
	template<typename Visit>
	bool VisitActorsInBox(const ivec3& start, const ivec3& end, Visit&& visit)
	{
		int checkCounter = NextCheckCounter();
		if (end.x - start.x >= 100 || end.y - start.y >= 100 || end.z - start.z >= 100)
		{
			Array<UActor*> found;
			for (UActor* actor : GetLevel()->Actors)
			{
				if (actor && actor->Collision.Inserted && actor->Collision.CheckCounter != checkCounter && Reaches(actor, start, end))
				{
					actor->Collision.CheckCounter = checkCounter;
					found.push_back(actor);
				}
			}
			SortAsFound(found, start);
			for (UActor* actor : found)
			{
				if (visit(actor))
					return true;
			}
			return false;
		}
		for (int x = start.x; x < end.x; x++)
		{
			for (int y = start.y; y < end.y; y++)
			{
				for (int z = start.z; z < end.z; z++)
				{
					const Array<UActor*>& cell = GetActors(x, y, z);
					for (size_t i = cell.size(); i-- > 0;)
					{
						UActor* actor = cell[i];
						if (actor->Collision.CheckCounter != checkCounter)
						{
							actor->Collision.CheckCounter = checkCounter;
							if (visit(actor))
								return true;
						}
					}
				}
			}
		}
		return false;
	}

	// Whether an actor's cells reach into the box of cells from start to end
	static bool Reaches(UActor* actor, const ivec3& start, const ivec3& end)
	{
		const ivec3& a = actor->Collision.CellStart;
		const ivec3& b = actor->Collision.CellEnd;
		return a.x < end.x && b.x > start.x && a.y < end.y && b.y > start.y && a.z < end.z && b.z > start.z;
	}

	// Actors in the order the original's walk of the cells from start finds
	// them (VisitActorsInBox): by the first of their cells it comes to -- X,
	// then Y, then Z --, the newest filed first in a cell
	static void SortAsFound(Array<UActor*>& actors, const ivec3& start)
	{
		auto first = [&](UActor* actor) {
			const ivec3& a = actor->Collision.CellStart;
			return ivec3(std::max(a.x, start.x), std::max(a.y, start.y), std::max(a.z, start.z));
		};
		std::stable_sort(actors.begin(), actors.end(), [&](UActor* l, UActor* r) {
			ivec3 a = first(l), b = first(r);
			if (a.x != b.x)
				return a.x < b.x;
			if (a.y != b.y)
				return a.y < b.y;
			if (a.z != b.z)
				return a.z < b.z;
			return l->Collision.Filed > r->Collision.Filed;
		});
	}

private:
	CollisionSystem* Collision = nullptr;
	inline static int CheckCounter = 0;
};
