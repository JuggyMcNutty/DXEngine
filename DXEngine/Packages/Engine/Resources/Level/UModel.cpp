
#include "Precomp.h"
#include "Packages/Engine/Actors/Info/UZoneInfo.h"
#include "UModel.h"
#include "UVectors.h"
#include "UBspNodes.h"
#include "UBspSurfs.h"
#include "UVerts.h"
#include "UPolys.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "Utils/Logger.h"
#include "Engine.h"

int UModel::FindLeafAt(const vec3& location)
{
	if (Nodes.empty())
		return -1;
	vec4 p = vec4(location, 1.0f);
	BspNode* nodes = Nodes.data();
	BspNode* node = nodes;
	while (true)
	{
		vec4 plane = { node->PlaneX, node->PlaneY, node->PlaneZ, -node->PlaneW };
		bool swapFrontAndBack = dot(p, plane) < 0.0f;
		int front = node->Front;
		if (swapFrontAndBack)
			front = node->Back;
		if (front >= 0)
			node = nodes + front;
		else
			return swapFrontAndBack ? node->Leaf0 : node->Leaf1;
	}
}

void UModel::Load(ObjectStream* stream)
{
	UPrimitive::Load(stream);

	if (stream->GetVersion() <= 61)
	{
		OldFormat.Vectors = stream->ReadObject<UVectors>();
		OldFormat.Points = stream->ReadObject<UVectors>();
		OldFormat.Nodes = stream->ReadObject<UBspNodes>();
		OldFormat.Surfaces = stream->ReadObject<UBspSurfs>();
		OldFormat.Verts = stream->ReadObject<UVerts>();

		OldFormat.Vectors->LoadNow();
		OldFormat.Points->LoadNow();
		OldFormat.Nodes->LoadNow();
		OldFormat.Surfaces->LoadNow();
		OldFormat.Verts->LoadNow();

		Vectors = OldFormat.Vectors->Vectors;
		Points = OldFormat.Points->Vectors;
		Nodes = OldFormat.Nodes->Nodes;
		Zones = OldFormat.Nodes->Zones;
		NumZones = (int)Zones.size();
		if (Zones.size() < 64)
			Zones.resize(64);
		Surfaces = OldFormat.Surfaces->Surfaces;
		Vertices = OldFormat.Verts->Vertices;
		NumSharedSides = OldFormat.Verts->NumSharedSides;
	}
	else
	{
		int count = stream->ReadIndex();
		for (int i = 0; i < count; i++)
		{
			vec3 v;
			v.x = stream->ReadFloat();
			v.y = stream->ReadFloat();
			v.z = stream->ReadFloat();
			Vectors.push_back(v);
		}

		count = stream->ReadIndex();
		for (int i = 0; i < count; i++)
		{
			vec3 v;
			v.x = stream->ReadFloat();
			v.y = stream->ReadFloat();
			v.z = stream->ReadFloat();
			Points.push_back(v);
		}

		count = stream->ReadIndex();
		for (int i = 0; i < count; i++)
		{
			BspNode node;
			node.PlaneX = stream->ReadFloat();
			node.PlaneY = stream->ReadFloat();
			node.PlaneZ = stream->ReadFloat();
			node.PlaneW = stream->ReadFloat();
			node.ZoneMask = stream->ReadUInt64();
			// NF_IsNew, NF_IsFront and NF_IsBack, which play sets (the
			// original's saves and ten of the game's maps carry them),
			// cleared, as the original's load clears them (Engine.dll
			// 0x103aa780)
			node.NodeFlags = stream->ReadUInt8() & 0x1F;
			node.VertPool = stream->ReadIndex();
			node.Surf = stream->ReadIndex();
			node.Back = stream->ReadIndex();
			node.Front = stream->ReadIndex();
			node.Plane = stream->ReadIndex();
			node.CollisionBound = stream->ReadIndex();
			node.RenderBound = stream->ReadIndex();
			node.Zone0 = stream->ReadIndex();
			node.Zone1 = stream->ReadIndex();
			node.NumVertices = stream->ReadUInt8();
			node.Leaf0 = stream->ReadInt32();
			node.Leaf1 = stream->ReadInt32();
			Nodes.push_back(node);
		}

		count = stream->ReadIndex();
		for (int i = 0; i < count; i++)
		{
			BspSurface surface;
			surface.Material = stream->ReadObject<UTexture>();
			// The editor's PF_Highlighted cleared, as the original's load
			// clears it (0x103aa3a0): a surface saved highlighted draws as
			// any other
			surface.PolyFlags = stream->ReadUInt32() & ~(uint32_t)PF_Highlighted;
			surface.pBase = stream->ReadIndex();
			surface.vNormal = stream->ReadIndex();
			surface.vTextureU = stream->ReadIndex();
			surface.vTextureV = stream->ReadIndex();
			surface.LightMap = stream->ReadIndex();
			surface.BrushPoly = stream->ReadIndex();
			surface.PanU = stream->ReadInt16();
			surface.PanV = stream->ReadInt16();
			surface.BrushActor = stream->ReadObject<UActor>();
			Surfaces.push_back(surface);
		}

		count = stream->ReadIndex();
		for (int i = 0; i < count; i++)
		{
			BspVert vert;
			vert.Vertex = stream->ReadIndex();
			vert.Side = stream->ReadIndex();
			Vertices.push_back(vert);
		}

		NumSharedSides = stream->ReadInt32();

		int32_t NumZones = stream->ReadInt32();
		for (int i = 0; i < NumZones; i++)
		{
			ZoneProperties zone;
			zone.ZoneActor = stream->ReadObject<UActor>();
			zone.Connectivity = stream->ReadUInt64();
			zone.Visibility = stream->ReadUInt64();
			Zones.push_back(zone);
		}
		this->NumZones = NumZones;
		if (NumZones < 64)
			Zones.resize(64);
	}

	Polys = stream->ReadObject<UPolys>();

	static uint32_t NextLMCacheID = 0; // For easier unique CacheIDs for lightmap textures

	int count = stream->ReadIndex();
	for (int i = 0; i < count; i++)
	{
		LightMapIndex entry;
		entry.DataOffset = stream->ReadInt32();
		entry.PanX = stream->ReadFloat();
		entry.PanY = stream->ReadFloat();
		entry.PanZ = stream->ReadFloat();
		entry.UClamp = stream->ReadIndex();
		entry.VClamp = stream->ReadIndex();
		entry.UScale = stream->ReadFloat();
		entry.VScale = stream->ReadFloat();
		entry.LightActors = stream->ReadInt32();
		entry.LMCacheID = NextLMCacheID++;
		LightMap.push_back(entry);
	}

	LightBits.resize(stream->ReadIndex());
	stream->ReadBytes(LightBits.data(), (uint32_t)LightBits.size());

	count = stream->ReadIndex();
	for (int i = 0; i < count; i++)
	{
		BBox boundingBox;
		boundingBox.min.x = stream->ReadFloat();
		boundingBox.min.y = stream->ReadFloat();
		boundingBox.min.z = stream->ReadFloat();
		boundingBox.max.x = stream->ReadFloat();
		boundingBox.max.y = stream->ReadFloat();
		boundingBox.max.z = stream->ReadFloat();
		boundingBox.IsValid = stream->ReadInt8() != 0;
		Bounds.push_back(boundingBox);
	}

	count = stream->ReadIndex();
	for (int i = 0; i < count; i++)
	{
		LeafHulls.push_back(stream->ReadInt32());
	}

	count = stream->ReadIndex();
	for (int i = 0; i < count; i++)
	{
		ConvexVolumeLeaf leaf;
		leaf.Zone = stream->ReadIndex();
		leaf.Permeating = stream->ReadIndex();
		leaf.Volumetric = stream->ReadIndex();
		leaf.VisibleZones = stream->ReadUInt64();
		Leaves.push_back(leaf);
	}

	count = stream->ReadIndex();
	for (int i = 0; i < count; i++)
	{
		Lights.push_back(stream->ReadObject<UActor>());
	}

	if (stream->GetVersion() <= 61)
	{
		OldFormat.Unknown1 = stream->ReadObject<UObject>();
		OldFormat.Unknown2 = stream->ReadObject<UObject>();
	}

	RootOutside = stream->ReadInt32();
	Linked = stream->ReadInt32();

	// The package's indexes checked once, here, rather than on every walk:
	// a hand-made map's surface or node pointing outside its arrays refused
	for (size_t i = 0; i < Surfaces.size(); i++)
	{
		const BspSurface& surface = Surfaces[i];
		if (surface.pBase < 0 || (size_t)surface.pBase >= Points.size() ||
			surface.vTextureU < 0 || (size_t)surface.vTextureU >= Vectors.size() ||
			surface.vTextureV < 0 || (size_t)surface.vTextureV >= Vectors.size() ||
			surface.vNormal < 0 || (size_t)surface.vNormal >= Vectors.size())
			Exception::Throw("Model " + Name.ToString() + ": surface " + std::to_string(i) + " points outside its points or vectors");
	}
	for (size_t i = 0; i < Nodes.size(); i++)
	{
		const BspNode& node = Nodes[i];
		if (node.Front >= (int)Nodes.size() || node.Back >= (int)Nodes.size() || node.Plane >= (int)Nodes.size() ||
			node.Zone0 < 0 || (size_t)node.Zone0 >= Zones.size() || node.Zone1 < 0 || (size_t)node.Zone1 >= Zones.size())
			Exception::Throw("Model " + Name.ToString() + ": node " + std::to_string(i) + " points outside its nodes or zones");
		// The walks' other reads: the node's surface, its render box, its
		// vertices and their points, and its collision hull (a list of node
		// indexes in LeafHulls, a negative one ending it, the box's six floats
		// after). The vertex pool's entries no node names are left alone:
		// every map of the game has thousands past its points.
		if (node.Surf >= (int)Surfaces.size() || node.RenderBound >= (int)Bounds.size())
			Exception::Throw("Model " + Name.ToString() + ": node " + std::to_string(i) + " points outside its surfaces or bounds");
		if (node.NumVertices > 0)
		{
			if (node.VertPool < 0 || (size_t)node.VertPool + node.NumVertices > Vertices.size())
				Exception::Throw("Model " + Name.ToString() + ": node " + std::to_string(i) + " points outside its vertices");
			for (int k = 0; k < node.NumVertices; k++)
			{
				const BspVert& vert = Vertices[(size_t)node.VertPool + k];
				if (vert.Vertex < 0 || (size_t)vert.Vertex >= Points.size())
					Exception::Throw("Model " + Name.ToString() + ": node " + std::to_string(i) + "'s vertex " + std::to_string(k) + " points outside its points");
			}
		}
		if (node.CollisionBound >= 0)
		{
			size_t k = (size_t)node.CollisionBound;
			while (k < LeafHulls.size() && LeafHulls[k] >= 0)
			{
				if ((size_t)(LeafHulls[k] & ~0x4000'0000) >= Nodes.size())
					Exception::Throw("Model " + Name.ToString() + ": node " + std::to_string(i) + "'s hull names a node outside its nodes");
				k++;
			}
			if (k + 1 + 6 > LeafHulls.size())
				Exception::Throw("Model " + Name.ToString() + ": node " + std::to_string(i) + "'s hull runs outside its leaf hulls");
		}
	}

	// Calculate center and radius for all surface lightmaps:
	for (BspSurface& surface : Surfaces)
	{
		if (surface.LightMap < 0 || (size_t)surface.LightMap >= LightMap.size())
			continue;

		// There is a simpler version possible here somehow...

		Coords mapCoords;
		mapCoords.Origin = Points[surface.pBase];
		mapCoords.XAxis = Vectors[surface.vTextureU];
		mapCoords.YAxis = Vectors[surface.vTextureV];
		mapCoords.ZAxis = Vectors[surface.vNormal];

		const LightMapIndex& lmindex = LightMap[surface.LightMap];
		int width = lmindex.UClamp;
		int height = lmindex.VClamp;

		float UDot = dot(mapCoords.XAxis, mapCoords.Origin);
		float VDot = dot(mapCoords.YAxis, mapCoords.Origin);
		float LMUPan = UDot + lmindex.PanX - 0.5f * lmindex.UScale;
		float LMVPan = VDot + lmindex.PanY - 0.5f * lmindex.VScale;
		float LMUMult = 1.0f / lmindex.UScale;
		float LMVMult = 1.0f / lmindex.VScale;

		vec3 p[3] =
		{
			mapCoords.Origin,
			mapCoords.Origin + mapCoords.XAxis,
			mapCoords.Origin + mapCoords.YAxis
		};

		vec2 uv[3];
		for (int j = 0; j < 3; j++)
		{
			uv[j] =
			{
				(dot(mapCoords.XAxis, p[j]) - LMUPan) * LMUMult,
				(dot(mapCoords.YAxis, p[j]) - LMVPan) * LMVMult
			};
		}

		float leftDX = uv[2].x - uv[0].x;
		float leftDY = uv[2].y - uv[0].y;
		float leftStep = leftDX / leftDY;
		float rightDX = uv[2].x - uv[1].x;
		float rightDY = uv[2].y - uv[1].y;
		float rightStep = rightDX / rightDY;

		vec3 aabbMin = { 0.0f }, aabbMax = { 0.0f };
		for (int y : { 0, height - 1 })
		{
			float x0 = uv[0].x + leftStep * (y + 0.5f - uv[0].y) + 0.5f;
			float x1 = uv[1].x + rightStep * (y + 0.5f - uv[1].y) + 0.5f;
			float t0 = (y + 0.5f - uv[0].y) / leftDY;
			float t1 = (y + 0.5f - uv[1].y) / rightDY;
			vec3 p0 = mix(p[0], p[2], t0);
			vec3 p1 = mix(p[1], p[2], t1);

			float left = (0.5f - x0) / (x1 - x0);
			float right = (width - 0.5f - x0) / (x1 - x0);
			vec3 posleft = mix(p0, p1, left);
			vec3 posright = mix(p0, p1, right);

			if (y == 0)
			{
				aabbMin = posleft;
				aabbMax = posleft;
			}
			else
			{
				aabbMin.x = std::min(aabbMin.x, posleft.x);
				aabbMin.y = std::min(aabbMin.y, posleft.y);
				aabbMin.z = std::min(aabbMin.z, posleft.z);
				aabbMax.x = std::max(aabbMax.x, posleft.x);
				aabbMax.y = std::max(aabbMax.y, posleft.y);
				aabbMax.z = std::max(aabbMax.z, posleft.z);
			}
			aabbMin.x = std::min(aabbMin.x, posright.x);
			aabbMin.y = std::min(aabbMin.y, posright.y);
			aabbMin.z = std::min(aabbMin.z, posright.z);
			aabbMax.x = std::max(aabbMax.x, posright.x);
			aabbMax.y = std::max(aabbMax.y, posright.y);
			aabbMax.z = std::max(aabbMax.z, posright.z);
		}

		auto halfmin = aabbMin * 0.5f;
		auto halfmax = aabbMax * 0.5f;
		surface.Center = halfmax + halfmin;
		surface.Radius = length(halfmax - halfmin);
	}
}

void UModel::Save(PackageStreamWriter* stream)
{
	UPrimitive::Save(stream);

	if (stream->GetVersion() <= 61)
	{
		stream->WriteObject(OldFormat.Vectors);
		stream->WriteObject(OldFormat.Points);
		stream->WriteObject(OldFormat.Nodes);
		stream->WriteObject(OldFormat.Surfaces);
		stream->WriteObject(OldFormat.Verts);
	}
	else
	{
		stream->WriteIndex((int)Vectors.size());
		for (const vec3& v : Vectors)
		{
			stream->WriteFloat(v.x);
			stream->WriteFloat(v.y);
			stream->WriteFloat(v.z);
		}

		stream->WriteIndex((int)Points.size());
		for (const vec3& v : Points)
		{
			stream->WriteFloat(v.x);
			stream->WriteFloat(v.y);
			stream->WriteFloat(v.z);
		}

		stream->WriteIndex((int)Nodes.size());
		for (const BspNode& node : Nodes)
		{
			stream->WriteFloat(node.PlaneX);
			stream->WriteFloat(node.PlaneY);
			stream->WriteFloat(node.PlaneZ);
			stream->WriteFloat(node.PlaneW);
			stream->WriteUInt64(node.ZoneMask);
			stream->WriteUInt8(node.NodeFlags);
			stream->WriteIndex(node.VertPool);
			stream->WriteIndex(node.Surf);
			stream->WriteIndex(node.Back);
			stream->WriteIndex(node.Front);
			stream->WriteIndex(node.Plane);
			stream->WriteIndex(node.CollisionBound);
			stream->WriteIndex(node.RenderBound);
			stream->WriteIndex(node.Zone0);
			stream->WriteIndex(node.Zone1);
			stream->WriteUInt8(node.NumVertices);
			stream->WriteInt32(node.Leaf0);
			stream->WriteInt32(node.Leaf1);
		}

		stream->WriteIndex((int)Surfaces.size());
		for (const BspSurface& surface : Surfaces)
		{
			stream->WriteObject(surface.Material);
			stream->WriteUInt32(surface.PolyFlags);
			stream->WriteIndex(surface.pBase);
			stream->WriteIndex(surface.vNormal);
			stream->WriteIndex(surface.vTextureU);
			stream->WriteIndex(surface.vTextureV);
			stream->WriteIndex(surface.LightMap);
			stream->WriteIndex(surface.BrushPoly);
			stream->WriteInt16(surface.PanU);
			stream->WriteInt16(surface.PanV);
			stream->WriteObject(surface.BrushActor);
		}

		stream->WriteIndex((int)Vertices.size());
		for (const BspVert& vert : Vertices)
		{
			stream->WriteIndex(vert.Vertex);
			stream->WriteIndex(vert.Side);
		}

		stream->WriteInt32(NumSharedSides);

		stream->WriteInt32((int)Zones.size());
		for (const ZoneProperties& zone : Zones)
		{
			stream->WriteObject(zone.ZoneActor);
			stream->WriteUInt64(zone.Connectivity);
			stream->WriteUInt64(zone.Visibility);
		}
	}

	stream->WriteObject(Polys);

	stream->WriteIndex((int)LightMap.size());
	for (const LightMapIndex& entry : LightMap)
	{
		stream->WriteInt32(entry.DataOffset);
		stream->WriteFloat(entry.PanX);
		stream->WriteFloat(entry.PanY);
		stream->WriteFloat(entry.PanZ);
		stream->WriteIndex(entry.UClamp);
		stream->WriteIndex(entry.VClamp);
		stream->WriteFloat(entry.UScale);
		stream->WriteFloat(entry.VScale);
		stream->WriteInt32(entry.LightActors);
	}

	stream->WriteIndex((int)LightBits.size());
	stream->WriteBytes(LightBits.data(), (uint32_t)LightBits.size());

	stream->WriteIndex((int)Bounds.size());
	for (const BBox& boundingBox : Bounds)
	{
		stream->WriteFloat(boundingBox.min.x);
		stream->WriteFloat(boundingBox.min.y);
		stream->WriteFloat(boundingBox.min.z);
		stream->WriteFloat(boundingBox.max.x);
		stream->WriteFloat(boundingBox.max.y);
		stream->WriteFloat(boundingBox.max.z);
		stream->WriteInt8(boundingBox.IsValid ? 1 : 0);
	}

	stream->WriteIndex((int)LeafHulls.size());
	for (int32_t v : LeafHulls)
		stream->WriteInt32(v);

	stream->WriteIndex((int)Leaves.size());
	for (const ConvexVolumeLeaf& leaf : Leaves)
	{
		stream->WriteIndex(leaf.Zone);
		stream->WriteIndex(leaf.Permeating);
		stream->WriteIndex(leaf.Volumetric);
		stream->WriteUInt64(leaf.VisibleZones);
	}

	stream->WriteIndex((int)Lights.size());
	for (UActor* actor : Lights)
		stream->WriteObject(actor);

	if (stream->GetVersion() <= 61)
	{
		stream->WriteObject(OldFormat.Unknown1);
		stream->WriteObject(OldFormat.Unknown2);
	}

	stream->WriteInt32(RootOutside);
	stream->WriteInt32(Linked);
}

namespace
{
	// The original's walk (Engine.dll 0x103f5db0), T the x87's precision in
	// the tick it runs in, float at 24 bits and double for 64 (X87): from the
	// root, each node's side by the point's distance from its plane -- the
	// Z, Y and X products summed in that order less W, compared with 0
	// unstored: in front at 0 or more, behind under it or for a NaN -- to the
	// child on that side, until there is none; the region is that node's
	// zone and leaf on that side, a node's zones and leaves the back side's,
	// then the front's
	template<typename T>
	void FindLeaf(const BspNode* nodes, const vec3& point, PointRegion& region)
	{
		const BspNode* node = nodes;
		while (true)
		{
			T side = ((T)point.z * (T)node->PlaneZ + (T)point.y * (T)node->PlaneY) + (T)point.x * (T)node->PlaneX - (T)node->PlaneW;
			bool front = !X87::Under(side, 0.0);
			int child = front ? node->Front : node->Back;
			if (child < 0)
			{
				region.ZoneNumber = front ? node->Zone1 : node->Zone0;
				region.BspLeaf = front ? node->Leaf1 : node->Leaf0;
				return;
			}
			node = nodes + child;
		}
	}
}

PointRegion UModel::FindRegion(const vec3& point, UZoneInfo* levelZoneInfo)
{
	// The zone actor is the walked model's own, else the LevelInfo given
	// (an Entry actor's, under a map), as the original's PointRegion takes
	// them; DXEngine took the current level's zone of the number
	auto zoneActor = [&](int n) -> UZoneInfo* {
		if (n >= 0 && (size_t)n < Zones.size())
			if (UZoneInfo* zone = UObject::TryCast<UZoneInfo>(Zones[n].ZoneActor))
				return zone;
		return levelZoneInfo ? levelZoneInfo : engine->GetZoneActor(n);
	};

	// A model with no nodes has no region to find: the zone given, no leaf
	// (-1) and zone 0, as the original's (0x103f5e05)
	if (Nodes.empty())
	{
		static bool messageShown = false;
		if (!messageShown)
		{
			LogMessage("Model.FindRegion encountered an empty model: " + Name.ToString());
			messageShown = true;
		}
		PointRegion region;
		region.BspLeaf = -1;
		region.ZoneNumber = 0;
		region.Zone = zoneActor(region.ZoneNumber);
		return region;
	}

	PointRegion region;
	if (X87::Single)
		FindLeaf<float>(Nodes.data(), point, region);
	else
		FindLeaf<double>(Nodes.data(), point, region);
	region.Zone = zoneActor(region.ZoneNumber);
	return region;
}

const Array<int>& UModel::SurfaceNodes(int surf)
{
	if (SurfNodes.size() != Surfaces.size())
	{
		SurfNodes.clear();
		SurfNodes.resize(Surfaces.size());
		for (size_t i = 0; i < Nodes.size(); i++)
		{
			int s = Nodes[i].Surf;
			if (s >= 0 && (size_t)s < Surfaces.size())
				SurfNodes[s].push_back((int)i);
		}
	}
	static const Array<int> none;
	return surf >= 0 && (size_t)surf < SurfNodes.size() ? SurfNodes[surf] : none;
}

/////////////////////////////////////////////////////////////////////////////

BBox BspNode::GetCollisionBox(UModel* model) const
{
	int32_t* hullIndexList = &model->LeafHulls[CollisionBound];
	int hullPlanesCount = 0;
	while (hullIndexList[hullPlanesCount] >= 0)
		hullPlanesCount++;

	vec3* bboxStart = (vec3*)(&hullIndexList[hullPlanesCount + 1]);

	BBox bbox;
	bbox.min = bboxStart[0];
	bbox.max = bboxStart[1];

	return bbox;
}

void UModel::MarkSurfaces(GCMarker& marker, Array<BspSurface>& surfaces)
{
	marker.SetField("Surfaces");
	for (BspSurface& surface : surfaces)
	{
		marker.Mark(surface.Material);
		marker.Mark(surface.BrushActor);
	}
}

void UModel::MarkZones(GCMarker& marker, Array<ZoneProperties>& zones)
{
	marker.SetField("Zones");
	for (ZoneProperties& zone : zones)
		marker.Mark(zone.ZoneActor);
}

void UModel::Mark(GCMarker& marker)
{
	UPrimitive::Mark(marker);
	MarkSurfaces(marker, Surfaces);
	MarkZones(marker, Zones);
	marker.SetField("OldFormat");
	marker.Mark(OldFormat.Vectors);
	marker.Mark(OldFormat.Points);
	marker.Mark(OldFormat.Nodes);
	marker.Mark(OldFormat.Surfaces);
	marker.Mark(OldFormat.Verts);
	marker.Mark(OldFormat.Unknown1);
	marker.Mark(OldFormat.Unknown2);
	marker.SetField("Polys");
	marker.Mark(Polys);
	// The lists in it are found by place (a light map's LightActors, a
	// leaf's Permeating, each ended by a null): an entry dropped would shift
	// every later list and its shadow bits, so a listed light is kept,
	// destroyed or not
	marker.SetField("Lights");
	for (UActor* light : Lights)
		marker.MarkConst(light);
}
