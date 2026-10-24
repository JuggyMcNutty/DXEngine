#pragma once

#include "Math/vec.h"
#include "Math/mat.h"
#include <vector>
#include <cstdint>

class SoftwareTexture;

// A scene node's transform and viewport, copied at each SetSceneNode: the
// renderer hands some nodes on the stack (VisibleBrush's), and every draw
// is set up at Unlock.
struct SoftwareState
{
	mat4 ObjectToProjection; // PolygonShift, the frustum, WorldToView, ObjectToWorld
	// NearClip carried into object space: a point is drawn where its
	// distance from it is not negative, as the Vulkan device's clip distance
	vec4 ClipPlane = vec4(0.0f);
	int ViewportX = 0;
	int ViewportY = 0;
	int ViewportWidth = 0;
	int ViewportHeight = 0;
	float RFX2 = 0.0f;
	float RFY2 = 0.0f;
};

// A vertex as the Vulkan device's SceneVertex has it
struct SoftwareVertex
{
	vec3 Position;
	// The base texture's; the light map's (a mesh's fog colour, rg); the
	// macro texture's (the fog's ba); the detail texture's or fog map's
	vec2 TexCoord[4];
	vec4 Color;
};

enum class SoftwareDrawType : uint8_t
{
	Polygon,   // a fan of its vertices: (0, i - 1, i)
	Triangles, // three vertices each
	Tile,      // four corners, drawn as a rectangle where they make one on the screen
	Line,      // two vertices
	ClearZ,    // the depth buffer cleared, all of it
	Flash      // the screen flash over the viewport
};

// The blend of a draw's pixels into the frame, by its flags as the Vulkan
// device's pipelines take them (RenderPassManager::GetPipeline)
enum class SoftwareBlend : uint8_t
{
	Translucent,  // ONE, ONE_MINUS_SRC_COLOR
	Modulated,    // DST_COLOR, SRC_COLOR
	Highlighted,  // ONE, ONE_MINUS_SRC_ALPHA
	Solid,        // ONE, ZERO
	SubpixelFont  // CONSTANT_COLOR, ONE_MINUS_SRC_COLOR
};

// One draw: its vertices, and what its pixels are made with
struct SoftwareDraw
{
	SoftwareDrawType Type = SoftwareDrawType::Polygon;
	SoftwareBlend Blend = SoftwareBlend::Solid;
	bool ColorWrite = true;  // not PF_Invisible
	bool DepthWrite = false; // PF_Occlude
	bool DepthTest = true;   // all but the subpixel font's
	bool AlphaTest = false;  // PF_Masked: alpha under a half discarded
	bool NoSmooth = false;   // the base texture's nearest texel and mip
	bool Clamp = false;      // the base texture held at its edges
	bool DepthBias = false;  // a mover's step toward the viewer
	uint32_t ShaderFlags = 0; // Scene.frag's flags (VulkanRenderDevice's SceneVertex::Flags)
	int State = 0;
	uint32_t FirstVertex = 0;
	uint32_t VertexCount = 0;
	float DepthMin = 0.1f;
	float DepthMax = 1.0f;
	SoftwareTexture* Texture = nullptr; // null: the white texel
	SoftwareTexture* LightMap = nullptr;
	SoftwareTexture* MacroTexture = nullptr;
	SoftwareTexture* DetailTexture = nullptr; // or the fog map
	vec4 Constant = vec4(0.0f); // the subpixel font's blend colour; the flash's colour, its scale in w
};

// A run of draws the front end sets up apart from the others: a chunk
// closes at a draw's end once it holds about MaxTriangles
struct SoftwareChunk
{
	uint32_t FirstDraw = 0;
	uint32_t LastDraw = 0;
};

// The frame's draws in the order given, kept from frame to frame so their
// storage is reused
class SoftwareCommands
{
public:
	static constexpr uint32_t MaxTriangles = 128;

	void Reset();
	int AddState(const SoftwareState& state);
	// A draw on the current state; its vertices follow, VertexCount of them
	SoftwareDraw& AddDraw(SoftwareDrawType type, uint32_t vertexCount, SoftwareVertex*& vertices);
	void CloseChunk();

	int CurrentState() const { return (int)States.size() - 1; }
	bool Empty() const { return Draws.empty(); }

	// Whether the next draw of vertexCount vertices, or the next state, moves
	// the storage what is recorded lies in, which the device's threads may
	// be reading
	bool WouldGrow(uint32_t vertexCount) const { return Draws.size() == Draws.capacity() || Vertices.size() + vertexCount > Vertices.capacity(); }
	bool StateWouldGrow() const { return States.size() == States.capacity(); }

	std::vector<SoftwareState> States;
	std::vector<SoftwareDraw> Draws;
	std::vector<SoftwareVertex> Vertices;
	std::vector<SoftwareChunk> Chunks;

private:
	uint32_t ChunkTriangles = 0;
	uint32_t ChunkStart = 0;
};
