#pragma once

#include "Engine.h"
#include "VisibleNode.h"
#include "VisibleTranslucent.h"
#include "VisibleCorona.h"
#include "VisibleActor.h"
#include "VisiblePortal.h"
#include "BspClipper.h"
#include "RenderDevice/RenderDevice.h"

class VisibleFrame
{
public:
	// A mirror's frame gives its eye reflected in the mirror, which lies behind
	// it, and the zone on the viewer's side, which it starts in (viewZone);
	// any other frame starts in the zone at its eye.
	void Process(const vec3& location, const mat4& worldToView, const Coords& viewRotation, bool mirrorFlag = false, int portalDepth = 0, const Array<PortalSpan>& portalSpans = {}, const vec4& portalPlane = vec4(0.0f, 0.0f, 0.0f, 1.0f), int viewZone = -1);
	void Draw();
	void DrawCoronas();

	RenderDevice* Device = nullptr;

	SceneNode Frame;
	BspClipper Clipper;
	vec4 ViewLocation = vec4(0.0f);
	// A mirror's or a warp zone's plane, positive on the side its frame shows:
	// what lies wholly on the other is not drawn.
	vec4 PortalPlane = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	Coords ViewRotation = {};
	int ViewZone = 0;
	//uint64_t ViewZoneMask = 0;
	int FrameCounter = 0;
	int SceneIndex = 0;   // its place among the scene's frames, 0 the main view's
	bool MirrorFlag = false;
	int PortalDepth = 0;
	// Deus Ex's fog maps drawn this frame: the render device's
	// VolumetricLighting on and the viewport's actor in a fog zone, as the
	// original's OccludeBsp gathers volumetric lights only then
	// (dx-reverse-info render-dll.md, light maps).
	bool VolumetricFog = false;

	Array<VisibleNode> OpaqueNodes;
	Array<VisibleActor> Actors;
	Array<VisibleCorona> Coronas;
	Array<VisiblePortal> Portals;

	// Whether an actor counts as drawn, for its LastRenderTime: stamped only
	// if some piece of its proxy survives the world in front of it.
	void AddOcclusionProxy(UActor* actor);

	// Whether a mover's polygon reflects this frame (ProcessMoverMirrors)
	bool HasMoverMirror(const UMover* mover, const Poly* poly) const;

	// What is drawn after the opaque world and actors. Deus Ex's goes in
	// three lists, each back to front, as its DrawFrame draws them
	// (dx-reverse-info/render-dll.md, a frame): the parts of the actors not
	// drawn translucent -- a mesh's translucent faces, a sprite of another
	// style --, then the translucent and modulated surfaces, the movers'
	// among them, then the actors drawn translucent (STY_Translucent). Other
	// games keep Surreal Engine's one list, nearest first.
	enum TranslucentList { SolidActorParts, TranslucentSurfaces, TranslucentActors, NumTranslucentLists };
	void AddTranslucent(TranslucentList list, const VisibleTranslucent& item);
	static TranslucentList ActorTranslucentList(UActor* actor) { return actor->Style() == STY_Translucent ? TranslucentActors : SolidActorParts; }

	// The actor's rectangle on the frame, as the original's sprite setup
	// makes it (Render.dll FDynamicSprite::Setup 0x10b23910): a mesh's
	// render box through BoundVisible, a sprite's texture at its draw scale,
	// none for a location behind the viewer or another draw type. In the
	// view's x across and y up over the depth; z the location's depth.
	bool SpriteRectangle(UActor* actor, float& minX, float& minY, float& maxX, float& maxY, float& z);

	// A tile drawn in the frame, as the original's sprite is, lies half a
	// pixel left of and above its polygons (SceneNode::PolygonsAtFX15): the
	// move, in the world, that puts a quad facing the view at location there.
	vec3 TileOffset(const vec3& location) const;

private:
	// An actor's proxy for the occlusion test, as the original's sprites
	// have one: its screen rectangle set back in the world at its
	// location's depth, filtered down the BSP and split at its planes, each
	// piece tested at its own place in the front-to-back walk.
	struct OcclusionFragment
	{
		enum { MaxVerts = 12 };
		UActor* Actor = nullptr;
		float Sample = 1.0f; // the clipper's pixel sample for it: a sprite's 0, a mesh's 1
		// Its corners lie at the depth the clipper tests it at, the proxy's
		// own nearer than that: how far along their lines from the eye the
		// proxy's own lie, 1 when they are the same (AddOcclusionProxy)
		float DepthScale = 1.0f;
		int NumVerts = 0;
		vec3 Verts[MaxVerts];
	};

	void SetupSceneFrame(const mat4& worldToView);
	bool BoundRectangle(const BBox& box, float& minX, float& minY, float& maxX, float& maxY);
	void ProcessNode(BspNode* node, size_t fragFirst, size_t fragCount, bool outside);
	void SplitFragments(size_t first, size_t count, const vec4& plane, bool viewerInFront);
	void TestLeafFragments(size_t first, size_t count, bool outside, int zone);
	void TestFragment(int index);
	void MarkZoneSeen(int zone);

	Array<OcclusionFragment> Fragments;
	Array<int> FragmentStack;
	Array<int> NearFragments;
	Array<int> ZoneSeenFrame;
	mat3 ViewToWorld;
	vec3 ViewTranslation = vec3(0.0f);
	float FrustumTanX = 1.0f;       // the frame's half width and height over the depth
	float FrustumTanY = 1.0f;
	float Now = 0.0f;
	void ProcessNodeSurface(BspNode* node, bool front);
	void ProcessRenderIterators();
	void ProcessMoverMirrors();
	void SortTranslucent();

	Array<VisibleTranslucent> Translucents[NumTranslucentLists];
	bool DeusExOrder = false;

	void DrawOpaqueNodes();
	void DrawOpaqueActors();
	void DrawTranslucent();
	void DrawPortals();

	int FindZoneAt(const vec3& location);
	int FindZoneAt(const vec4& location, BspNode* node, BspNode* nodes);

	vec3 WarpLocationToOtherSide(UWarpZoneInfo* warpZone, vec3 p);
	vec3 WarpNormalToOtherSide(UWarpZoneInfo* warpZone, vec3 n);
	Coords WarpRotationToOtherSide(UWarpZoneInfo* warpZone, Coords rotation);
};
