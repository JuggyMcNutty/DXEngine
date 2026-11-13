
#include "Precomp.h"
#include "UDecal.h"
#include "Collision/TopLevel/CollisionSystem.h"
#include "Collision/BottomLevel/TraceRayModel.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/Level/UPolys.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "Utils/Random.h"
#include "Utils/Logger.h"
#include "Math/coords.h"
#include <algorithm>

namespace
{
	// Normalized where its square is at least 1e-8, else as it is, as the
	// original's vectors are (FVector::Normalize)
	vec3 Normal8(const vec3& v)
	{
		float sq = dot(v, v);
		if (sq < 1e-8f)
			return v;
		return v * (float)(1.0 / std::sqrt((double)sq));
	}

	// The polygon cut by a plane as the original's clips cut one: a point put
	// in where an edge crosses from one side to the other, then each point
	// below 0 taken out
	template<typename Dist>
	void ClipPolygon(Array<vec3>& points, Dist dist)
	{
		Array<float> d;
		d.reserve(points.size() + 8);
		for (const vec3& p : points)
			d.push_back(dist(p));
		for (size_t i = 0; i < points.size(); i++)
		{
			size_t next = (i + 1) % points.size();
			if ((d[i] > 0.0f && d[next] < 0.0f) || (d[i] < 0.0f && d[next] > 0.0f))
			{
				vec3 p = points[i] + (points[next] - points[i]) * (d[i] / (d[i] - d[next]));
				points.insert(points.begin() + i + 1, p);
				d.insert(d.begin() + i + 1, 0.0f);
				i++;
			}
		}
		size_t kept = 0;
		for (size_t i = 0; i < points.size(); i++)
		{
			if (d[i] >= 0.0f)
				points[kept++] = points[i];
		}
		points.resize(kept);
	}

	// A node's corners in the world: the level's own, or a filed polygon's
	// where the tracker filed it
	void NodeCorners(ULevel* level, int index, Array<vec3>& corners)
	{
		UModel* model = level->Model;
		corners.clear();
		if ((size_t)index < model->Nodes.size())
		{
			const BspNode& node = model->Nodes[index];
			const BspVert* v = &model->Vertices[node.VertPool];
			for (int i = 0; i < node.NumVertices; i++)
				corners.push_back(model->Points[v[i].Vertex]);
			return;
		}
		CollisionSystem& collision = level->Collision;
		const BspNode& node = collision.FiledNode(index);
		UMover* mover = UObject::TryCast<UMover>(collision.LineSurface(node.Surf).BrushActor);
		if (!mover)
			return;
		mat4 toWorld = mover->BrushToWorld();
		const vec3* local = collision.FiledCorners(index);
		for (int i = 0; i < node.NumVertices; i++)
			corners.push_back((toWorld * vec4(local[i], 1.0f)).xyz());
	}

	// The original's line check of the level's model with no extent, passing
	// what is shoot-through and not vis-blocking (the node flags 6,
	// Engine.dll 0x103e75dd; UModel::LineCheck's walk 0x103f3560): the BSP
	// with the movers the tracker filed into it, the line stopping where it
	// enters solid. The node whose plane it crossed there, the first of that
	// plane's chain; -1 where it stops nowhere.
	int LineHit(ULevel* level, const vec3& start, const vec3& end)
	{
		const Array<BspNode>& nodes = level->Collision.LineNodes();
		vec3 place;
		BspNode* crossed = nullptr;
		if (!TraceRayModel::WalkSolid(nodes, level->Collision.LineRootOutside(), start, end, NF_ShootThrough | NF_NotVisBlocking, place, crossed) || !crossed)
			return -1;
		return (int)(crossed - nodes.data());
	}

	// Where the line meets a surface's plane, as the original places a decal
	// (0x103e764c-0x103e7724)
	vec3 PlaneHit(const vec3& start, const vec3& end, const vec3& base, const vec3& normal)
	{
		vec3 d = end - start;
		float t = dot(base - start, normal) / dot(d, normal);
		return start + d * t;
	}

	// The surface of the first node down a plane's chain whose polygon holds
	// the point, as the original's search finds it (0x103e9090): each edge
	// by its surface's normal, the point inside while the side it lies on
	// never turns from one edge to the next; -1 where none holds it
	int SurfaceAt(ULevel* level, int index, const vec3& p)
	{
		const Array<BspNode>& nodes = level->Collision.LineNodes();
		Array<vec3> corners;
		while (index >= 0)
		{
			const BspNode& node = nodes[index];
			vec3 base, normal;
			if (node.NumVertices > 0 && UDecal::SurfaceFrame(level, node.Surf, base, normal))
			{
				NodeCorners(level, index, corners);
				bool inside = !corners.empty();
				float last = 0.0f;
				vec3 prev = corners.empty() ? vec3(0.0f) : corners.back();
				for (const vec3& cur : corners)
				{
					vec3 side = cross(normal, cur - prev);
					float d = dot(p, side) - dot(cur, side);
					if ((d < 0.0f && last > 0.0f) || (d > 0.0f && last < 0.0f))
					{
						inside = false;
						break;
					}
					prev = cur;
					last = d;
				}
				if (inside)
					return node.Surf;
			}
			index = node.Plane;
		}
		return -1;
	}

	// Whether a decal's corners, clipped in its surface's plane by the edges
	// of a node's polygon, leave anything: a node the decal lies on
	// (0x103e9340)
	bool OnNode(UModel* model, const BspNode& node, const vec3* world, const vec3& normal)
	{
		if (node.NumVertices == 0)
			return false;
		Array<vec3> points(world, world + 4);
		const BspVert* v = &model->Vertices[node.VertPool];
		vec3 prev = model->Points[v[node.NumVertices - 1].Vertex];
		for (int i = 0; i < node.NumVertices && !points.empty(); i++)
		{
			const vec3& cur = model->Points[v[i].Vertex];
			vec3 side = cross(normal, cur - prev);
			float w = dot(cur, side);
			ClipPolygon(points, [&](const vec3& p) { return dot(p, side) - w; });
			prev = cur;
		}
		return !points.empty();
	}

	// A side's share, each component worked from the fraction at the x87's
	// precision and stored, as the original's grid works it (0x103e7f68,
	// 0x103e7fa9)
	vec3 Share(const vec3& side, double fraction)
	{
		return vec3((float)(side.x * fraction), (float)(side.y * fraction), (float)(side.z * fraction));
	}

	// Before the surface's first decal of the same texture, else last, as the
	// original keeps a surface's decals, each texture's together
	// (0x103e7922-0x103e798a)
	void InsertDecal(Array<LevelDecal>& list, const LevelDecal& decal)
	{
		UTexture* texture = decal.Decal->Texture();
		for (auto it = list.begin(); it != list.end(); ++it)
		{
			if (it->Decal->Texture() == texture)
			{
				list.insert(it, decal);
				return;
			}
		}
		list.push_back(decal);
	}
}

// A surface's base and normal in the world: the level's own from its model;
// a mover's polygon's where the brush tracker last filed it, as each filing
// moves its surface's (Engine.dll 0x1037a400): the polygon's base where the
// mover is drawn, the normal its fragments' plane takes. False for a
// polygon its last filing dropped whole, in no node of the BSP.
bool UDecal::SurfaceFrame(ULevel* level, int surf, vec3& base, vec3& normal, UMover** mover)
{
	UModel* model = level->Model;
	if (mover)
		*mover = nullptr;
	if (surf < 0)
		return false;
	if ((size_t)surf < model->Surfaces.size())
	{
		const BspSurface& surface = model->Surfaces[surf];
		if (surface.pBase < 0 || surface.vNormal < 0 || (size_t)surface.pBase >= model->Points.size() || (size_t)surface.vNormal >= model->Vectors.size())
			return false;
		base = model->Points[surface.pBase];
		normal = model->Vectors[surface.vNormal];
		return true;
	}
	CollisionSystem& collision = level->Collision;
	if (!collision.FiledDecals(surf))
		return false;
	const BspSurface& surface = collision.LineSurface(surf);
	UMover* owner = UObject::TryCast<UMover>(surface.BrushActor);
	if (!owner || !owner->Brush() || !owner->Brush()->Polys || surface.BrushPoly < 0 || (size_t)surface.BrushPoly >= owner->Brush()->Polys->Polys.size())
		return false;
	const Array<Array<BrushTracker::FiledFragment>>* fragments = collision.FiledFragments(owner);
	if (!fragments || (size_t)surface.BrushPoly >= fragments->size() || (*fragments)[surface.BrushPoly].empty())
		return false;
	const BspNode& node = collision.FiledNode((*fragments)[surface.BrushPoly].front().Node);
	const Poly& poly = owner->Brush()->Polys->Polys[surface.BrushPoly];
	base = (owner->BrushToWorld() * vec4(poly.Base, 1.0f)).xyz();
	normal = vec3(node.PlaneX, node.PlaneY, node.PlaneZ);
	if (mover)
		*mover = owner;
	return true;
}

// The original's AttachDecal (Engine.dll ADecal::execAttachDecal,
// 0x103e7250; dx-reverse-info engine-dll.md, the natives): a line from the
// decal TraceDistance units along its back, passing what is shoot-through
// and not vis-blocking, meets the level's model with the movers the tracker
// filed into it; the surface is the one of the first node in the crossed
// plane's chain that holds the point, refused only when the decal faces
// along it, the point is off its plane or it is auto-panned. The decal is a
// square USize times DrawScale on a side about the point, turned by DecalDir
// in the surface's plane, a random direction drawn from within the unit
// sphere where DecalDir is zero. It lies on every node of the surface it
// covers, wherever they are in the BSP, and a grid of MultiDecalLevel
// squared lines, 4 for every stock decal, lays it on the other surfaces they
// meet that face within 45 degrees or so of the first. On a mover's polygon
// it keeps the mover's rotation and is based on its Owner.
UObject* UDecal::AttachDecal(float traceDistance, vec3 decalDir)
{
	if (!Texture())
	{
		LogMessage("AttachDecal: No Texture");
		return nullptr;
	}

	// The level held to 4 at most, kept so (0x103e733b)
	if (MultiDecalLevel() > 4)
		MultiDecalLevel() = 4;

	ULevel* level = XLevel();
	if (!level || !level->Model)
		return nullptr;
	CollisionSystem& collision = level->Collision;
	if (collision.LineNodes().empty())
		return nullptr;

	vec3 xaxis = Coords::Rotation(Rotation()).XAxis;
	vec3 trace = Normal8(-xaxis) * traceDistance;

	// No direction: one drawn from within the unit sphere, normalized, as
	// the original draws one (0x103e74fd-0x103e7580)
	bool random = false;
	if (decalDir.x == 0.0f && decalDir.y == 0.0f && decalDir.z == 0.0f)
	{
		vec3 v;
		float sq;
		do
		{
			v.x = FRand() * 2.0f - 1.0f;
			v.y = FRand() * 2.0f - 1.0f;
			v.z = FRand() * 2.0f - 1.0f;
			sq = dot(v, v);
		} while (sq > 1.0f || sq == 0.0f);
		decalDir = v * (1.0f / std::sqrt(sq));
		random = true;
	}

	vec3 start = Location();
	vec3 end = start + trace;
	int item = LineHit(level, start, end);
	if (item < 0)
		return nullptr;

	vec3 base, normal;
	if (!SurfaceFrame(level, collision.LineNodes()[item].Surf, base, normal))
		return nullptr;
	int surf = SurfaceAt(level, item, PlaneHit(start, end, base, normal));
	UMover* mover = nullptr;
	if (surf < 0 || !SurfaceFrame(level, surf, base, normal, &mover))
		return nullptr;
	vec3 hit = PlaneHit(start, end, base, normal);

	// Refused facing along the surface, off its plane by more than a
	// thousandth, and auto-panned (0x103e787d-0x103e791c); the line meets
	// the plane where it crosses it, so the second never refuses one
	const BspSurface& surface = collision.LineSurface(surf);
	if (dot(xaxis, normal) == 0.0f)
		return nullptr;
	if (!(std::abs(dot(base - hit, normal)) <= 0.001f) || (surface.PolyFlags & (PF_AutoUPan | PF_AutoVPan)))
		return nullptr;

	LevelDecal decal;
	decal.Decal = this;
	if (mover)
	{
		decal.MoverRotation = mover->Rotation();
		SetBase(Owner(), true);
	}

	// Its corners: half its diagonal from the point, a square USize times
	// DrawScale on a side; with a direction its sides along the direction
	// and across it, without one its corners
	float usize = (float)Texture()->USize();
	float scale = DrawScale();
	float radius = (float)std::sqrt((double)scale * scale * usize * usize * 0.5);
	vec3 v0, v1;
	bool sides = false;
	if (!random)
	{
		vec3 along = decalDir - normal * dot(decalDir, normal);
		if (std::abs(along.x) >= 0.0001f || std::abs(along.y) >= 0.0001f || std::abs(along.z) >= 0.0001f)
		{
			vec3 across = Normal8(cross(along, normal));
			along = Normal8(along);
			v0 = across + along;
			v1 = along - across;
			sides = true;
		}
		else
		{
			// One along the normal: the direction to the surface's base
			decalDir = base - hit;
		}
	}
	if (!sides)
	{
		v0 = decalDir - normal * dot(decalDir, normal);
		v1 = cross(v0, normal);
	}
	v0 = Normal8(v0) * radius;
	v1 = Normal8(v1) * radius;
	vec3 world[4] = { hit + v0, hit + v1, hit - v0, hit - v1 };

	AttachTo(surf, decal, world, base, normal);

	// The grid: from each of MultiDecalLevel squared points of the square
	// (its first corner's sides less the first row and column), a line from
	// 16 units before it to 16 past it along the trace, another surface it
	// meets taking the decal, its corners moved onto that surface's plane
	// (0x103e7ea2-0x103e8910)
	int levels = MultiDecalLevel();
	vec3 side1 = world[1] - world[0];
	vec3 side3 = world[3] - world[0];
	vec3 offset = xaxis * -16.0f;
	for (int i = 0; i < levels; i++)
	{
		for (int j = 0; j < levels; j++)
		{
			vec3 p = Share(side3, (j + 1.0) / levels) + (world[0] + Share(side1, (i + 1.0) / levels));
			vec3 lineStart = p - offset;
			vec3 lineEnd = p + offset;
			int item2 = LineHit(level, lineStart, lineEnd);
			if (item2 < 0)
				continue;
			vec3 base2, normal2;
			if (!SurfaceFrame(level, collision.LineNodes()[item2].Surf, base2, normal2))
				continue;
			int surf2 = SurfaceAt(level, item2, PlaneHit(lineStart, lineEnd, base2, normal2));
			if (surf2 < 0 || std::find(Surfaces.begin(), Surfaces.end(), surf2) != Surfaces.end())
				continue;
			if (collision.LineSurface(surf2).PolyFlags & (PF_AutoUPan | PF_AutoVPan))
				continue;
			UMover* mover2 = nullptr;
			if (!SurfaceFrame(level, surf2, base2, normal2, &mover2))
				continue;

			// Facing within an angle of 0.7's cosine of the first, either way
			float facing = dot(normal2, normal) / (length(normal) * length(normal2));
			if (!(std::abs(facing) > 0.7f))
				continue;

			// Each corner taken to the plane square on, then back toward
			// where the first surface's normal takes it by the two distances'
			// difference over the facing
			vec3 world2[4];
			for (int k = 0; k < 4; k++)
			{
				vec3 a = world[k] - normal;
				vec3 alongFirst = a + (world[k] - a) * (dot(base2 - a, normal2) / dot(world[k] - a, normal2));
				vec3 b = world[k] - normal2;
				vec3 square = b + (world[k] - b) * (dot(base2 - b, normal2) / dot(world[k] - b, normal2));
				float squareDist = length(world[k] - square) / facing;
				float firstDist = length(world[k] - alongFirst) / facing;
				world2[k] = square - Normal8(square - alongFirst) * (firstDist - squareDist);
			}

			// On a mover's polygon with the mover's rotation; the original's
			// keeps none, so it is drawn turned by the mover's whole rotation
			LevelDecal decal2;
			decal2.Decal = this;
			if (mover2)
				decal2.MoverRotation = mover2->Rotation();
			AttachTo(surf2, decal2, world2, base2, normal2);
		}
	}

	return Level();
}

// The decal on a surface: on each node of a level surface its corners
// cover, clipped in the surface's plane (0x103e9340), or on all of them
// where it covers none, as the original's decal with no nodes listed is
// drawn on each; on a mover's polygon wherever its fragments are, the
// original listing a filed surface no nodes. Its corners relative to the
// surface's base.
void UDecal::AttachTo(int surf, LevelDecal& decal, const vec3* world, const vec3& base, const vec3& normal)
{
	ULevel* level = XLevel();
	for (int k = 0; k < 4; k++)
		decal.Vertices[k] = world[k] - base;
	Surfaces.push_back(surf);

	if (Array<LevelDecal>* filed = level->Collision.FiledDecals(surf))
	{
		InsertDecal(*filed, decal);
		FiledSurfaces.push_back(surf);
		return;
	}

	UModel* model = level->Model;
	const Array<int>& nodes = model->SurfaceNodes(surf);
	size_t first = Nodes.size();
	for (int index : nodes)
	{
		if (OnNode(model, model->Nodes[index], world, normal))
		{
			InsertDecal(model->Nodes[index].Decals, decal);
			Nodes.push_back(&model->Nodes[index]);
		}
	}
	if (Nodes.size() == first)
	{
		for (int index : nodes)
		{
			InsertDecal(model->Nodes[index].Decals, decal);
			Nodes.push_back(&model->Nodes[index]);
		}
	}
}

void UDecal::DetachDecal()
{
	for (BspNode* node : Nodes)
	{
		auto& decals = node->Decals;
		auto it = decals.begin();
		while (it != decals.end())
		{
			auto& leveldecal = *it;
			if (leveldecal.Decal == this)
				it = decals.erase(it);
			else
				++it;
		}
	}
	Nodes.clear();

	// A new tracker's surfaces hold none of the old one's decals
	ULevel* level = XLevel();
	for (int surf : FiledSurfaces)
	{
		Array<LevelDecal>* decals = level ? level->Collision.FiledDecals(surf) : nullptr;
		if (!decals)
			continue;
		auto it = decals->begin();
		while (it != decals->end())
		{
			if (it->Decal == this)
				it = decals->erase(it);
			else
				++it;
		}
	}
	FiledSurfaces.clear();
	Surfaces.clear();
}

void UDecal::OnGCDestroy()
{
	DetachDecal();
	UActor::OnGCDestroy();
}
