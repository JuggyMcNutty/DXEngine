#pragma once

#include "Packages/Engine/Resources/UPrimitive.h"

class UTexture;

struct MeshTri
{
	uint16_t Indices[3];
	uvec2b UV[3];
	uint32_t PolyFlags;
	int32_t TextureIndex;
};

struct MeshAnimNotify
{
	float Time;
	NameString Function;
};

struct MeshAnimSeq
{
	NameString Name;
	NameString Group;
	int StartFrame;
	int NumFrames;
	float Rate;
	Array<MeshAnimNotify> Notifys;
};

struct MeshVertConnect
{
	int NumVertTriangles;
	uint32_t TriangleListOffset;
};

struct MeshFace
{
	uint16_t Indices[3];
	uint16_t MaterialIndex;
};

struct MeshWedge
{
	uint16_t Vertex;
	uint8_t U;
	uint8_t V;
};

struct MeshMaterial
{
	uint32_t PolyFlags = 0;
	int TextureIndex = 0;
};

class UMesh : public UPrimitive
{
public:
	using UPrimitive::UPrimitive;

	void Load(ObjectStream* stream) override;
	void Save(PackageStreamWriter* stream) override;
	void Mark(GCMarker& marker) override;

	// The sequence of that name; none for a name the mesh lacks, as the
	// original's GetAnimSeq answers (it gave the first sequence, so HasAnim
	// was true for any name and PlayAnim of a missing one played the first)
	MeshAnimSeq* GetSequence(const NameString& name)
	{
		for (MeshAnimSeq& seq : AnimSeqs)
			if (seq.Name == name)
				return &seq;
		return nullptr;
	}

	Array<vec3> Verts;
	Array<MeshTri> Tris;
	Array<MeshAnimSeq> AnimSeqs;
	Array<MeshVertConnect> Connects;
	Array<BBox> BoundingBoxes;
	Array<vec4> BoundingSpheres;
	BBox MeshBoundingBox;
	vec4 MeshBoundingSphere;
	Array<int> VertLinks;
	Array<UTexture*> Textures;
	Array<float> TextureLOD;
	int FrameVerts = 0;
	int AnimFrames = 0;
	uint32_t AndFlags = 0;
	uint32_t OrFlags = 0;
	vec3 Scale = { 0.0f };
	vec3 Origin = { 0.0f };
	Rotator RotOrigin;
	uint32_t CurPoly = 0;
	uint32_t CurVertex = 0;

	Array<vec3> Normals;
	mat4 meshToObject;

	// A face drawn opaque after one drawn translucent, in face order: the
	// original draws a mesh's faces in their order, so a translucent face
	// that a later one behind it writes over is lost (render-dll.md, which
	// faces are drawn), and such a mesh is drawn whole in that order
	bool OpaqueAfterTranslucent = false;
	// Sets it from each face's flags in turn, a face of PF_Invisible drawn
	// by neither
	void FindFaceOrder(const Array<uint32_t>& faceFlags);
};
