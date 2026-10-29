
#include "Precomp.h"
#include "VisibleFrame.h"
#include <tuple>
#include "VisibleCorona.h"
#include "RenderSubsystem.h"
#include "RenderDevice/RenderDevice.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/URenderIterator.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/Level/UPolys.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "VisibleBrush.h"
#include "VM/ScriptCall.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "Packages/Engine/Resources/Mesh/UMesh.h"
#include "Packages/Engine/Actors/Info/UWarpZoneInfo.h"
#include "Packages/Engine/Actors/Info/USkyZoneInfo.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Subsystems/UDXEngineRenderDevice.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/UDecal.h"
#include "Packages/Engine/UDXEngineClient.h"
#include "Packages/Extension/Windows/TabGroup/URootWindow.h"

void VisibleFrame::Process(const vec3& location, const mat4& worldToView, const Coords& viewRotation, bool mirrorFlag, int portalDepth, const Array<PortalSpan>& portalSpans, const vec4& portalPlane, int viewZone)
{
	engine->render->Stats.Frames++;

	Device = engine->render->Device;
	FrameCounter = engine->render->FrameCounter++;
	MirrorFlag = mirrorFlag;
	PortalDepth = portalDepth;
	DeusExOrder = true;
	UActor* viewActor = engine->viewport->Actor();
	UZoneInfo* viewActorZone = viewActor ? viewActor->Region().Zone : nullptr;
	VolumetricFog = engine->renderdev->VolumetricLighting && viewActorZone && viewActorZone->bFogZone();

	SetupSceneFrame(worldToView);

	Clipper.numDrawSpans = 0;
	Clipper.numSurfs = 0;
	Clipper.numTris = 0;
	// One grid row per row of the image: finer only costs, coarser could lose
	// what shows through a one-pixel gap. A portal's frame is the same size,
	// so the spans it inherits are in the same grid.
	Clipper.SetViewportSize(Frame.X, Frame.Y);
	// A surface's pixel taken by its centre, as the original's span buffer
	// takes it (BspClipper::SetPixelSample); a proxy's in TestFragment.
	Clipper.SetPixelSample(0.5f);
	Clipper.Setup(Frame.Projection * Frame.WorldToView * Frame.ObjectToWorld, portalSpans, portalPlane);

	ViewLocation = vec4(location, 1.0f);
	PortalPlane = portalPlane;
	ViewZone = viewZone >= 0 ? viewZone : FindZoneAt(location);
	//ViewZoneMask = ViewZone ? 1ULL << ViewZone : -1;
	ViewRotation = viewRotation;

	// The frame's own zone counts as seen (the original's OccludeBsp stamps
	// it first); the zones behind visible portals are stamped as they pass
	// the clipper (ProcessNodeSurface).
	auto& zones = engine->Level->Model->Zones;
	if ((size_t)ViewZone < zones.size())
		zones[ViewZone].LastRenderTime = engine->LevelInfo->TimeSeconds();
	ZoneSeenFrame.resize(zones.size(), -1);
	MarkZoneSeen(ViewZone);

	OpaqueNodes.clear();
	Actors.clear();
	for (Array<VisibleTranslucent>& list : Translucents)
		list.clear();
	Coronas.clear();
	Portals.clear();

	// The occlusion proxies are built in view space and set back in the
	// world: the view's inverse, a rotation (mirrored in a mirror) and a move.
	Now = engine->LevelInfo->TimeSeconds();
	Fragments.clear();
	FragmentStack.clear();
	ViewToWorld = mat3::inverse(mat3(Frame.WorldToView));
	ViewTranslation = vec3(Frame.WorldToView[12], Frame.WorldToView[13], Frame.WorldToView[14]);

	// Before the BSP walk: the clipper's spans fill as surfaces draw, so a
	// test after it would cull everything; here it clips items to the view
	// (and a portal's spans) only. Their proxies go down the whole walk.
	ProcessRenderIterators();
	ProcessMoverMirrors();

	ProcessNode(&engine->Level->Model->Nodes[0], 0, FragmentStack.size(), engine->Level->Model->RootOutside != 0);
}

// A mover's mirror, which the original files with the mover's other polygons
// into the level's BSP, to open a frame as any mirror there (dx-reverse-info
// render-dll.md, a frame). A mover is no part of the walk here, so before it
// each mirrored face of a shown mover becomes a mirror's portal, its spans
// all of the face the frame shows: the depth test leaves out what lies in
// front of it. Each face a frame of its own, three frames deep at most; one
// seen from behind passed over, and a translucent one reflecting only while
// the render device's ShinySurfaces is on, as the BSP's mirrors.
void VisibleFrame::ProcessMoverMirrors()
{
	if (PortalDepth >= 3)
		return;

	// Only the level's movers with a mirrored face (RenderSubsystem::MirrorMovers)
	for (UMover* mover : engine->render->MirrorMovers)
	{
		if (mover->bDeleteMe() || mover->bHidden() || mover->DrawType() != DT_Brush || !mover->Brush() || !mover->Brush()->Polys)
			continue;

		mat4 toWorld = VisibleBrush::MoverToWorld(mover);
		for (const Poly& poly : mover->Brush()->Polys->Polys)
		{
			uint32_t flags = poly.PolyFlags;
			if (!(flags & PF_Mirrored) || (flags & PF_Invisible) || poly.Vertices.size() < 3 || poly.Vertices.size() > 32)
				continue;
			if ((flags & PF_Translucent) && !engine->renderdev->ShinySurfaces)
				continue;

			vec3 points[32];
			for (size_t i = 0; i < poly.Vertices.size(); i++)
				points[i] = (toWorld * vec4(poly.Vertices[i], 1.0f)).xyz();
			vec3 normal = (toWorld * vec4(poly.Normal, 0.0f)).xyz();
			float len = length(normal);
			if (len <= 0.0f)
				continue;
			normal = normal / len;
			vec4 plane(normal, -dot(normal, points[0]));
			if (!(flags & PF_TwoSided) && dot(ViewLocation, plane) < 0.0f)
				continue;

			Array<PortalSpan> spans = Clipper.ProjectPortal(points, (uint32_t)poly.Vertices.size());
			if (spans.empty())
				continue;

			VisiblePortal portal;
			portal.Mover = mover;
			portal.MoverPoly = &poly;
			portal.MirrorPoint = points[0];
			portal.MirrorNormal = normal;
			portal.Spans = std::move(spans);
			Portals.push_back(std::move(portal));
		}
	}
}

bool VisibleFrame::HasMoverMirror(const UMover* mover, const Poly* poly) const
{
	for (const VisiblePortal& portal : Portals)
	{
		if (portal.Mover == mover && portal.MoverPoly == poly)
			return true;
	}
	return false;
}

// A mesh's render box as the original's UMesh::GetRenderBoundingBox makes it
// (dx-reverse-info/render-dll.md, which actors are drawn): the boxes of the
// animation's frame and the next, or the whole mesh's while it is not
// animating, scaled -- by 1.5 for particles --, grown by a unit and turned
// and placed as the actor.
static BBox MeshRenderBox(UActor* actor, UMesh* mesh)
{
	BBox local = mesh->BoundingBox;
	const MeshAnimSeq* seq = nullptr;
	for (const MeshAnimSeq& s : mesh->AnimSeqs)
	{
		if (s.Name == actor->AnimSequence())
		{
			seq = &s;
			break;
		}
	}
	if (seq && seq->NumFrames > 0 && actor->AnimFrame() >= 0.0f)
	{
		int frame = (int)std::floor((actor->AnimFrame() + 1.0f) * seq->NumFrames);
		size_t frame1 = seq->StartFrame + frame % seq->NumFrames;
		size_t frame2 = seq->StartFrame + (frame + 1) % seq->NumFrames;
		if (frame1 < mesh->BoundingBoxes.size() && frame2 < mesh->BoundingBoxes.size())
		{
			const BBox& a = mesh->BoundingBoxes[frame1];
			const BBox& b = mesh->BoundingBoxes[frame2];
			local = BBox(vec3(std::min(a.min.x, b.min.x), std::min(a.min.y, b.min.y), std::min(a.min.z, b.min.z)),
				vec3(std::max(a.max.x, b.max.x), std::max(a.max.y, b.max.y), std::max(a.max.z, b.max.z)));
		}
	}

	vec3 scale = mesh->Scale * (actor->bParticles() ? 1.5f : actor->DrawScale());
	vec3 low = (local.min - mesh->Origin) * scale - vec3(1.0f);
	vec3 high = (local.max - mesh->Origin) * scale + vec3(1.0f);
	mat4 toWorld = mat4::translate(actor->Location() + actor->PrePivot()) * Coords::Rotation(actor->Rotation()).ToMatrix() * Coords::Rotation(mesh->RotOrigin).ToMatrix();
	BBox box;
	for (int i = 0; i < 8; i++)
	{
		vec3 corner((i & 1) ? high.x : low.x, (i & 2) ? high.y : low.y, (i & 4) ? high.z : low.z);
		vec3 p = (toWorld * vec4(corner, 1.0f)).xyz();
		if (i == 0)
		{
			box = BBox(p, p);
		}
		else
		{
			box.min = vec3(std::min(box.min.x, p.x), std::min(box.min.y, p.y), std::min(box.min.z, p.z));
			box.max = vec3(std::max(box.max.x, p.x), std::max(box.max.y, p.y), std::max(box.max.z, p.z));
		}
	}
	return box;
}

// A box's screen rectangle as the original's URender::BoundVisible finds it
// (x across and y up, over the depth): from its corners, running to the
// frame's edge on a side some corner is beyond, the whole frame with the
// viewer inside it; false when it is all behind the viewer or all beyond
// one side.
bool VisibleFrame::BoundRectangle(const BBox& box, float& minX, float& minY, float& maxX, float& maxY)
{
	vec3 eye = ViewLocation.xyz();
	if (eye.x >= box.min.x && eye.x <= box.max.x && eye.y >= box.min.y && eye.y <= box.max.y && eye.z >= box.min.z && eye.z <= box.max.z)
	{
		minX = -FrustumTanX;
		maxX = FrustumTanX;
		minY = -FrustumTanY;
		maxY = FrustumTanY;
		return true;
	}

	int allOutside = 0xf, anyOutside = 0;
	bool anyInFront = false, anyProjected = false;
	for (int i = 0; i < 8; i++)
	{
		vec3 corner((i & 1) ? box.max.x : box.min.x, (i & 2) ? box.max.y : box.min.y, (i & 4) ? box.max.z : box.min.z);
		vec4 v = Frame.WorldToView * vec4(corner, 1.0f);
		if (v.z >= 0.0f)
			anyInFront = true;
		int outside = 0;
		if (v.z * FrustumTanX + v.x < 0.0f) outside |= 1;
		if (v.z * FrustumTanX - v.x < 0.0f) outside |= 2;
		if (v.z * FrustumTanY + v.y < 0.0f) outside |= 4;
		if (v.z * FrustumTanY - v.y < 0.0f) outside |= 8;
		allOutside &= outside;
		anyOutside |= outside;
		if (v.z != 0.0f)
		{
			float x = v.x / v.z, y = v.y / v.z;
			if (!anyProjected)
			{
				minX = maxX = x;
				minY = maxY = y;
				anyProjected = true;
			}
			else
			{
				minX = std::min(minX, x);
				maxX = std::max(maxX, x);
				minY = std::min(minY, y);
				maxY = std::max(maxY, y);
			}
		}
	}
	if (!anyInFront || allOutside || !anyProjected)
		return false;
	if (anyOutside & 1) minX = -FrustumTanX;
	if (anyOutside & 2) maxX = FrustumTanX;
	if (anyOutside & 4) minY = -FrustumTanY;
	if (anyOutside & 8) maxY = FrustumTanY;
	minX = std::max(minX, -FrustumTanX);
	maxX = std::min(maxX, FrustumTanX);
	minY = std::max(minY, -FrustumTanY);
	maxY = std::min(maxY, FrustumTanY);
	return minX < maxX && minY < maxY;
}

bool VisibleFrame::SpriteRectangle(UActor* actor, float& minX, float& minY, float& maxX, float& maxY, float& z)
{
	// Only a mesh or a sprite has one: at the depth of the actor's location,
	// none when that is behind the viewer.
	vec4 location = Frame.WorldToView * vec4(actor->Location(), 1.0f);
	z = location.z;
	if (z < 0.0f)
		return false;

	EDrawType dt = (EDrawType)actor->DrawType();
	if (dt == DT_Mesh && actor->Mesh())
		return BoundRectangle(MeshRenderBox(actor, actor->Mesh()), minX, minY, maxX, maxY);

	if ((dt == DT_Sprite || dt == DT_SpriteAnimOnce) && actor->Texture())
	{
		// The texture's size at the draw scale, around where the location
		// lands; none nearer than a unit.
		if (z <= 1.0f)
			return false;
		float halfWidth = actor->Texture()->USize() * actor->DrawScale() * 0.5f;
		float halfHeight = actor->Texture()->VSize() * actor->DrawScale() * 0.5f;
		minX = std::max((location.x - halfWidth) / z, -FrustumTanX);
		maxX = std::min((location.x + halfWidth) / z, FrustumTanX);
		minY = std::max((location.y - halfHeight) / z, -FrustumTanY);
		maxY = std::min((location.y + halfHeight) / z, FrustumTanY);
		return minX < maxX && minY < maxY;
	}

	return false;
}

vec3 VisibleFrame::TileOffset(const vec3& location) const
{
	if (!Frame.PolygonsAtFX15 || Frame.X <= 0 || Frame.Y <= 0)
		return vec3(0.0f);

	// The device's half pixel (PolygonShift) taken back at the location's
	// depth, in the view's plane: half a pixel there is the frustum's
	// tangent over the frame's size, times the depth.
	float z = (Frame.WorldToView * vec4(location, 1.0f)).z;
	return ViewToWorld * vec3(-z * FrustumTanX / Frame.X, -z * FrustumTanY / Frame.Y, 0.0f);
}

void VisibleFrame::AddOcclusionProxy(UActor* actor)
{
	if (actor->LastRenderTime() == Now)
		return;

	// Only a mesh or a sprite gets one, as the original's sprites.
	float minX, minY, maxX, maxY, z;
	if (!SpriteRectangle(actor, minX, minY, maxX, maxY, z))
		return;

	// The rectangle set back at that depth -- no nearer than the clipper's
	// near plane, which would cut it away.
	z = std::max(z, 1.5f);
	auto toWorld = [&](float x, float y) { return ViewToWorld * (vec3(x * z, y * z, z) - ViewTranslation); };
	OcclusionFragment fragment;
	fragment.Actor = actor;
	fragment.Sample = actor->DrawType() == DT_Mesh ? 1.0f : 0.0f;
	fragment.NumVerts = 4;
	fragment.Verts[0] = toWorld(minX, minY);
	fragment.Verts[1] = toWorld(maxX, minY);
	fragment.Verts[2] = toWorld(maxX, maxY);
	fragment.Verts[3] = toWorld(minX, maxY);
	FragmentStack.push_back((int)Fragments.size());
	Fragments.push_back(fragment);
}

void VisibleFrame::SplitFragments(size_t first, size_t count, const vec4& plane, bool viewerInFront)
{
	// Each piece goes to the side of the plane it lies on -- the viewer's
	// side into NearFragments, the far side onto the stack -- and one across
	// it is cut in two. Pieces of an actor already drawn are dropped.
	const float epsilon = 0.01f;
	for (size_t i = 0; i < count; i++)
	{
		// Read before the pool grows: the pieces go into it last
		int index = FragmentStack[first + i];
		const OcclusionFragment& fragment = Fragments[index];
		if (fragment.Actor->LastRenderTime() == Now)
			continue;

		float dist[OcclusionFragment::MaxVerts];
		int nearVerts = 0, farVerts = 0;
		for (int v = 0; v < fragment.NumVerts; v++)
		{
			float d = dot(vec4(fragment.Verts[v], 1.0f), plane);
			dist[v] = viewerInFront ? d : -d;
			if (dist[v] > epsilon)
				nearVerts++;
			else if (dist[v] < -epsilon)
				farVerts++;
		}
		if (farVerts == 0)
		{
			NearFragments.push_back(index);
			continue;
		}
		if (nearVerts == 0)
		{
			FragmentStack.push_back(index);
			continue;
		}

		OcclusionFragment nearPiece, farPiece;
		nearPiece.Actor = farPiece.Actor = fragment.Actor;
		nearPiece.Sample = farPiece.Sample = fragment.Sample;
		bool overflow = false;
		auto add = [&](OcclusionFragment& piece, const vec3& v)
		{
			if (piece.NumVerts < OcclusionFragment::MaxVerts)
				piece.Verts[piece.NumVerts++] = v;
			else
				overflow = true;
		};
		for (int v = 0; v < fragment.NumVerts; v++)
		{
			int next = (v + 1) % fragment.NumVerts;
			const vec3& a = fragment.Verts[v];
			const vec3& b = fragment.Verts[next];
			if (dist[v] >= 0.0f)
				add(nearPiece, a);
			if (dist[v] <= 0.0f)
				add(farPiece, a);
			if ((dist[v] > 0.0f && dist[next] < 0.0f) || (dist[v] < 0.0f && dist[next] > 0.0f))
			{
				vec3 cut = a + (b - a) * (dist[v] / (dist[v] - dist[next]));
				add(nearPiece, cut);
				add(farPiece, cut);
			}
		}

		if (overflow)
		{
			// Too many corners: the whole piece on both sides, tested early
			NearFragments.push_back(index);
			FragmentStack.push_back(index);
			continue;
		}
		NearFragments.push_back((int)Fragments.size());
		Fragments.push_back(nearPiece);
		FragmentStack.push_back((int)Fragments.size());
		Fragments.push_back(farPiece);
	}
}

void VisibleFrame::TestFragment(int index)
{
	OcclusionFragment& fragment = Fragments[index];
	if (fragment.Actor->LastRenderTime() == Now)
		return;

	// Its rectangle's pixels as the original rounds them, a sprite's up, a
	// mesh's down (BspClipper::SetPixelSample); then back to a surface's
	Clipper.SetPixelSample(fragment.Sample);
	bool visible = Clipper.IsPolygonVisible(fragment.Verts, fragment.NumVerts);
	Clipper.SetPixelSample(0.5f);
	if (visible)
		fragment.Actor->LastRenderTime() = Now;
}

void VisibleFrame::TestLeafFragments(size_t first, size_t count, bool outside, int zone)
{
	// Pieces that come to a leaf inside solid are not drawn, nor are those
	// in a zone no visible portal has led to yet: the original tests each
	// piece against its zone's own span buffer, which such a zone lacks.
	if (!outside || zone < 0 || (size_t)zone >= ZoneSeenFrame.size() || ZoneSeenFrame[zone] != FrameCounter)
		return;
	for (size_t i = 0; i < count; i++)
		TestFragment(FragmentStack[first + i]);
}

void VisibleFrame::MarkZoneSeen(int zone)
{
	if (zone >= 0 && (size_t)zone < ZoneSeenFrame.size())
		ZoneSeenFrame[zone] = FrameCounter;
}

void VisibleFrame::ProcessRenderIterators()
{
	// An actor with a RenderIteratorClass is drawn as the items its
	// iterator lists, and gets no sprite of its own. Each scene frame: Init
	// with the viewer, First, then until IsDone an item for the actor
	// CurrentItem gives, and Next. The iterators move one proxy actor from
	// item to item, so each item keeps what the proxy was as it was listed
	// (dx-reverse-info/render-dll.md, render iterators).
	UPlayerPawn* viewer = UObject::TryCast<UPlayerPawn>(engine->viewport->Actor());
	for (UActor* actor : engine->render->IteratorActors)
	{
		if (actor->bDeleteMe())
			continue;
		URenderIterator* iterator = actor->RenderInterface();
		if (!iterator)
			continue;

		iterator->Init(viewer);
		iterator->First();
		while (!iterator->IsDone())
		{
			UActor* item = iterator->CurrentItem();
			iterator->Next();
			if (!item)
				continue;

			EDrawType dt = (EDrawType)item->DrawType();
			float extent;
			if (dt == DT_Mesh && item->Mesh())
			{
				// A proxy is a plain effect with no collision, so size the
				// box from the mesh itself
				const BBox& meshBox = item->Mesh()->BoundingBox;
				extent = std::max(length(meshBox.extents()) * item->DrawScale(), item->CollisionRadius() + item->CollisionHeight());
			}
			else if ((dt == DT_Sprite || dt == DT_SpriteAnimOnce) && item->Texture())
			{
				UTexture* texture = item->Texture();
				extent = std::max(texture->USize(), texture->VSize()) * item->DrawScale() * 0.5f;
			}
			else
			{
				continue;
			}

			vec3 location = item->Location();
			BBox box(location - vec3(extent), location + vec3(extent));
			if (!Clipper.IsAABBVisible(box))
				continue;

			// The proxy counts as drawn if some item survives the world in
			// front of it
			AddOcclusionProxy(item);

			VisibleIteratorItem visitem;
			visitem.Actor = item;
			visitem.Type = dt;
			visitem.Location = location;
			visitem.Rotation = item->Rotation();
			visitem.DrawScale = item->DrawScale();
			visitem.ScaleGlow = item->ScaleGlow();

			vec3 v = location - ViewLocation.xyz();
			AddTranslucent(ActorTranslucentList(item), VisibleTranslucent(visitem, dot(v, v)));
		}
	}
}

void VisibleFrame::SetupSceneFrame(const mat4& worldToView)
{
	Frame.XB = engine->viewport->ViewportX();
	Frame.YB = engine->viewport->ViewportY();
	Frame.X = engine->viewport->ViewportWidth();
	Frame.Y = engine->viewport->ViewportHeight();
	Frame.FX = (float)engine->viewport->ViewportWidth();
	Frame.FY = (float)engine->viewport->ViewportHeight();

	if (engine->dxRootWindow && engine->dxRootWindow->RenderViewportSet)
	{
		float virtscale = engine->dxRootWindow->GetVirtualScale();
		float x = engine->dxRootWindow->renderX();
		float y = engine->dxRootWindow->renderY();
		float w = engine->dxRootWindow->renderWidth();
		float h = engine->dxRootWindow->renderHeight();

		// Center, scale and possibly extend viewport if it covers the entire root window
		if (x <= 0.0f && w >= engine->dxRootWindow->Width())
		{
			x = 0.0f;
			w = (float)engine->viewport->ViewportWidth();
		}
		else
		{
			x = (engine->dxRootWindow->UsedX + x) * virtscale;
			w *= virtscale;
		}
		if (y <= 0.0f && h >= engine->dxRootWindow->Height())
		{
			y = 0.0f;
			h = (float)engine->viewport->ViewportHeight();
		}
		else
		{
			y = (engine->dxRootWindow->UsedY + y) * virtscale;
			h *= virtscale;
		}

		// A pixel at the least and 16,384 at the most (DrawViewport passes
		// over a window of no size): a frame of no width had no aspect, and
		// one past an int's range no size at all
		w = clamp(w, 1.0f, 16384.0f);
		h = clamp(h, 1.0f, 16384.0f);

		Frame.X = (int)w;
		Frame.Y = (int)h;
		Frame.XB = (int)std::round(x);
		Frame.YB = (int)std::round(y);
		Frame.FX = w;
		Frame.FY = h;
	}

	Frame.FX2 = Frame.FX * 0.5f;
	Frame.FY2 = Frame.FY * 0.5f;
	Frame.PolygonsAtFX15 = true;
	Frame.ObjectToWorld = mat4::identity();
	Frame.WorldToView = worldToView;
	Frame.FovAngle = HorPlusFovAngle(engine->CameraFovAngle, Frame.FX, Frame.FY);
	float Aspect = Frame.FY / Frame.FX;
	float RProjZ = (float)std::tan(radians(Frame.FovAngle) * 0.5f);
	float RFX2 = 2.0f * RProjZ / Frame.FX;
	float RFY2 = 2.0f * RProjZ * Aspect / Frame.FY;
	Frame.Projection = mat4::frustum(-RProjZ, RProjZ, -Aspect * RProjZ, Aspect * RProjZ, 1.0f, 32768.0f, handedness::left, clipzrange::zero_positive_w);
	FrustumTanX = RProjZ;
	FrustumTanY = Aspect * RProjZ;
}

void VisibleFrame::ProcessNode(BspNode* node, size_t fragFirst, size_t fragCount, bool outside)
{
	// Skip node if it is not part of the portal zones we have seen so far
	//if ((node->ZoneMask & ViewZoneMask) == 0)
	//	return;

	// The occlusion proxies' pieces that came down into this node's space
	// are FragmentStack[fragFirst, +fragCount); those of the actors filed
	// here follow from base. All the walk has drawn so far lies in front
	// of them. Outside is whether this node's space is empty, not solid.
	size_t base = FragmentStack.size();

	// Skip node if its AABB is not visible -- and what came down into its
	// space with it, as the original drops it
	if (node->RenderBound != -1 && !Clipper.IsAABBVisible(engine->Level->Model->Bounds[node->RenderBound]))
	{
		return;
	}

	// Add bsp node actors to the visible set
	for (UActor* actor = node->ActorList; actor != nullptr; actor = actor->BspInfo.Next)
	{
		if (actor->LastDrawFrame != FrameCounter)
		{
			actor->LastDrawFrame = FrameCounter;

			VisibleActor visactor;
			visactor.Process(this, actor);
		}
	}
	size_t end = FragmentStack.size();

	// Decide which side the plane the camera is
	vec4 plane = { node->PlaneX, node->PlaneY, node->PlaneZ, -node->PlaneW };
	bool swapFrontAndBack = dot(ViewLocation, plane) < 0.0f;
	int back = node->Back;
	int front = node->Front;
	if (swapFrontAndBack)
		std::swap(front, back);

	// Whether each side's space is empty, as the original's ChildOutside
	// works it out: a CSG node's front is, its back is not.
	bool csg = node->NumVertices > 0 && (node->NodeFlags & (NF_NotCsg | 0x20)) == 0;
	bool nearOutside = swapFrontAndBack ? (outside && !csg) : (outside || csg);
	bool farOutside = swapFrontAndBack ? (outside || csg) : (outside && !csg);
	int nearZone = swapFrontAndBack ? node->Zone0 : node->Zone1;
	int farZone = swapFrontAndBack ? node->Zone1 : node->Zone0;

	// The pieces beyond the plane stay on the stack under those on the
	// viewer's side, which the walk takes first. A piece that comes to a
	// leaf is tested there: before this plane's surfaces toward the viewer,
	// after them away from it.
	size_t farFirst = FragmentStack.size();
	NearFragments.clear();
	SplitFragments(fragFirst, fragCount, plane, !swapFrontAndBack);
	SplitFragments(base, end - base, plane, !swapFrontAndBack);
	size_t farCount = FragmentStack.size() - farFirst;
	size_t nearFirst = FragmentStack.size();
	for (int index : NearFragments)
		FragmentStack.push_back(index);
	size_t nearCount = FragmentStack.size() - nearFirst;

	// Recursively divide front space (toward the viewer)
	if (front >= 0)
	{
		ProcessNode(&engine->Level->Model->Nodes[front], nearFirst, nearCount, nearOutside);
	}
	else
	{
		TestLeafFragments(nearFirst, nearCount, nearOutside, nearZone);
	}
	FragmentStack.resize(nearFirst);

	// Draw surfaces on this plane
	BspNode* polynode = node;
	while (true)
	{
		// Each coplanar node by its own plane, which may face the other way:
		// the side the viewer is on picks its zone (the original's
		// OccludeBsp, Render.dll 0x10b173d0).
		vec4 polyPlane = { polynode->PlaneX, polynode->PlaneY, polynode->PlaneZ, -polynode->PlaneW };
		bool polyBehind = dot(ViewLocation, polyPlane) < 0.0f;
		ProcessNodeSurface(polynode, polyBehind);

		if (polynode->Plane < 0) break;
		polynode = &engine->Level->Model->Nodes[polynode->Plane];
	}

	// Possibly divide back space (away from the viewer)
	if (back >= 0)
	{
		ProcessNode(&engine->Level->Model->Nodes[back], farFirst, farCount, farOutside);
	}
	else
	{
		TestLeafFragments(farFirst, farCount, farOutside, farZone);
	}
	FragmentStack.resize(base);
}

void VisibleFrame::ProcessNodeSurface(BspNode* node, bool front)
{
	if (node->NumVertices < 3 || node->Surf < 0)
		return;

	UModel* model = engine->Level->Model;
	const BspSurface& surface = model->Surfaces[node->Surf];

	// The surface's points, gathered only when a test needs them: over half
	// the surfaces in view are one-sided back faces, skipped below without.
	int numverts = node->NumVertices;
	vec3* points = nullptr;
	auto gatherPoints = [&]()
	{
		points = engine->render->GetTempVertexBuffer(numverts);
		BspVert* v = &model->Vertices[node->VertPool];
		if (MirrorFlag)
		{
			for (int j = 0; j < numverts; j++)
			{
				points[numverts - 1 - j] = model->Points[v[j].Vertex];
			}
		}
		else
		{
			for (int j = 0; j < numverts; j++)
			{
				points[j] = model->Points[v[j].Vertex];
			}
		}
	};

	uint32_t PolyFlags = surface.PolyFlags;
	UTexture* texture = surface.Material;
	if (!texture)
		texture = engine->LevelInfo->DefaultTexture();

	if (surface.Material)
		PolyFlags |= surface.Material->PolyFlags();

	VisibleNode info;
	info.Node = node;
	info.ViewerBehind = front;
	info.PolyFlags = PolyFlags;

	// A sky's and a warp zone's frames open three deep at most, as a
	// mirror's, as the original's OccludeBsp opens them (Render.dll
	// 0x10b173d0, Recursion < 3); deeper, the surface is drawn as it is
	if (PortalDepth < 3)
	{
		if ((PolyFlags & (PF_FakeBackdrop | PF_Invisible)) == PF_FakeBackdrop)
		{
			int zone = front ? node->Zone1 : node->Zone0;
			UZoneInfo* zoneInfo = engine->GetZoneActor(zone);
			if (zoneInfo->SkyZone())
			{
				gatherPoints();
				Array<PortalSpan> spans = Clipper.CheckPortal(points, numverts);
				if (!spans.empty())
				{
					USkyZoneInfo* skyZone = zoneInfo->SkyZone();
					for (auto& p : Portals)
					{
						if (p.SkyZone == skyZone)
						{
							p.Nodes.push_back(info);
							p.Spans.insert(p.Spans.end(), spans.begin(), spans.end());
							return;
						}
					}
					VisiblePortal portal;
					portal.Nodes.push_back(info);
					portal.SkyZone = skyZone;
					portal.Spans = std::move(spans);
					Portals.push_back(std::move(portal));
				}
				return;
			}
		}
		else if (PolyFlags & PF_Portal)
		{
			int portalZone = front ? node->Zone1 : node->Zone0;
			if (portalZone > 0)
			{
				UWarpZoneInfo* warpZone = UObject::TryCast<UWarpZoneInfo>(engine->GetZoneActor(portalZone));
				// The other side generated first when the zone has none, and
				// with none still (a cross-level warp's; an OtherSideURL that
				// names no zone of the level) no frame, the surface drawn as it
				// is: the original's OccludeBsp (Render.dll 0x10b173d0) calls
				// the zone's Generate and opens the frame only with the other
				// side set. The transforms dereferenced a missing other side.
				if (warpZone && !warpZone->OtherSideActor())
					CallEvent(warpZone, "Generate", {});
				if (warpZone && warpZone->OtherSideActor())
				{
					gatherPoints();
					Array<PortalSpan> spans = Clipper.CheckPortal(points, numverts);
					if (!spans.empty())
					{
						for (auto& p : Portals)
						{
							if (p.WarpZone == warpZone)
							{
								p.Nodes.push_back(info);
								p.Spans.insert(p.Spans.end(), spans.begin(), spans.end());
								return;
							}
						}
						VisiblePortal portal;
						portal.Nodes.push_back(info);
						portal.WarpZone = warpZone;
						portal.Spans = std::move(spans);
						Portals.push_back(std::move(portal));
					}
					return;
				}
			}
		}
		// A mirror opens a frame three frames deep at most, a sky's and a warp
		// zone's counted, as the original's (Render.dll OccludeBsp
		// 0x10b173d0, Recursion < 3); deeper, it is drawn as any surface
		else if ((PolyFlags & PF_Mirrored) && PortalDepth < 3)
		{
			// Seen from behind, a one-sided mirror is passed over, as the
			// original passes over any surface seen from behind.
			vec4 plane = { node->PlaneX, node->PlaneY, node->PlaneZ, -node->PlaneW };
			if (!(PolyFlags & PF_TwoSided) && dot(ViewLocation, plane) < 0.0f)
				return;

			// A translucent mirror is a shiny surface: with the render
			// device's ShinySurfaces off, the original draws it solid and
			// reflects nothing.
			if ((PolyFlags & PF_Translucent) && !engine->renderdev->ShinySurfaces)
			{
				PolyFlags &= ~PF_Translucent;
				info.PolyFlags = PolyFlags;
			}
			else
			{
				gatherPoints();
				Array<PortalSpan> spans = Clipper.CheckPortal(points, numverts);
				if (!spans.empty())
				{
					// One frame for the mirrors of one plane seen from one
					// zone, as the original's child frames merge
					// (Render.dll CreateChildFrame 0x10b15a40)
					int zone = front ? node->Zone0 : node->Zone1;
					for (auto& p : Portals)
					{
						if (p.WarpZone || p.SkyZone || p.Mover)
							continue;
						const BspNode* other = p.Nodes.front().Node;
						int otherZone = p.Nodes.front().ViewerBehind ? other->Zone0 : other->Zone1;
						if (otherZone == zone && std::abs(other->PlaneX - node->PlaneX) < 0.001f && std::abs(other->PlaneY - node->PlaneY) < 0.001f &&
							std::abs(other->PlaneZ - node->PlaneZ) < 0.001f && std::abs(other->PlaneW - node->PlaneW) < 0.1f)
						{
							p.Nodes.push_back(info);
							p.Spans.insert(p.Spans.end(), spans.begin(), spans.end());
							return;
						}
					}

					VisiblePortal portal;
					portal.Nodes.push_back(info);
					portal.Spans = std::move(spans);
					Portals.push_back(std::move(portal));
				}
				return;
			}
		}
	}

	// A one-sided surface seen from behind is never drawn (UE1 did not either),
	// and in a closed level a front face drawn earlier already hides it, so it
	// hides nothing either: skip the clipper's test, over half of the surfaces
	// in view. A mirror's frame is seen from the reflected eye, its
	// ViewLocation; portals, skies and mirrors themselves returned above -- and
	// a zone portal, which leads into the zone beyond it from either side, as
	// the original takes it.
	if (!(PolyFlags & (PF_TwoSided | PF_Portal)))
	{
		vec4 plane = { node->PlaneX, node->PlaneY, node->PlaneZ, -node->PlaneW };
		if (dot(ViewLocation, plane) < 0.0f)
			return;
	}

	gatherPoints();

	// The decals that show on this surface, tested before it fills the spans:
	// the original stamps a decal only when some of it is left once clipped to
	// what is drawn of its surface (Render.dll DrawFrame 0x10b1a2d0, ClipDecal
	// 0x10b1bda0), not every decal on a surface drawn.
	Array<UDecal*> shownDecals;
	if (engine->client->Decals)
	{
		for (auto& leveldecal : node->Decals)
		{
			if (leveldecal.Decal->Texture() && Clipper.IsPolygonVisible(leveldecal.Positions.data(), (uint32_t)leveldecal.Positions.size()))
				shownDecals.push_back(leveldecal.Decal);
		}
	}

	if (!Clipper.CheckSurface(points, numverts, (PolyFlags & PF_NoOcclude) == 0))
		return;

	// A visible portal surface means the zones it borders were seen: their
	// render time is what stasis and the AI events read for actors there.
	if (PolyFlags & PF_Portal)
	{
		auto& zones = engine->Level->Model->Zones;
		float now = engine->LevelInfo->TimeSeconds();
		if ((size_t)node->Zone0 < zones.size())
			zones[node->Zone0].LastRenderTime = now;
		if ((size_t)node->Zone1 < zones.size())
			zones[node->Zone1].LastRenderTime = now;
		MarkZoneSeen(node->Zone0);
		MarkZoneSeen(node->Zone1);
	}

	if (PolyFlags & PF_Invisible)
		return;

	// A drawn decal stamps its own LastRenderedTime; LastRendered() reads
	// LastRenderTime, which a decal never gets, so a decal never counts as drawn.
	for (UDecal* decal : shownDecals)
		decal->LastRenderedTime() = engine->LevelInfo->TimeSeconds();

	if ((PolyFlags & (PF_Translucent | PF_Modulated)) == 0 && (PolyFlags & PF_Occlude) != PF_Occlude)
	{
		OpaqueNodes.push_back(info);
	}
	else
	{
		UModel* model = engine->Level->Model;
		const BspSurface& surface = model->Surfaces[node->Surf];
		vec3 v = model->Points[surface.pBase] - ViewLocation.xyz();
		AddTranslucent(TranslucentSurfaces, VisibleTranslucent(info, dot(v, v)));
	}
}

void VisibleFrame::AddTranslucent(TranslucentList list, const VisibleTranslucent& item)
{
	Translucents[DeusExOrder ? list : TranslucentSurfaces].push_back(item);
}

void VisibleFrame::SortTranslucent()
{
	// By the distance of one point of each: back to front (DeusExOrder), as
	// the original's lists run in the order its walk met them, each new one
	// at the head; nearest first without it. An item at no place (a NaN
	// location, a script's 0/0) sorts as nearest: a NaN key orders nothing,
	// and the sort then has no order to keep.
	auto key = [](const VisibleTranslucent& t) { return t.DistSqr == t.DistSqr ? t.DistSqr : 0.0f; };
	for (Array<VisibleTranslucent>& list : Translucents)
	{
		if (DeusExOrder)
			std::sort(list.begin(), list.end(), [&](const VisibleTranslucent& a, const VisibleTranslucent& b) { return key(a) > key(b); });
		else
			std::sort(list.begin(), list.end(), [&](const VisibleTranslucent& a, const VisibleTranslucent& b) { return key(a) < key(b); });
	}
}

int VisibleFrame::FindZoneAt(const vec3& location)
{
	return FindZoneAt(vec4(location, 1.0f), &engine->Level->Model->Nodes.front(), engine->Level->Model->Nodes.data());
}

int VisibleFrame::FindZoneAt(const vec4& location, BspNode* node, BspNode* nodes)
{
	while (true)
	{
		vec4 plane = { node->PlaneX, node->PlaneY, node->PlaneZ, -node->PlaneW };
		bool swapFrontAndBack = dot(location, plane) < 0.0f;
		int front = node->Front;
		int back = node->Back;
		if (swapFrontAndBack)
			std::swap(front, back);

		if (front >= 0)
		{
			node = nodes + front;
		}
		else
		{
			return swapFrontAndBack ? node->Zone0 : node->Zone1;
		}
	}
}

void VisibleFrame::Draw()
{
	DrawPortals();
	DrawOpaqueNodes();
	DrawOpaqueActors();
	SortTranslucent();
	DrawTranslucent();
}

void VisibleFrame::DrawCoronas()
{
	// Tiles, about the frame's centre: the original projects a corona's
	// place about FX2 (Render.dll DrawFrame)
	SceneNode frame2d = Frame;
	frame2d.ObjectToWorld = mat4::identity();
	frame2d.WorldToView = mat4::identity();
	frame2d.PolygonsAtFX15 = false;
	Device->SetSceneNode(&frame2d);

	// Deus Ex coronas are kept and faded across frames, from the viewer's
	// leaf, not gathered from the drawn parts of the level
	engine->render->DrawCoronasDX(this);
}

void VisibleFrame::DrawOpaqueNodes()
{
	// By zone, then texture, as the original's draw list sorts its solid
	// surfaces (dx-reverse-info render-dll.md, a frame), then by light map:
	// a run of one texture and one page of light maps is one batch
	// (LightmapAtlas). The depth test makes their order free.
	UModel* model = engine->Level->Model;
	auto key = [&](const VisibleNode& node)
	{
		const BspSurface& surface = model->Surfaces[node.Node->Surf];
		int zone = node.ViewerBehind ? node.Node->Zone0 : node.Node->Zone1;
		return std::make_tuple(zone, (uintptr_t)surface.Material, surface.LightMap);
	};
	std::stable_sort(OpaqueNodes.begin(), OpaqueNodes.end(), [&](const VisibleNode& a, const VisibleNode& b) { return key(a) < key(b); });

	Device->SetSceneNode(&Frame);
	for (VisibleNode& node : OpaqueNodes)
		node.Draw(this);
}

void VisibleFrame::DrawOpaqueActors()
{
	Device->SetSceneNode(&Frame);
	for (VisibleActor& actor : Actors)
		actor.DrawOpaque(this);
}

void VisibleFrame::DrawTranslucent()
{
	Device->SetSceneNode(&Frame);
	for (Array<VisibleTranslucent>& list : Translucents)
	{
		for (VisibleTranslucent& translucent : list)
			translucent.Draw(this);
	}
}

// A portal's spans by row, and each row's by x0, as BspClipper::Setup reads
// them. They come a surface at a time, each surface's row by row: the rows
// are counted, each span put in its row's place, and each row's few sorted.
// A row's spans never overlap (each was drawn solid as it was found), so this
// is the order a sort by row and x0 gives, without its cost: a sky seen
// through many surfaces brings tens of thousands of spans a frame.
static void SortPortalSpans(Array<PortalSpan>& spans)
{
	if (spans.size() < 2)
		return;
	int64_t lastRow = 0;
	for (const PortalSpan& span : spans)
	{
		if (span.y < 0 || span.y >= BspClipper::MaxHeight)
		{
			std::sort(spans.begin(), spans.end(), [](const PortalSpan& a, const PortalSpan& b) { return a.y != b.y ? a.y < b.y : a.x0 < b.x0; });
			return;
		}
		lastRow = std::max(lastRow, span.y);
	}

	Array<uint32_t> rowStart((size_t)lastRow + 2, 0u);
	for (const PortalSpan& span : spans)
		rowStart[(size_t)span.y + 1]++;
	for (size_t row = 1; row < rowStart.size(); row++)
		rowStart[row] += rowStart[row - 1];

	Array<PortalSpan> sorted(spans.size());
	Array<uint32_t> next = rowStart;
	for (const PortalSpan& span : spans)
		sorted[next[(size_t)span.y]++] = span;

	for (size_t row = 0; row + 1 < rowStart.size(); row++)
	{
		PortalSpan* first = sorted.data() + rowStart[row];
		PortalSpan* last = sorted.data() + rowStart[row + 1];
		for (PortalSpan* i = first + 1; i < last; i++)
		{
			PortalSpan span = *i;
			PortalSpan* j = i;
			while (j > first && (j - 1)->x0 > span.x0)
			{
				*j = *(j - 1);
				j--;
			}
			*j = span;
		}
	}
	spans.swap(sorted);
}

void VisibleFrame::DrawPortals()
{
	for (VisiblePortal& portal : Portals)
	{
		// BspClipper requires the visible spans list to be sorted
		SortPortalSpans(portal.Spans);

		if (portal.SkyZone)
		{
			mat4 skyToView =
				Coords::ViewToRenderDev().ToMatrix() *
				ViewRotation.Inverse().ToMatrix() *
				Coords::Rotation(portal.SkyZone->Rotation()).Inverse().ToMatrix() *
				Coords::Location(portal.SkyZone->Location()).ToMatrix();

			VisibleFrame skyframe;
			skyframe.Process(portal.SkyZone->Location(), skyToView, ViewRotation * Coords::Rotation(portal.SkyZone->Rotation()).Inverse(), MirrorFlag, PortalDepth + 1, portal.Spans);
			Device->SetSceneNode(&skyframe.Frame);
			skyframe.Draw();
			Device->ClearZ();
		}
		else if (portal.WarpZone)
		{
			// Warp camera
			vec3 newLocation = WarpLocationToOtherSide(portal.WarpZone, ViewLocation.xyz());
			Coords rotation = WarpRotationToOtherSide(portal.WarpZone, ViewRotation);
			mat4 worldToView = Coords::ViewToRenderDev().ToMatrix() * rotation.Inverse().ToMatrix() * mat4::translate(-newLocation);

			// Find clipping plane for portal and warp it
			auto node = portal.Nodes.front().Node;
			vec4 plane = { node->PlaneX, node->PlaneY, node->PlaneZ, -node->PlaneW };
			bool isFrontfacing = dot(ViewLocation, plane) < 0.0f;
			vec3 p = WarpLocationToOtherSide(portal.WarpZone, plane.xyz() * -plane.w);
			vec3 n = WarpNormalToOtherSide(portal.WarpZone, vec3(node->PlaneX, node->PlaneY, node->PlaneZ));
			vec4 portalPlane(n, -dot(p,n));
			if (!isFrontfacing)
				portalPlane = -portalPlane;

			VisibleFrame portalframe;
			portalframe.Process(newLocation, worldToView, rotation, MirrorFlag, PortalDepth + 1, portal.Spans, portalPlane);
			Device->SetSceneNode(&portalframe.Frame);
			portalframe.Draw();
			Device->ClearZ();
		}
		else // Mirror
		{
			// The original's child frame (dx-reverse-info render-dll.md, a
			// frame): the view and its eye reflected in the mirror's plane, so
			// the BSP is walked front to back, and faces culled, from the
			// reflected eye; it starts in the zone on the viewer's side, its
			// spans the mirror's. What lies behind the mirror would show in
			// front of it: the mirror's plane, the viewer's side kept, clips it
			// from the clipper, and on a device with a clip distance from
			// the drawing.
			// A mover's: its face's plane, and the zone on the viewer's side
			vec3 v, n;
			if (portal.Mover)
			{
				v = portal.MirrorPoint;
				n = portal.MirrorNormal;
			}
			else
			{
				const VisibleNode& first = portal.Nodes.front();
				UModel* model = engine->Level->Model;
				const BspSurface& surface = model->Surfaces[first.Node->Surf];
				v = model->Points[surface.pBase];
				n = model->Vectors[surface.vNormal];
			}
			mat4 mirrorRotation = mat4::mirror(n);
			mat4 mirrorToView = Frame.WorldToView * mat4::translate(v) * mirrorRotation * mat4::translate(-v);

			vec4 plane(n, -dot(n, v));
			if (dot(plane, ViewLocation) < 0.0f)
				plane = -plane;
			vec3 eye = ViewLocation.xyz() - plane.xyz() * (2.0f * dot(plane, ViewLocation));
			int zone;
			if (portal.Mover)
			{
				zone = FindZoneAt(v + plane.xyz() * 2.0f);
			}
			else
			{
				const VisibleNode& first = portal.Nodes.front();
				zone = first.ViewerBehind ? first.Node->Zone0 : first.Node->Zone1;
			}

			VisibleFrame mirrorframe;
			mirrorframe.Process(eye, mirrorToView, ViewRotation * Coords::FromMatrix(mirrorRotation).Inverse(), !MirrorFlag, PortalDepth + 1, portal.Spans, plane, zone);

			// The clip distance is the plane's in the mirror frame's view
			// space, the parent's view of the reflected world, in which the
			// reflection flips the plane's sign.
			mat3 toView = mat3(Frame.WorldToView);
			vec3 translation(Frame.WorldToView[12], Frame.WorldToView[13], Frame.WorldToView[14]);
			vec3 viewNormal = mat3::transpose(mat3::inverse(toView)) * plane.xyz();
			mirrorframe.Frame.NearClip = -vec4(viewNormal, plane.w - dot(viewNormal, translation));

			Device->SetSceneNode(&mirrorframe.Frame);
			mirrorframe.Draw();
			Device->ClearZ();
		}
	}

	// Seal the portals
	Device->SetSceneNode(&Frame);
	for (VisiblePortal& portal : Portals)
	{
		if (portal.Mover)
			VisibleBrush().DrawMirrorSeal(this, portal.Mover, *portal.MoverPoly);

		for (const auto& info : portal.Nodes)
		{
			VisibleNode visnode(info);
			visnode.PolyFlags |= PF_Occlude | PF_Invisible;
			visnode.ViewerBehind = true;
			visnode.Draw(this);

			if (info.PolyFlags & PF_NoOcclude)
			{
				if ((info.PolyFlags & (PF_Translucent | PF_Modulated)) == 0)
				{
					OpaqueNodes.push_back(info);
				}
				else
				{
					UModel* model = engine->Level->Model;
					const BspSurface& surface = model->Surfaces[info.Node->Surf];
					vec3 v = model->Points[surface.pBase] - ViewLocation.xyz();
					AddTranslucent(TranslucentSurfaces, VisibleTranslucent(info, dot(v, v)));
				}
			}
		}
	}
}

vec3 VisibleFrame::WarpLocationToOtherSide(UWarpZoneInfo* warpZone, vec3 p)
{
	// Transform to warp space:
	{
		vec3 origin = warpZone->WarpCoords().Origin;
		mat3 rotate = warpZone->WarpCoords().ToMatrix();
		//mat3 invrotate = mat3::transpose(rotate);
		p = rotate * (p - origin);
	}

	// Transform from warp space:
	{
		vec3 origin = warpZone->OtherSideActor()->WarpCoords().Origin;
		mat3 rotate = warpZone->OtherSideActor()->WarpCoords().ToMatrix();
		mat3 invrotate = mat3::transpose(rotate);
		p = (invrotate * p) + origin;
	}

	return p;
}

// Turned as a location is, its origins aside: a rigid turn takes a normal
// by the same matrix as a direction, and the other order turned it back
// the other way, tilting the warped frame's clip plane
vec3 VisibleFrame::WarpNormalToOtherSide(UWarpZoneInfo* warpZone, vec3 n)
{
	// Transform to warp space:
	{
		mat3 rotate = warpZone->WarpCoords().ToMatrix();
		n = rotate * n;
	}

	// Transform from warp space:
	{
		mat3 rotate = warpZone->OtherSideActor()->WarpCoords().ToMatrix();
		mat3 invrotate = mat3::transpose(rotate);
		n = invrotate * n;
	}

	return n;
}

Coords VisibleFrame::WarpRotationToOtherSide(UWarpZoneInfo* warpZone, Coords rotation)
{
	mat3 newRotation = rotation.ToMatrix();

	// Transform to warp space:
	{
		vec3 origin = warpZone->WarpCoords().Origin;
		mat3 rotate = warpZone->WarpCoords().ToMatrix();
		//mat3 invrotate = mat3::transpose(rotate);
		newRotation = rotate * newRotation;
	}

	// Transform from warp space:
	{
		vec3 origin = warpZone->OtherSideActor()->WarpCoords().Origin;
		mat3 rotate = warpZone->OtherSideActor()->WarpCoords().ToMatrix();
		mat3 invrotate = mat3::transpose(rotate);
		newRotation = invrotate * newRotation;
	}

	rotation.XAxis = vec3(newRotation[0], newRotation[1], newRotation[2]);
	rotation.YAxis = vec3(newRotation[3], newRotation[4], newRotation[5]);
	rotation.ZAxis = vec3(newRotation[6], newRotation[7], newRotation[8]);
	return rotation;
}
