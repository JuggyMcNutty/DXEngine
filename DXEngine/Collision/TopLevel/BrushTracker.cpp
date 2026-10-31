#include "Precomp.h"
#include "BrushTracker.h"
#include "CollisionSystem.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UPolys.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Math/coords.h"
#include "GC/GC.h"

// The node flags the original's sphere filter leaves on the nodes it sorts
// (UModel::PrecomputeSphereFilter): the sphere wholly in front, or behind
static const uint8_t NF_IsFront = 0x40;
static const uint8_t NF_IsBack = 0x80;

// The original's solid node, to the filing: a polygon's, neither NF_NotCsg
// nor NF_IsNew
static bool Solid(const BspNode& node)
{
	return node.NumVertices != 0 && (node.NodeFlags & (NF_NotCsg | NF_IsNew)) == 0;
}

void BrushTracker::Clear()
{
	Level = nullptr;
	LineNodes.clear();
	Slots.clear();
	VertOwners.clear();
	SlotVerts.clear();
	FiledSurfaces.clear();
	Movers.clear();
	FirstNode = NextNode = 0;
	VertPoolSize = NextVert = 0;
	Filing = nullptr;
}

// The tracker's start (Engine.dll 0x1037ac40): each moving brush -- one with
// a brush, not bStatic -- given a surface for each of its polygons
// (AddActor, 0x1037b5c0), room made past the level's own nodes and vertices,
// then every one filed (the update with no mover, 0x1037a400), each queued
// in the actor list's order ahead of the last, so filed from the list's end
void BrushTracker::Start(ULevel* level)
{
	Clear();
	if (!level || !level->Model)
		return;
	Level = level;
	UModel* model = level->Model;

	LineNodes.reserve(model->Nodes.size());
	for (const BspNode& node : model->Nodes)
	{
		BspNode copy;
		copy.PlaneX = node.PlaneX;
		copy.PlaneY = node.PlaneY;
		copy.PlaneZ = node.PlaneZ;
		copy.PlaneW = node.PlaneW;
		copy.ZoneMask = node.ZoneMask;
		copy.NodeFlags = node.NodeFlags;
		copy.VertPool = node.VertPool;
		copy.Surf = node.Surf;
		copy.Back = node.Back;
		copy.Front = node.Front;
		copy.Plane = node.Plane;
		copy.CollisionBound = node.CollisionBound;
		copy.RenderBound = node.RenderBound;
		copy.Zone0 = node.Zone0;
		copy.Zone1 = node.Zone1;
		copy.NumVertices = node.NumVertices;
		copy.Leaf0 = node.Leaf0;
		copy.Leaf1 = node.Leaf1;
		LineNodes.push_back(copy);
	}

	FirstFiledSurface = (int)model->Surfaces.size();
	for (UActor* actor : level->Actors)
	{
		UMover* mover = UObject::TryCast<UMover>(actor);
		if (!mover || mover->bStatic() || mover->bDeleteMe() || !mover->Brush())
			continue;
		auto tracked = std::make_unique<Tracked>();
		tracked->Mover = mover;
		tracked->FirstSurface = FirstFiledSurface + (int)FiledSurfaces.size();
		UModel* brush = mover->Brush();
		if (brush->Polys)
		{
			int index = 0;
			for (const Poly& poly : brush->Polys->Polys)
			{
				BspSurface surface = {};
				surface.Material = poly.Texture;
				surface.PolyFlags = CollisionSystem::FiledFlags(mover, poly);
				surface.pBase = surface.vNormal = surface.vTextureU = surface.vTextureV = -1;
				surface.LightMap = -1;
				surface.BrushPoly = index++;
				surface.PanU = poly.PanU;
				surface.PanV = poly.PanV;
				surface.BrushActor = mover;
				FiledSurfaces.push_back(surface);
			}
		}
		Movers.push_back(std::move(tracked));
	}

	// Room for twice the level's nodes and 512 more, at most 65,536 nodes in
	// all, and for as many vertices again
	int nodeCount = (int)model->Nodes.size();
	int slotCount = std::max(std::min(512 + 2 * nodeCount, 0x10000) - nodeCount, 0);
	FirstNode = NextNode = nodeCount;
	LineNodes.resize(nodeCount + slotCount);
	Slots.resize(slotCount);
	SlotVerts.resize(slotCount);
	for (int& verts : SlotVerts)
		verts = -1;
	VertPoolSize = 512 + (int)model->Vertices.size();
	VertOwners.resize(VertPoolSize);
	for (Tracked*& owner : VertOwners)
		owner = nullptr;
	NextVert = 0;

	Array<Tracked*> queue;
	for (auto& tracked : Movers)
		Queue(tracked.get(), queue);
	Refile(queue);
}

BrushTracker::Tracked* BrushTracker::Find(UMover* mover)
{
	for (auto& tracked : Movers)
	{
		if (tracked->Mover == mover)
			return tracked.get();
	}
	return nullptr;
}

// Queued ahead of those queued before it, its filing's place and turn
// recorded as the original's marks it (SavedPos and SavedRot)
void BrushTracker::Queue(Tracked* tracked, Array<Tracked*>& queue)
{
	if (tracked->Queued)
		return;
	tracked->Queued = true;
	queue.insert(queue.begin(), tracked);
	if (UMover* mover = tracked->Mover)
	{
		mover->SavedPos() = mover->Location();
		mover->SavedRot() = mover->Rotation();
	}
}

void BrushTracker::Update(UMover* mover)
{
	if (!Level || !mover)
		return;
	Tracked* tracked = Find(mover);
	if (!tracked)
		return;
	if (mover->SavedPos() == mover->Location() && mover->SavedRot() == mover->Rotation())
		return;
	Array<Tracked*> queue;
	Queue(tracked, queue);
	Refile(queue);
}

// The original's update past its queue (0x1037a400): the nodes of the movers
// queued taken out, which queues the movers filed under them; then every
// queued mover's nodes freed, and each filed afresh, the last queued first
void BrushTracker::Refile(Array<Tracked*>& queue)
{
	Array<Tracked*> first = queue;
	for (Tracked* tracked : first)
	{
		for (int node : tracked->Nodes)
			TakeOut(node, queue);
	}
	for (Tracked* tracked : queue)
		Free(tracked);
	for (Tracked* tracked : queue)
		File(tracked);
}

// A node taken out (0x1037c4f0): it and every node below it -- fronts, backs
// and planes -- flagged NF_IsNew, each one's mover queued
void BrushTracker::TakeOut(int node, Array<Tracked*>& queue)
{
	BspNode& n = LineNodes[node];
	if (n.NodeFlags & NF_IsNew)
		return;
	n.NodeFlags |= NF_IsNew;
	if (n.Front != -1)
		TakeOut(n.Front, queue);
	if (n.Back != -1)
		TakeOut(n.Back, queue);
	if (n.Plane != -1)
		TakeOut(n.Plane, queue);
	if (Tracked* owner = Slots[node - FirstNode].Owner)
		Queue(owner, queue);
}

// A mover's nodes freed (0x1037c6d0): each unhooked from its parent, its
// vertices and slot let go
void BrushTracker::Free(Tracked* tracked)
{
	for (int node : tracked->Nodes)
	{
		Slot& slot = Slots[node - FirstNode];
		BspNode& parent = LineNodes[slot.Parent];
		if (parent.Front == node)
			parent.Front = -1;
		else if (parent.Back == node)
			parent.Back = -1;
		else if (parent.Plane == node)
			parent.Plane = -1;
		int verts = SlotVerts[node - FirstNode];
		if (verts >= 0)
		{
			for (int i = 0; i < LineNodes[node].NumVertices; i++)
				VertOwners[verts + i] = nullptr;
		}
		SlotVerts[node - FirstNode] = -1;
		slot = Slot();
	}
	tracked->Nodes.clear();
}

// One mover filed where it is: its polygons in the world, the nodes sorted
// against the sphere round them, each polygon from the root; then its nodes
// lose NF_IsNew. A mover gone (destroyed: the original keeps a pointer to
// freed memory) is not filed again.
void BrushTracker::File(Tracked* tracked)
{
	tracked->Queued = false;
	UMover* mover = tracked->Mover;
	if (!mover || !mover->Brush() || !mover->Brush()->Polys)
		return;
	Filing = tracked;

	mat4 objectToWorld = mat4::translate(mover->Location()) * Coords::Rotation(mover->Rotation()).ToMatrix() * mat4::scale(mover->MainScale().Scale) * mat4::translate(-mover->PrePivot());
	mat4 rotation = Coords::Rotation(mover->Rotation()).ToMatrix();
	Array<Fragment> polys;
	Array<vec3> points;
	for (const Poly& poly : mover->Brush()->Polys->Polys)
	{
		Fragment fragment;
		for (const vec3& v : poly.Vertices)
		{
			vec3 world = (objectToWorld * vec4(v, 1.0f)).xyz();
			fragment.Vertices.push_back(world);
			points.push_back(world);
		}
		fragment.Base = (objectToWorld * vec4(poly.Base, 1.0f)).xyz();
		vec3 normal = (rotation * vec4(poly.Normal, 0.0f)).xyz();
		float len = length(normal);
		fragment.Normal = len > 0.0f ? normal / len : vec3(0.0f);
		fragment.PolyFlags = poly.PolyFlags;
		polys.push_back(std::move(fragment));
	}

	// The sphere round its corners (Core.dll's FSphere): the middle of their
	// box, 1.001 times as far out as the farthest
	if (!points.empty())
	{
		vec3 boxMin = points[0], boxMax = points[0];
		for (const vec3& p : points)
		{
			boxMin = vec3(std::min(boxMin.x, p.x), std::min(boxMin.y, p.y), std::min(boxMin.z, p.z));
			boxMax = vec3(std::max(boxMax.x, p.x), std::max(boxMax.y, p.y), std::max(boxMax.z, p.z));
		}
		vec4 sphere((boxMin.x + boxMax.x) * 0.5f, (boxMin.y + boxMax.y) * 0.5f, (boxMin.z + boxMax.z) * 0.5f, 0.0f);
		for (const vec3& p : points)
		{
			float dy = sphere.y - p.y;
			float dz = sphere.z - p.z;
			float dx = sphere.x - p.x;
			float d = dy * dy + dx * dx + dz * dz;
			if (d > sphere.w)
				sphere.w = d;
		}
		sphere.w = (float)(std::sqrt(sphere.w) * 1.001);
		PrecomputeSphereFilter(sphere);
	}

	if (!LineNodes.empty())
	{
		for (size_t i = 0; i < polys.size(); i++)
			FilterPoly(tracked->FirstSurface + (int)i, 0, -1, polys[i], true);
	}
	for (int node : tracked->Nodes)
		LineNodes[node].NodeFlags &= ~NF_IsNew;
	mover->Filed = true;
	Filing = nullptr;
}

// UModel::PrecomputeSphereFilter (Engine.dll 0x103f6460): down from the root,
// each node the sphere lies wholly in front of or behind flagged so, the
// walk going that way; one it straddles flagged neither, its back sorted
// too, the walk going on in front
void BrushTracker::PrecomputeSphereFilter(const vec4& sphere)
{
	PrecomputeSphereFilterBack(0, sphere);
}

void BrushTracker::PrecomputeSphereFilterBack(int node, const vec4& sphere)
{
	while (node != -1)
	{
		BspNode& n = LineNodes[node];
		n.NodeFlags &= 0x3f;
		float d = n.PlaneZ * sphere.z + n.PlaneY * sphere.y + sphere.x * n.PlaneX - n.PlaneW;
		if (d >= -sphere.w)
		{
			if (d <= sphere.w)
			{
				if (n.Back != -1)
					PrecomputeSphereFilterBack(n.Back, sphere);
			}
			else
			{
				n.NodeFlags |= NF_IsFront;
			}
			node = n.Front;
		}
		else
		{
			n.NodeFlags |= NF_IsBack;
			node = n.Back;
		}
	}
}

namespace
{
	enum { SplitCoplanar = 0, SplitFront = 1, SplitBack = 2, SplitBoth = 3 };

	// FPoly::SplitWithPlaneFast (Engine.dll 0x10386ad0): each corner behind
	// the plane when under 0 from it, the polygon in front when one is over
	// 0.25 in front and none under -0.25, and so behind; split where corners
	// lie past 0.25 on both sides, each edge's crossing given to both parts
	int SplitWithPlaneFast(const Array<vec3>& vertices, const vec4& plane, Array<vec3>* front, Array<vec3>* back)
	{
		int count = (int)vertices.size();
		if (count <= 0)
			return SplitCoplanar;
		bool anyFront = false, anyBack = false;
		Array<int> behind;
		behind.resize(count);
		for (int i = 0; i < count; i++)
		{
			const vec3& v = vertices[i];
			float dist = plane.z * v.z + plane.x * v.x + plane.y * v.y - plane.w;
			if (dist < 0.0f)
			{
				behind[i] = 1;
				if (dist < -0.25f)
					anyBack = true;
			}
			else
			{
				behind[i] = 0;
				if (dist > 0.25f)
					anyFront = true;
			}
		}
		if (!anyFront)
			return anyBack ? SplitBack : SplitCoplanar;
		if (!anyBack)
			return SplitFront;

		front->clear();
		back->clear();
		int prevBehind = behind[count - 1];
		vec3 prev = vertices[count - 1];
		for (int i = 0; i < count; i++)
		{
			const vec3& v = vertices[i];
			if (behind[i] != prevBehind)
			{
				vec3 d = v - prev;
				float t = (plane.w - (plane.x * prev.x + plane.z * prev.z + plane.y * prev.y)) / (plane.z * d.z + plane.y * d.y + d.x * plane.x);
				vec3 crossing(d.x * t + prev.x, d.y * t + prev.y, d.z * t + prev.z);
				back->push_back(crossing);
				front->push_back(crossing);
			}
			if (behind[i])
				back->push_back(v);
			else
				front->push_back(v);
			prevBehind = behind[i];
			prev = v;
		}
		return SplitBoth;
	}

	// FPoly::SplitInHalf (0x10386f70): the first half's corners up to the
	// middle one, the other half from there round to the first
	void SplitInHalf(Array<vec3>& first, Array<vec3>& second)
	{
		int count = (int)first.size();
		int half = count / 2;
		second.clear();
		for (int i = half; i < count; i++)
			second.push_back(first[i]);
		second.push_back(first[0]);
		first.resize(half + 1);
	}
}

// FilterPoly (Engine.dll 0x1037c0c0): a polygon down the tree from a node
void BrushTracker::FilterPoly(int surface, int node, int coplanarParent, Fragment& poly, bool outside)
{
	while (true)
	{
		BspNode* n = nullptr;
		bool toFront = false;
		while (true)
		{
			n = &LineNodes[node];
			if (poly.Vertices.size() >= 14)
			{
				Fragment half;
				half.Base = poly.Base;
				half.Normal = poly.Normal;
				half.PolyFlags = poly.PolyFlags;
				SplitInHalf(poly.Vertices, half.Vertices);
				FilterPoly(surface, node, coplanarParent, half, outside);
			}
			if (n->NodeFlags & NF_IsFront)
			{
				toFront = true;
			}
			else if (n->NodeFlags & NF_IsBack)
			{
				toFront = false;
			}
			else
			{
				vec4 plane(n->PlaneX, n->PlaneY, n->PlaneZ, n->PlaneW);
				Fragment frontPart, backPart;
				int result = SplitWithPlaneFast(poly.Vertices, plane, &frontPart.Vertices, &backPart.Vertices);
				if (result == SplitFront)
				{
					toFront = true;
				}
				else if (result == SplitBack)
				{
					toFront = false;
				}
				else if (result == SplitBoth)
				{
					// The front part down the front, the back part on behind
					for (Fragment* part : { &frontPart, &backPart })
					{
						part->Base = poly.Base;
						part->Normal = poly.Normal;
						part->PolyFlags = poly.PolyFlags;
					}
					bool frontOutside = outside || Solid(*n);
					if (n->Front == -1)
					{
						if (frontOutside)
							AddPolyFragment(surface, node, coplanarParent, 1, frontPart);
					}
					else
					{
						FilterPoly(surface, n->Front, coplanarParent, frontPart, frontOutside);
					}
					n = &LineNodes[node];
					poly = std::move(backPart);
					toFront = false;
				}
				else
				{
					// In the plane: down the front, the plane its own when it
					// faces as the node does
					if (dot(poly.Normal, vec3(n->PlaneX, n->PlaneY, n->PlaneZ)) >= 0.0f)
						coplanarParent = node;
					toFront = true;
				}
			}
			if (!toFront)
				break;
			outside = outside || Solid(*n);
			if (n->Front == -1)
			{
				if (outside)
					AddPolyFragment(surface, node, coplanarParent, 1, poly);
				return;
			}
			node = n->Front;
		}
		outside = outside && !Solid(*n);
		if (n->Back == -1)
			break;
		node = n->Back;
	}
	if (outside)
		AddPolyFragment(surface, node, coplanarParent, 0, poly);
}

// AddPolyFragment (Engine.dll 0x1037bb00): a node for a fragment in empty
// space, hung from its parent as that side's child, or for one in a node's
// plane at the end of the plane's chain; dropped when no slot or vertex room
// is left
void BrushTracker::AddPolyFragment(int surface, int parent, int coplanarParent, int side, const Fragment& poly)
{
	int linkParent = parent;
	int link = side;
	bool otherNode = false;
	if (coplanarParent != -1)
	{
		otherNode = parent != coplanarParent;
		link = 2;
		linkParent = coplanarParent;
		while (LineNodes[linkParent].Plane != -1)
			linkParent = LineNodes[linkParent].Plane;
	}

	int node = AllocNode();
	if (node < 0)
		return;
	Slots[node - FirstNode] = { Filing, linkParent };

	BspNode& n = LineNodes[node];
	const BspNode& from = LineNodes[linkParent];
	n.Surf = surface;
	n.CollisionBound = -1;
	n.RenderBound = -1;
	n.ZoneMask = from.ZoneMask;
	n.Front = n.Back = n.Plane = -1;
	n.NumVertices = (uint8_t)poly.Vertices.size();
	n.ActorList = nullptr;
	n.Decals.clear();
	n.PlaneX = poly.Normal.x;
	n.PlaneY = poly.Normal.y;
	n.PlaneZ = poly.Normal.z;
	n.PlaneW = (poly.Base.y * poly.Normal.y + poly.Base.z * poly.Normal.z) + poly.Base.x * poly.Normal.x;
	n.NodeFlags = (poly.PolyFlags & PF_NotSolid) ? (NF_IsNew | NF_NotCsg) : NF_IsNew;
	if (poly.PolyFlags & (PF_Invisible | PF_Portal))
		n.NodeFlags |= NF_NotVisBlocking;
	if (poly.PolyFlags & PF_Masked)
		n.NodeFlags |= NF_ShootThrough;

	// Its zones and leaves: the parent's on its side, or the plane node's,
	// turned as the fragment faces it
	const BspNode& owner = LineNodes[parent];
	if (coplanarParent == -1 || otherNode)
	{
		n.Zone0 = n.Zone1 = side ? owner.Zone1 : owner.Zone0;
		n.Leaf0 = n.Leaf1 = side ? owner.Leaf1 : owner.Leaf0;
	}
	else
	{
		bool turned = from.PlaneZ * n.PlaneZ + from.PlaneY * n.PlaneY + from.PlaneX * n.PlaneX < 0.0f;
		n.Zone0 = turned ? from.Zone1 : from.Zone0;
		n.Leaf0 = turned ? from.Leaf1 : from.Leaf0;
		n.Zone1 = turned ? from.Zone0 : from.Zone1;
		n.Leaf1 = turned ? from.Leaf0 : from.Leaf1;
	}
	LineNodes[parent].ZoneMask |= (1ULL << (n.Zone0 & 63)) | (1ULL << (n.Zone1 & 63));

	int verts = AllocVerts(n.NumVertices);
	if (verts < 0)
	{
		Slots[node - FirstNode] = Slot();
		return;
	}
	SlotVerts[node - FirstNode] = verts;
	n.VertPool = -1; // its corners are not kept: no line walk reads them

	BspNode& hang = LineNodes[linkParent];
	if (link == 0)
		hang.Back = node;
	else if (link == 1)
		hang.Front = node;
	else
		hang.Plane = node;
	Filing->Nodes.push_back(node);
}

// A free node slot, from the last one taken on, then from the first back to
// it
int BrushTracker::AllocNode()
{
	int count = (int)Slots.size();
	for (int i = NextNode - FirstNode; i < count; i++)
	{
		if (!Slots[i].Owner)
		{
			NextNode = FirstNode + i;
			return NextNode;
		}
	}
	for (int i = 0; i < NextNode - FirstNode; i++)
	{
		if (!Slots[i].Owner)
		{
			NextNode = FirstNode + i;
			return NextNode;
		}
	}
	return -1;
}

// A run of free vertex slots, as the original's search finds one: on from
// where the last ended while the run would end short of the pool's end, then
// from the pool's start while it would end short of where the search began
int BrushTracker::AllocVerts(int count)
{
	int start = NextVert;
	int run = 0;
	for (int k = NextVert; ; k++)
	{
		NextVert = k;
		if (k + count >= VertPoolSize)
		{
			run = 0;
			for (int m = 0; ; m++)
			{
				NextVert = m;
				if (m + count >= start)
					return -1;
				if (VertOwners[m])
					run = 0;
				else if (++run >= count)
				{
					for (int i = m - count + 1; i <= m; i++)
						VertOwners[i] = Filing;
					return m - count + 1;
				}
			}
		}
		if (VertOwners[k])
		{
			run = 0;
			continue;
		}
		if (++run >= count)
		{
			for (int i = k - count + 1; i <= k; i++)
				VertOwners[i] = Filing;
			return k - count + 1;
		}
	}
}

const BspSurface& BrushTracker::Surface(int index) const
{
	if (index < FirstFiledSurface)
		return Level->Model->Surfaces[index];
	return FiledSurfaces[index - FirstFiledSurface];
}

void BrushTracker::Mark(GCMarker& marker)
{
	marker.SetField("BrushTracker");
	for (auto& tracked : Movers)
		marker.Mark(tracked->Mover);
	for (BspSurface& surface : FiledSurfaces)
	{
		marker.MarkConst(surface.Material);
		marker.Mark(surface.BrushActor);
	}
}
