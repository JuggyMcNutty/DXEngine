
#include "Precomp.h"
#include "VisibleFrame.h"
#include <tuple>
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
#include "VisibleDecal.h"
#include "Packages/Engine/UDXEngineClient.h"
#include "Packages/Extension/Windows/TabGroup/URootWindow.h"
#include "Collision/TopLevel/CollisionSystem.h"

void VisibleFrame::Process(const vec3& location, const mat4& worldToView, const Coords& viewRotation, bool mirrorFlag, int portalDepth, const Array<PortalSpan>& portalSpans, const vec4& portalPlane, int viewZone)
{
	engine->render->Stats.Frames++;
	// The walk timed for STAT GLOBAL's Occlusion, as the original's
	// OccludeBsp clocks its own (Render.dll 0x10b17473-0x10b19c5f)
	StatClock clock(engine->render->PanelStats.Occlusion);

	Device = engine->render->Device;
	FrameCounter = engine->render->FrameCounter++;
	SceneIndex = FrameCounter - engine->render->SceneFirstFrame;
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
	// STAT OCCLUSION's figures of this walk, its stages timed while the
	// panel is shown (BspClipper::Stats)
	Clipper.Stats = {};
	Clipper.Timed = engine->render->StatPanels[RenderSubsystem::StatOcclusion];
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
	// (and a portal's spans) only. Their proxies, and every actor's, go down
	// the whole walk.
	ProcessRenderIterators();
	ProcessMoverMirrors();
	AddActorProxies();

	ProcessNode(0, 0, FragmentStack.size(), engine->Level->Model->RootOutside != 0);

	// STAT ZONE's: the zones this walk saw, of the level's model's own
	// count (its NumZones, not the list made up to 64), each walk's added,
	// as the original's OccludeBsp adds its (0x10b19bf9-0x10b19c31); and
	// STAT OCCLUSION's nodes of the level's BSP as the movers are filed into
	// it, the original's tracker filing them into the model's own nodes
	// (DrawFrame 0x10b1bb9d), and the clipper's figures
	auto& panel = engine->render->PanelStats;
	for (int seen : ZoneSeenFrame)
		panel.VisibleZones += seen == FrameCounter ? 1 : 0;
	panel.Zones += engine->Level->Model->NumZones;
	panel.Nodes += (int)engine->Level->Collision.LineNodes().size();
	panel.Clip.Add(Clipper.Stats);
}

// A mover's mirror, which the original files with the mover's other polygons
// into the level's BSP, to open a frame as any mirror there (dx-reverse-info
// render-dll.md, a frame). The walk meets a mover's filed polygons only to
// fill the spans (ProcessFiledSurface), so before it each mirrored face of a
// mover where it is drawn -- a filed one hidden too, as its filing stays in
// the BSP -- becomes a mirror's portal, its spans all of the face the frame
// shows: the depth test leaves out what lies in front of it. Each face a
// frame of its own, three frames deep at most; one seen from behind passed
// over, and a translucent one reflecting only while the render device's
// ShinySurfaces is on, as the BSP's mirrors. A face the brush tracker
// dropped whole, in solid or for want of room, lies in no node of the BSP:
// the original's walk never meets it, and it opens none, as it is not drawn
// (VisibleBrush::DrawBrushPoly). Six of the stock maps' 24 mirrored mover
// faces are so, glass buried in solid on 06_HongKong_WanChai_Underworld.
void VisibleFrame::ProcessMoverMirrors()
{
	if (PortalDepth >= 3)
		return;

	// Only the level's movers with a mirrored face (RenderSubsystem::MirrorMovers)
	for (UMover* mover : engine->render->MirrorMovers)
	{
		if (mover->bDeleteMe() || (!mover->Filed && mover->bHidden()) || mover->DrawType() != DT_Brush || !mover->Brush() || !mover->Brush()->Polys)
			continue;

		// Its polygons as the tracker last filed them; none for a mover it
		// does not file (spawned after the load)
		const Array<Array<BrushTracker::FiledFragment>>* filed = engine->Level->Collision.FiledFragments(mover);
		const Array<Poly>& polys = mover->Brush()->Polys->Polys;
		mat4 toWorld = VisibleBrush::MoverToWorld(mover);
		for (size_t index = 0; index < polys.size(); index++)
		{
			const Poly& poly = polys[index];
			uint32_t flags = poly.PolyFlags;
			if (!(flags & PF_Mirrored) || (flags & PF_Invisible) || poly.Vertices.size() < 3 || poly.Vertices.size() > 32)
				continue;
			if (filed && index < filed->size() && (*filed)[index].empty())
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
		// None whose left edge's pixel, the column it rounds up to, is the
		// frame's last or past it, as the original's has none (Render.dll
		// 0x10b23c91-0x10b23c9a)
		if (std::ceil(Frame.FX2 + (location.x - halfWidth) / z * (Frame.FX2 / FrustumTanX)) >= (float)(Frame.X - 1))
			return false;
		minX = std::max((location.x - halfWidth) / z, -FrustumTanX);
		maxX = std::min((location.x + halfWidth) / z, FrustumTanX);
		minY = std::max((location.y - halfHeight) / z, -FrustumTanY);
		maxY = std::min((location.y + halfHeight) / z, FrustumTanY);
		return minX < maxX && minY < maxY;
	}

	return false;
}

// The polygon cut to the view's four sides, which all meet at the eye, so
// no plane before it cuts it, and in a mirror's or a warp zone's frame to
// its plane, as the original's ClipBspSurf cuts a surface for the walk to
// save (Render.dll 0x10b14cb0; the plane, a mirror frame's NearClip, at
// 0x10b1545e): a point put in where an edge crosses a side, then each point
// beyond it taken out
void VisibleFrame::ViewPolygon(const vec3* points, int count, Array<vec3>& out) const
{
	out.assign(points, points + count);
	mat3 back = mat3::transpose(mat3(Frame.WorldToView));
	vec3 translation(Frame.WorldToView[12], Frame.WorldToView[13], Frame.WorldToView[14]);
	vec4 planes[5];
	int numPlanes = 0;
	for (const vec3& side : { vec3(1.0f, 0.0f, FrustumTanX), vec3(-1.0f, 0.0f, FrustumTanX), vec3(0.0f, 1.0f, FrustumTanY), vec3(0.0f, -1.0f, FrustumTanY) })
		planes[numPlanes++] = vec4(back * side, dot(side, translation));
	if (PortalPlane.x != 0.0f || PortalPlane.y != 0.0f || PortalPlane.z != 0.0f)
		planes[numPlanes++] = PortalPlane;

	Array<float> dist;
	for (int k = 0; k < numPlanes && !out.empty(); k++)
	{
		const vec4& plane = planes[k];
		dist.clear();
		for (const vec3& p : out)
			dist.push_back(dot(plane, vec4(p, 1.0f)));
		for (size_t i = 0; i < out.size(); i++)
		{
			size_t next = (i + 1) % out.size();
			if ((dist[i] > 0.0f && dist[next] < 0.0f) || (dist[i] < 0.0f && dist[next] > 0.0f))
			{
				vec3 p = out[i] + (out[next] - out[i]) * (dist[i] / (dist[i] - dist[next]));
				out.insert(out.begin() + i + 1, p);
				dist.insert(dist.begin() + i + 1, 0.0f);
				i++;
			}
		}
		size_t kept = 0;
		for (size_t i = 0; i < out.size(); i++)
		{
			if (dist[i] >= 0.0f)
				out[kept++] = out[i];
		}
		out.resize(kept);
	}
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

// Every actor's occlusion proxy starts at the BSP's root, as the original's
// SetupDynamics hands each actor it does not pass over to FDynamicSprite at
// node 0 (Render.dll 0x10b22d90, 0x10b233c0): cut at each plane on its way
// down (0x10b23e30), each piece tested where it comes to rest, a piece past
// a wall's plane on the wall's far side whatever node the actor lies under.
// The level's actors in its list's order; a mover has none, the brush
// tracker filing it, nor has an actor drawn as its render iterator's items,
// which have their own (ProcessRenderIterators).
void VisibleFrame::AddActorProxies()
{
	// None while the actors are hidden, SetupDynamics taking none
	// (RenderSubsystem::ActorsShown)
	if (!engine->render->ActorsShown)
		return;
	bool hasRenderIterators = PropOffsets_Actor.RenderInterface.DataOffset != ~(size_t)0;
	const vec4& plane = PortalPlane;
	for (UActor* actor : engine->Level->Actors)
	{
		if (!actor || actor->bDeleteMe() || UObject::TryCast<UMover>(actor))
			continue;
		if (hasRenderIterators && actor->RenderInterface())
			continue;
		if (VisibleActor::PassedOver(actor, PortalDepth))
			continue;

		// Behind a mirror, or before a warp zone's portal, nothing shows in
		// its frame (VisibleActor::Process)
		const BBox& box = actor->BspInfo.BoundingBox;
		vec3 farthest(plane.x >= 0.0f ? box.max.x : box.min.x, plane.y >= 0.0f ? box.max.y : box.min.y, plane.z >= 0.0f ? box.max.z : box.min.z);
		if (dot(plane, vec4(farthest, 1.0f)) < 0.0f)
			continue;

		AddOcclusionProxy(actor);
	}
}

void VisibleFrame::AddOcclusionProxy(UActor* actor)
{
	if (actor->LastRenderTime() == Now)
		return;

	// Only a mesh or a sprite gets one, as the original's sprites.
	float minX, minY, maxX, maxY, z;
	if (!SpriteRectangle(actor, minX, minY, maxX, maxY, z))
		return;

	// The rectangle set back at that depth, however near the eye, as the
	// original's FDynamicSprite sets it (Render.dll 0x10b233c0): its pieces
	// go down the BSP by that depth (SplitFragments), one at the eye's own
	// plane on the eye's side of every plane further off. Its corners lie
	// along their lines from the eye 1.5 units out at least, the clipper's
	// near plane cutting away what is nearer: the same pixels, which are
	// all the original's span buffer tests of it.
	float zTest = std::max(z, 1.5f);
	auto toWorld = [&](float x, float y) { return ViewToWorld * (vec3(x * zTest, y * zTest, zTest) - ViewTranslation); };
	OcclusionFragment fragment;
	fragment.Actor = actor;
	fragment.Sample = actor->DrawType() == DT_Mesh ? 1.0f : 0.0f;
	fragment.DepthScale = z < zTest ? z / zTest : 1.0f;
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
	// it is cut in two. Pieces of an actor already drawn are dropped, and so
	// is one with every corner within 0.01 of the plane, which the
	// original's walk hands to neither side (Render.dll 0x10b23e88-0x10b23eb9,
	// the sides taken at 0x10b24303 and 0x10b244c7).
	// A piece nearer than the depth it is tested at is on the plane's side
	// its own depth puts it: each corner's distance from the plane that far
	// along its line from the eye (DepthScale), which the cut keeps.
	const float epsilon = 0.01f;
	float eyeDist = dot(vec4(ViewToWorld * -ViewTranslation, 1.0f), plane);
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
			if (fragment.DepthScale < 1.0f)
				d = eyeDist + (d - eyeDist) * fragment.DepthScale;
			dist[v] = viewerInFront ? d : -d;
			if (dist[v] > epsilon)
				nearVerts++;
			else if (dist[v] < -epsilon)
				farVerts++;
		}
		if (nearVerts == 0 && farVerts == 0)
			continue;
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
		nearPiece.DepthScale = farPiece.DepthScale = fragment.DepthScale;
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
	if (!engine->render->ActorsShown)
		return;
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

	if (engine->render->ViewportRect.Set)
	{
		// A ViewportWindow's frame, and its mirrors', warp zones' and sky's:
		// the rectangle RenderSubsystem::DrawViewport takes from the window
		Frame.XB = engine->render->ViewportRect.X;
		Frame.YB = engine->render->ViewportRect.Y;
		Frame.X = engine->render->ViewportRect.Width;
		Frame.Y = engine->render->ViewportRect.Height;
		Frame.FX = (float)Frame.X;
		Frame.FY = (float)Frame.Y;
	}
	else if (engine->dxRootWindow && engine->dxRootWindow->RenderViewportSet)
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

		// A pixel at the least and 16,384 at the most: a frame of no width
		// had no aspect, and one past an int's range no size at all
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
	Frame.Projection = mat4::frustum(-RProjZ, RProjZ, -Aspect * RProjZ, Aspect * RProjZ, 1.0f, 32768.0f, handedness::left, clipzrange::zero_positive_w);
	FrustumTanX = RProjZ;
	FrustumTanY = Aspect * RProjZ;
}

// The walk takes the level's BSP with the movers' polygons the brush tracker
// filed into it, their nodes past the level's own, as the original's walk
// takes them, nodes of the level's own (OccludeBsp, Render.dll 0x10b173d0;
// dx-reverse-info render-dll.md, which actors are drawn): their planes cut
// the proxies, a solid one's back is solid, and their polygons fill the
// spans (ProcessFiledSurface). With no tracker, the level's own BSP.
void VisibleFrame::ProcessNode(int nodeIndex, size_t fragFirst, size_t fragCount, bool outside)
{
	// Skip node if it is not part of the portal zones we have seen so far
	//if ((node->ZoneMask & ViewZoneMask) == 0)
	//	return;

	UModel* model = engine->Level->Model;
	const Array<BspNode>& nodes = engine->Level->Collision.LineNodes();
	const int levelNodes = (int)model->Nodes.size();
	const BspNode* node = &nodes[nodeIndex];

	// The occlusion proxies' pieces that came down into this node's space
	// are FragmentStack[fragFirst, +fragCount), every proxy starting at the
	// root (AddActorProxies). All the walk has drawn so far lies in front
	// of them. Outside is whether this node's space is empty, not solid.
	size_t base = FragmentStack.size();
	engine->render->PanelStats.NodesVisited++;

	// Skip node if its AABB is not visible -- and what came down into its
	// space with it, as the original drops it. A filed node has none.
	if (node->RenderBound != -1 && !Clipper.IsAABBVisible(model->Bounds[node->RenderBound]))
	{
		return;
	}

	// Add bsp node actors to the visible set: those filed under a node of the
	// level's own; while the actors are hidden (RenderSubsystem::ActorsShown),
	// the filed movers alone, the level's polygons in the original
	bool actorsShown = engine->render->ActorsShown;
	if (nodeIndex < levelNodes)
	{
		for (UActor* actor = model->Nodes[nodeIndex].ActorList; actor != nullptr; actor = actor->BspInfo.Next)
		{
			if (!actorsShown)
			{
				UMover* mover = UObject::TryCast<UMover>(actor);
				if (!mover || !mover->Filed)
					continue;
			}
			if (actor->LastDrawFrame != FrameCounter)
			{
				actor->LastDrawFrame = FrameCounter;

				VisibleActor visactor;
				visactor.Process(this, actor);
			}
		}
	}

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
	size_t farCount = FragmentStack.size() - farFirst;
	size_t nearFirst = FragmentStack.size();
	for (int index : NearFragments)
		FragmentStack.push_back(index);
	size_t nearCount = FragmentStack.size() - nearFirst;

	// Recursively divide front space (toward the viewer)
	if (front >= 0)
	{
		ProcessNode(front, nearFirst, nearCount, nearOutside);
	}
	else
	{
		TestLeafFragments(nearFirst, nearCount, nearOutside, nearZone);
	}
	FragmentStack.resize(nearFirst);

	// Draw surfaces on this plane: the level's own, then a mover's polygon
	// filed in it, which hangs at the chain's end
	int poly = nodeIndex;
	while (true)
	{
		// Each coplanar node by its own plane, which may face the other way:
		// the side the viewer is on picks its zone (the original's
		// OccludeBsp, Render.dll 0x10b173d0).
		const BspNode& polynode = nodes[poly];
		vec4 polyPlane = { polynode.PlaneX, polynode.PlaneY, polynode.PlaneZ, -polynode.PlaneW };
		bool polyBehind = dot(ViewLocation, polyPlane) < 0.0f;
		if (poly < levelNodes)
			ProcessNodeSurface(&model->Nodes[poly], polyBehind);
		else
			ProcessFiledSurface(poly, polyBehind);

		if (polynode.Plane < 0) break;
		poly = polynode.Plane;
	}

	// Possibly divide back space (away from the viewer)
	if (back >= 0)
	{
		ProcessNode(back, farFirst, farCount, farOutside);
	}
	else
	{
		TestLeafFragments(farFirst, farCount, farOutside, farZone);
	}
	FragmentStack.resize(base);
}

// A mover's polygon the brush tracker filed, met where the walk meets its
// node, as the original's OccludeBsp takes it, a surface of the level's own
// (Render.dll 0x10b173d0): one-sided and seen from behind, passed over by
// its own flags; an opaque one -- neither masked, translucent, modulated nor
// invisible -- and a mirror fill the spans as a wall does, so that what lies
// behind a closed door, an actor's proxy or a zone portal, finds none; a
// portal's zones are seen. VisibleBrush draws it with its mover, and
// ProcessMoverMirrors opened a mirror's frame. A destroyed mover, drawn no
// more, hides nothing.
void VisibleFrame::ProcessFiledSurface(int index, bool behind)
{
	CollisionSystem& collision = engine->Level->Collision;
	const BspNode& node = collision.FiledNode(index);
	if (node.NumVertices < 3 || node.Surf < 0)
		return;
	const BspSurface& surface = collision.LineSurface(node.Surf);
	UMover* mover = UObject::TryCast<UMover>(surface.BrushActor);
	if (!mover || mover->bDeleteMe() || !mover->Brush())
		return;

	// The mover's box tested for its drawing before any of its polygons
	// fills the spans, should the walk meet one before the node the mover
	// is filed under
	if (mover->LastDrawFrame != FrameCounter)
	{
		mover->LastDrawFrame = FrameCounter;
		VisibleActor visactor;
		visactor.Process(this, mover);
	}

	uint32_t PolyFlags = surface.PolyFlags;
	if (!(PolyFlags & (PF_TwoSided | PF_Portal)) && behind)
		return;
	if (surface.Material)
		PolyFlags |= surface.Material->PolyFlags();

	// Its corners where the tracker filed it, turned round in a mirror's
	// frame as a level surface's are
	int numverts = node.NumVertices;
	vec3* points = engine->render->GetTempVertexBuffer(numverts);
	const vec3* corners = collision.FiledCorners(index);
	mat4 toWorld = mover->BrushToWorld();
	for (int j = 0; j < numverts; j++)
		points[MirrorFlag ? numverts - 1 - j : j] = (toWorld * vec4(corners[j], 1.0f)).xyz();

	bool occludes = (PolyFlags & PF_NoOcclude) == 0 || ((PolyFlags & PF_Mirrored) && PortalDepth < 3);
	if (!Clipper.CheckSurface(points, numverts, occludes))
		return;

	if (PolyFlags & PF_Portal)
		MarkPortalSeen(node);

	// The decals on its surface that show on this fragment, counted as drawn
	// (VisibleDecal::StampDecals)
	Array<LevelDecal>* decals = collision.FiledDecals(node.Surf);
	vec3 base, normal;
	if (decals && !decals->empty() && UDecal::SurfaceFrame(engine->Level, node.Surf, base, normal))
		VisibleDecal::StampDecals(this, *decals, base, normal, mover, points, numverts);
}

// A visible portal surface means the zones it borders were seen: their
// render time is what stasis and the AI events read for actors there.
void VisibleFrame::MarkPortalSeen(const BspNode& node)
{
	auto& zones = engine->Level->Model->Zones;
	float now = engine->LevelInfo->TimeSeconds();
	if ((size_t)node.Zone0 < zones.size())
		zones[node.Zone0].LastRenderTime = now;
	if ((size_t)node.Zone1 < zones.size())
		zones[node.Zone1].LastRenderTime = now;
	MarkZoneSeen(node.Zone0);
	MarkZoneSeen(node.Zone1);
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

	// A one-sided surface seen from behind is passed over first, a sky, a
	// warp zone and a mirror among them, by the surface's own flags: the
	// original's OccludeBsp (Render.dll 0x10b173d0) tests the surface's
	// with the viewport's ExtraPolyFlags at 0x10b17fc9, before anything
	// else, and ORs its texture's in only after (0x10b181b2), so a texture's
	// PF_TwoSided shows no surface from behind. Its test passes over a
	// surface more than a unit behind the eye (0x10b17fb9) and passes on one
	// within a unit, which no frame of the original's then shows either
	// (BackFaceConsole's views C and E). ExtraPolyFlags, which
	// UViewport::Lock sets as it locks (Engine.dll 0x10373750), is
	// PF_NoMerge with the viewer's RendMap at REN_Zones or REN_PolyCuts and
	// none otherwise: never PF_TwoSided or PF_Portal, so DXEngine keeps
	// none. A zone portal leads into the zone beyond it from either side, as
	// the original takes it. In a closed level a front face drawn earlier
	// already hides a back face, so a back face hides nothing either: the
	// clipper's test is skipped for over half of the surfaces in view. A
	// mirror's frame is seen from the reflected eye, its ViewLocation.
	if (!(PolyFlags & (PF_TwoSided | PF_Portal)))
	{
		vec4 plane = { node->PlaneX, node->PlaneY, node->PlaneZ, -node->PlaneW };
		if (dot(ViewLocation, plane) < 0.0f)
			return;
	}

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
		// 0x10b173d0, Recursion < 3); deeper, it is drawn as any surface. A
		// one-sided mirror seen from behind was passed over above.
		else if ((PolyFlags & PF_Mirrored) && PortalDepth < 3)
		{
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

	gatherPoints();

	if (!Clipper.CheckSurface(points, numverts, (PolyFlags & PF_NoOcclude) == 0))
		return;

	if (PolyFlags & PF_Portal)
		MarkPortalSeen(*node);

	// Its decals that ClipDecal leaves some of on it, counted as drawn, an
	// invisible surface's too, as the original's draw lists keep one: a
	// drawn decal stamps its own LastRenderedTime; LastRendered() reads
	// LastRenderTime, which a decal never gets, so a decal never counts as
	// drawn.
	if (!node->Decals.empty())
		VisibleDecal::StampDecals(this, node->Decals, model->Points[surface.pBase], model->Vectors[surface.vNormal], nullptr, points, numverts);

	if (PolyFlags & PF_Invisible)
		return;

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
	// The frames through its portals drawn, each from its own eye: its own
	// draws from its own
	engine->Level->Light.SetEye(ViewLocation.xyz(), SceneIndex, Frame.WorldToView);
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
	// By zone, then texture, then light map: a run of one texture and one
	// page of light maps is one batch (LightmapAtlas). The original's draw
	// list sorts its solid surfaces by a key of its objects' indices, the
	// zone actor's, the texture's and its palette's, in a sort that keeps no
	// order among equal keys (dx-reverse-info render-dll.md, a frame), which
	// no run can give: where two surfaces overlap at one depth, which shows
	// can differ, by choice. Elsewhere the depth test makes the order free.
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
		mat3 rotate = warpZone->WarpCoords().ToMatrix();
		//mat3 invrotate = mat3::transpose(rotate);
		newRotation = rotate * newRotation;
	}

	// Transform from warp space:
	{
		mat3 rotate = warpZone->OtherSideActor()->WarpCoords().ToMatrix();
		mat3 invrotate = mat3::transpose(rotate);
		newRotation = invrotate * newRotation;
	}

	rotation.XAxis = vec3(newRotation[0], newRotation[1], newRotation[2]);
	rotation.YAxis = vec3(newRotation[3], newRotation[4], newRotation[5]);
	rotation.ZAxis = vec3(newRotation[6], newRotation[7], newRotation[8]);
	return rotation;
}
