#pragma once

#include "CollisionSystem.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Actors/UActor.h"

class CollisionTester
{
public:
	CollisionTester(CollisionSystem* collision) : Collision(collision) {}

	static int NextCheckCounter() { return CheckCounter++; }

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
	// excluded), until it returns true; then so does this. A box of 100 cells
	// or more on an axis is asked of every actor in the hash instead, there
	// being fewer of them than its cells: a test that long (a rifle's 48000
	// units) still meets every actor on its way.
	template<typename Visit>
	bool VisitActorsInBox(const ivec3& start, const ivec3& end, Visit&& visit)
	{
		int checkCounter = NextCheckCounter();
		if (end.x - start.x >= 100 || end.y - start.y >= 100 || end.z - start.z >= 100)
		{
			for (UActor* actor : GetLevel()->Actors)
			{
				if (actor && actor->Collision.Inserted && actor->Collision.CheckCounter != checkCounter)
				{
					actor->Collision.CheckCounter = checkCounter;
					if (visit(actor))
						return true;
				}
			}
			return false;
		}
		for (int z = start.z; z < end.z; z++)
		{
			for (int y = start.y; y < end.y; y++)
			{
				for (int x = start.x; x < end.x; x++)
				{
					for (UActor* actor : GetActors(x, y, z))
					{
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

private:
	CollisionSystem* Collision = nullptr;
	inline static int CheckCounter = 0;
};
