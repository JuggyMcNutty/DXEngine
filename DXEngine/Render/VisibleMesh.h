#pragma once

#include "Math/mat.h"
#include "RenderDevice/RenderDevice.h"

class UActor;
class UMesh;
struct MeshFace;
class ULodMesh;
class VisibleFrame;

struct BlendInfo
{
	int offsets[3];
	float t0, t1;
	float weight;
};

// One vertex of a mesh as DrawLodMeshFaceDX last computed it: valid for the
// draw whose Generation it holds, its light for the LightKey's flags.
// TakenUnlit: an unlit face drawn so far this draw has written the unlit
// level into its light, as the original's face draw does (DrawMeshDX keeps
// that alone of a plain mesh's vertices here).
struct CachedMeshVertex
{
	uint32_t Generation = 0;
	uint32_t LightKey = 0;
	bool TakenUnlit = false;
	vec3 Point;
	vec3 Normal;
	vec3 Light;
};

// Which of a mesh's faces a draw takes: the opaque ones, the translucent ones
// (the scene draws them later, back to front), or all of them in face order,
// as the original draws a mesh
enum class MeshPass { Opaque, Translucent, FaceOrder };

class VisibleMesh
{
public:
	// Whether the actor's mesh is drawn whole in face order, an opaque face
	// coming after a translucent one, the actor's style not translucent
	static bool DrawsInFaceOrder(UActor* actor);
	bool DrawMesh(VisibleFrame* frame, UActor* actor, MeshPass pass);

	// Drawn as lines, as the original's renderer draws a mesh with the
	// viewer's RendMap at wire (Canvas.DrawActor's WireFrame)
	bool Wire = false;

	// Drawn on its own (Canvas.DrawActor, Canvas.DrawClippedActor,
	// GC.DrawActor), not by the scene: lit without the moving lights, as
	// the original's URender::DrawActor gives its sprite no leaf lights
	// (Render.dll 0x10b262c0; LightSystem::SetupForActorDX)
	bool OnItsOwn = false;

private:
	void DrawDebugInfo(VisibleFrame* frame, UActor* actor);
	bool WireTriangleInView(VisibleFrame* frame);
	bool DrawMeshAtLocation(VisibleFrame* frame, UActor* actor, UActor* lightLocationActor, UMesh* mesh, const mat4& meshToWorld, const mat3& meshNormalToWorld, MeshPass pass);
	bool DrawMeshDX(VisibleFrame* frame, UActor* actor, UActor* lightLocationActor, UMesh* mesh, const mat4& ObjectToWorld, const mat3& ObjectNormalToWorld, MeshPass pass);
	bool DrawLodMeshDX(VisibleFrame* frame, UActor* actor, UActor* lightLocationActor, ULodMesh* mesh, const mat4& ObjectToWorld, const mat3& ObjectNormalToWorld, MeshPass pass);
	bool DrawLodMeshFaceDX(VisibleFrame* frame, UActor* actor, UActor* lightLocationActor, ULodMesh* mesh, const Array<MeshFace>& faces, const mat4& ObjectToWorld, const mat3& ObjectNormalToWorld, int baseVertexOffset, const int* vertexOffsets, float t0, float t1, MeshPass pass, BlendInfo* blends, int blendCount);
	void SetupMeshTextures(UActor* actor, UMesh* mesh);
	// A bParticles mesh's vertices, particlePoints, each drawn as a tile
	bool DrawParticles(VisibleFrame* frame, UActor* actor, uint32_t polyFlags, MeshPass pass);
	void FindAttachmentPoints(ULodMesh* mesh, const mat4& ObjectToWorld, const int* vertexOffsets, float t0, float t1);

	Array<vec3> attachmentTris;
	Array<vec3> particlePoints;

	Array<CachedMeshVertex> vertexCache;
	uint32_t vertexCacheGeneration = 0;
	Array<GouraudVertex> faceBatch;
};
