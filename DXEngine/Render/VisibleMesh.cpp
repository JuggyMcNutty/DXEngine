
#include "Precomp.h"
#include "VisibleMesh.h"
#include "VisibleFrame.h"
#include "RenderSubsystem.h"
#include "RenderDevice/RenderDevice.h"
#include "Engine.h"
#include "VM/Frame.h"
#include "Packages/Engine/Actors/Pawn/UPawn.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/Actors/Inventory/UWeapon.h"
#include "Packages/Engine/Actors/Inventory/UInventory.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/NavigationPoint/UNavigationPoint.h"
#include "Packages/Engine/Resources/Mesh/ULodMesh.h"
#include "Packages/Engine/Resources/Level/UModel.h"

// Whether a mesh face faces the eye, as the original's DrawMesh and
// DrawLodMesh ask it (Render.dll 0x10b0d1a0, 0x10b0ea80): (A − B) × (C − A)
// against A from the eye, in its view space -- which mirrors the world's
// (x right, y down, z ahead), so the other way round here.
static bool FacesEye(const GouraudVertex* v, const vec3& eye)
{
	return dot(cross(v[0].Point - v[1].Point, v[2].Point - v[0].Point), v[0].Point - eye) > 0.0f;
}

// The sequence's two frames at an animation position (AnimFrame times the
// sequence's frames) and the second's weight: the weight taken before the
// position is wrapped into the sequence, as the original's GetFrame takes
// it (an AnimFrame of 1 or more, a script's, drew frames 0 and 1 at a weight
// of NumFrames or more, the vertices thrown far past both); a sequence of no
// frames (a hand-made mesh's) holds its first, not a division by zero.
static void SequenceFrames(const MeshAnimSeq* seq, float animFrame, int& frame0, int& frame1, float& t)
{
	int frame = animFrame < 2147483520.0f ? (int)animFrame : 0;
	t = animFrame - (float)frame;
	if (seq->NumFrames <= 0)
	{
		frame0 = 0;
		frame1 = 0;
		t = 0.0f;
		return;
	}
	frame0 = frame % seq->NumFrames;
	frame1 = (frame0 + 1) % seq->NumFrames;
}

// The actor's sequence of the mesh, or, for a name the mesh lacks (None, most
// decorations'), a sequence of the mesh's first frame alone: the original's
// GetFrame draws frame 0 for a sequence it does not find (Engine.dll
// ULodMesh::GetFrame 0x10355360, UMesh::GetFrame 0x103a7780)
static const MeshAnimSeq* SequenceOrFirstFrame(UMesh* mesh, const NameString& name)
{
	static const MeshAnimSeq firstFrame = { NameString(), NameString(), 0, 1, 0.0f, {} };
	const MeshAnimSeq* seq = mesh->GetSequence(name);
	return seq ? seq : &firstFrame;
}

bool VisibleMesh::DrawsInFaceOrder(UActor* actor)
{
	UMesh* mesh = actor->Mesh();
	return mesh && mesh->OpaqueAfterTranslucent && actor->Style() != STY_Translucent && actor->Style() != STY_Modulated;
}

bool VisibleMesh::DrawMesh(VisibleFrame* frame, UActor* actor, MeshPass pass)
{
	UMesh* mesh = actor->Mesh();
	if (!mesh)
		return false;

	engine->render->Stats.Actors++;
	// The actor's lights are picked in SetupForActorDX, not here

	// DrawDebugInfo(frame, actor);

	mat4 objectToWorld = mat4::translate(actor->Location() + actor->PrePivot()) * Coords::Rotation(actor->Rotation()).ToMatrix() * mat4::scale(actor->DrawScale());
	mat4 meshToWorld = objectToWorld * mesh->meshToObject;

	// Note: using transpose seems to be wrong here
	mat3 meshNormalToWorld = mat3(Coords::Rotation(actor->Rotation()).ToMatrix() * Coords::Rotation(mesh->RotOrigin).ToMatrix());

	bool needsTranslucentPass = DrawMeshAtLocation(frame, actor, actor, mesh, meshToWorld, meshNormalToWorld, pass);

	// A pawn's own attachments (render-dll.md, A pawn's attachments): where its
	// mesh has a triangle to hold a weapon at, the weapon in its third-person
	// mesh and scale at that triangle -- or, holding no weapon, its selected
	// item the same way (a multitool or a lockpick in the player's hand) --
	// in the pawn's style and lit as the pawn. A mesh with no such triangle
	// draws neither.
	if (UPawn* pawn = UObject::TryCast<UPawn>(actor))
	{
		ULodMesh* pawnLodMesh = UObject::TryCast<ULodMesh>(mesh);
		UInventory* held = pawn->Weapon();
		if (!held)
			held = pawn->SelectedItem();
		UMesh* heldMesh = held ? held->ThirdPersonMesh() : nullptr;
		if (heldMesh && pawnLodMesh && attachmentTris.size() >= 3)
		{
			// Find attachment triangle location in the world:
			const vec3& v0 = attachmentTris[0];
			const vec3& v1 = attachmentTris[1];
			const vec3& v2 = attachmentTris[2];

			// Create a coordinate space converting from weapon to the world:
			Coords attachment;
			attachment.XAxis = normalize(v1 - v0);
			attachment.YAxis = normalize(cross(attachment.XAxis, v2 - v0));
			attachment.ZAxis = normalize(cross(attachment.XAxis, attachment.YAxis));
			attachment.Origin = -(v0 + v2) * 0.5f;

			// Place what is held in this coordinate space, drawn in the pawn's
			// style for the draw as the original swaps it in (a cloaked pawn's
			// weapon goes translucent with it):
			mat4 heldMeshToWorld = attachment.ToMatrix() * mat4::scale(held->ThirdPersonScale()) * heldMesh->meshToObject;
			// Its normals turned as the pawn's own are, by the rotation alone:
			// the attachment's axes (orthonormal) and the mesh's RotOrigin
			mat3 heldNormalToWorld = mat3(attachment.ToMatrix() * Coords::Rotation(heldMesh->RotOrigin).ToMatrix());
			uint8_t& heldStyle = held->Value<uint8_t>(PropOffsets_Actor.Style);
			uint8_t ownStyle = heldStyle;
			heldStyle = (uint8_t)pawn->Style();
			needsTranslucentPass = DrawMeshAtLocation(frame, held, actor, heldMesh, heldMeshToWorld, heldNormalToWorld, pass) || needsTranslucentPass;
			heldStyle = ownStyle;
		}
	}

	return needsTranslucentPass;
}

bool VisibleMesh::DrawMeshAtLocation(VisibleFrame* frame, UActor* actor, UActor* lightLocationActor, UMesh* mesh, const mat4& meshToWorld, const mat3& meshNormalToWorld, MeshPass pass)
{
	if (auto lodmesh = UObject::TryCast<ULodMesh>(mesh))
	{
		return DrawLodMeshDX(frame, actor, lightLocationActor, lodmesh, meshToWorld, meshNormalToWorld, pass);
	}
	else
	{
		return DrawMeshDX(frame, actor, lightLocationActor, mesh, meshToWorld, meshNormalToWorld, pass);
	}
}

// Each of the mesh's texture slots as the original fills it (UMesh::GetTexture):
// the actor's MultiSkins entry; else the mesh's own texture -- the Skin
// before it in slot 0 --, then the Skin; else none. Each is brought up to date
// and shown at its animation's current frame. What a face with none, or an
// environment-mapped face, draws with is the environment map: the actor's
// Texture, its zone's map, the level's, or at last the last slot's texture.
// The viewer's Sprite, when it has one (Deus Ex's Matrix easter egg), stands
// in for all of them.
void VisibleMesh::SetupMeshTextures(UActor* actor, UMesh* mesh)
{
	auto& textures = engine->render->Mesh.textures;
	if (textures.size() < mesh->Textures.size())
		textures.resize(mesh->Textures.size());

	UTexture* sprite = nullptr;
	if (UPlayerPawn* viewer = UObject::TryCast<UPlayerPawn>(engine->viewport->Actor()))
		sprite = viewer->Sprite();

	UTexture* last = nullptr;
	for (int i = 0; i < (int)mesh->Textures.size(); i++)
	{
		UTexture* tex = actor->GetMultiskin(i);
		if (!tex)
		{
			tex = (i != 0) ? mesh->Textures[i] : nullptr;
			if (!tex)
				tex = actor->Skin();
			if (!tex)
				tex = mesh->Textures[i];
		}
		if (sprite)
			tex = sprite;
		if (tex)
		{
			engine->render->UpdateTexture(tex);
			tex = tex->GetAnimTexture();
			last = tex;
		}
		textures[i] = tex;
	}

	UTexture* envmap = actor->Texture();
	if (!envmap && actor->Region().Zone)
		envmap = actor->Region().Zone->EnvironmentMap();
	if (!envmap)
		envmap = actor->Level()->EnvironmentMap();
	if (!envmap)
		envmap = last;
	if (sprite)
		envmap = sprite;
	engine->render->Mesh.envmap = envmap;
}

void VisibleMesh::FindAttachmentPoints(ULodMesh* mesh, const mat4& ObjectToWorld, const int* vertexOffsets, float t0, float t1)
{
	for (const MeshFace& face : mesh->SpecialFaces)
	{
		for (int i = 0; i < 3; i++)
		{
			size_t vbase = (size_t)face.Indices[i];
			size_t vindex = mesh->ReMapAnimVerts.empty() || vbase >= mesh->ReMapAnimVerts.size() ? vbase : mesh->ReMapAnimVerts[vbase];
			size_t vindex0 = vindex + vertexOffsets[0];
			size_t vindex1 = vindex + vertexOffsets[1];

			if (vindex0 >= mesh->Verts.size() || vindex1 >= mesh->Verts.size())
				return; // out of bounds

			const vec3& v0 = mesh->Verts[vindex0];
			const vec3& v1 = mesh->Verts[vindex1];
			vec3 vertex = mix(v0, v1, t0);
			if (t1 != 0.0f)
			{
				size_t vindex2 = vindex + vertexOffsets[2];
				if (vindex2 >= mesh->Verts.size())
					return; // out of bounds

				const vec3& v2 = mesh->Verts[vindex2];
				vertex = mix(vertex, v2, t1);
			}

			attachmentTris.push_back((ObjectToWorld * vec4(vertex, 1.0f)).xyz());
		}
	}
}

bool VisibleMesh::DrawMeshDX(VisibleFrame* frame, UActor* actor, UActor* lightLocationActor, UMesh* mesh, const mat4& ObjectToWorld, const mat3& ObjectNormalToWorld, MeshPass pass)
{
	auto lightsys = &engine->Level->Light;
	float fatness = actor->Fatness() / 16.0f - 8.0f;

	UActor* animSource = actor;
	if (actor->bAnimByOwner() && actor->Owner())
		animSource = actor->Owner();

	const MeshAnimSeq* seq = SequenceOrFirstFrame(mesh, animSource->AnimSequence());
	float animFrame = animSource->AnimFrame() * seq->NumFrames;

	int vertexOffsets[3];
	float t0, t1;

	if (animFrame >= 0.0f)
	{
		int frame0, frame1;
		SequenceFrames(seq, animFrame, frame0, frame1, t0);
		t1 = 0.0f;
		vertexOffsets[0] = (seq->StartFrame + frame0) * mesh->FrameVerts;
		vertexOffsets[1] = (seq->StartFrame + frame1) * mesh->FrameVerts;
		vertexOffsets[2] = 0;
	}
	else // Tween from old animation
	{
		t0 = animSource->TweenFromAnimFrame.T;
		t1 = clamp(animFrame + 1.0f, 0.0f, 1.0f);
		vertexOffsets[0] = animSource->TweenFromAnimFrame.V0;
		vertexOffsets[1] = animSource->TweenFromAnimFrame.V1;
		vertexOffsets[2] = seq->StartFrame * mesh->FrameVerts;
	}

	BlendInfo blends[4];
	int blendCount = 0;

	for (int i = 0; i < 4; i++)
	{
		if (animSource->BlendAnimSequence()[i].IsNone())
			continue;

		MeshAnimSeq* seq = mesh->GetSequence(animSource->BlendAnimSequence()[i]);
		if (!seq || seq->Name != animSource->BlendAnimSequence()[i])
			continue;

		float frame = animSource->BlendAnimFrame()[i] * seq->NumFrames;

		BlendInfo& b = blends[blendCount++];

		if (frame >= 0.0f)
		{
			int f0, f1;
			SequenceFrames(seq, frame, f0, f1, b.t0);
			b.t1 = 0.0f;

			b.offsets[0] = (seq->StartFrame + f0) * mesh->FrameVerts;
			b.offsets[1] = (seq->StartFrame + f1) * mesh->FrameVerts;
			b.offsets[2] = 0;

			b.weight = 1.0f;
		}
		else // Tween from old blend state to new frame 0
		{
			float tweenFactor = clamp(frame + 1.0f, 0.0f, 1.0f);

			if (animSource->TweenFromBlendAnimFrame[i].T < 0.0f)
			{
				b.offsets[0] = seq->StartFrame * mesh->FrameVerts;
				b.offsets[1] = seq->StartFrame * mesh->FrameVerts;
				b.offsets[2] = seq->StartFrame * mesh->FrameVerts;
				b.t0 = 0.0f;
				b.t1 = 0.0f;
				b.weight = tweenFactor;
			}
			else
			{
				b.offsets[0] = animSource->TweenFromBlendAnimFrame[i].V0;
				b.offsets[1] = animSource->TweenFromBlendAnimFrame[i].V1;
				b.offsets[2] = seq->StartFrame * mesh->FrameVerts;
				b.t0 = animSource->TweenFromBlendAnimFrame[i].T;
				b.t1 = tweenFactor;
				b.weight = 1.0f;
			}
		}
	}

	SetupMeshTextures(actor, mesh);

	uint32_t polyflags = 0;
	switch (actor->Style())
	{
	default: break;
	case STY_None: break;
	case STY_AlphaBlend: break;
	case STY_Masked: polyflags |= PF_Masked; break;
	case STY_Translucent: polyflags |= PF_Translucent; break;
	case STY_Modulated: polyflags |= PF_Modulated; break;
	}
	if (actor->bNoSmooth()) polyflags |= PF_NoSmooth;
	if (actor->bSelected()) polyflags |= PF_Selected;
	if (actor->bMeshEnviroMap()) polyflags |= PF_Environment;
	if (actor->bMeshCurvy()) polyflags |= PF_Flat;
	if (actor->bNoSmooth()) polyflags |= PF_NoSmooth;
	if (actor->bUnlit() || actor->Region().ZoneNumber == 0) polyflags |= PF_Unlit;

	bool needTranslucentPass = false;

	UZoneInfo* zoneActor = engine->GetZoneActor(actor->Region().ZoneNumber);

	VertexLight vertexLight;
	lightsys->InitVertexLight(vertexLight, lightLocationActor, zoneActor);
	vec3 eye = frame->ViewLocation.xyz();

	GouraudVertex vertices[3];
	for (const MeshTri& tri : mesh->Tris)
	{
		if (tri.TextureIndex >= mesh->Textures.size())
			continue;

		uint32_t renderflags = tri.PolyFlags | polyflags;
		UTexture* tex = engine->render->Mesh.textures[tri.TextureIndex];
		if (!tex || (renderflags & PF_Environment))
			tex = engine->render->Mesh.envmap;
		if (!tex)
			continue;
		renderflags |= tex->PolyFlags() & PF_Masked;

		bool isTranslucent = (renderflags & (PF_Translucent | PF_Modulated | PF_Highlighted)) != 0;
		if (isTranslucent && pass == MeshPass::Opaque)
		{
			needTranslucentPass = true;
			continue;
		}
		else if (!isTranslucent && pass == MeshPass::Translucent)
		{
			// We already drew the opaque surface
			continue;
		}

		TextureInfo texinfo;
		engine->render->UpdateTextureInfo(texinfo, tex);

		float uscale = (tex ? tex->UsedMipmaps.front().Width : 256) * (1.0f / 256.0f);
		float vscale = (tex ? tex->UsedMipmaps.front().Height : 256) * (1.0f / 256.0f);

		vec3 normals[3];
		for (int i = 0; i < 3; i++)
		{
			size_t vindex = tri.Indices[i];
			size_t vindex0 = vindex + vertexOffsets[0];
			size_t vindex1 = vindex + vertexOffsets[1];

			if (vindex0 >= mesh->Verts.size() || vindex1 >= mesh->Verts.size())
				return false; // out of bounds

			const vec3& v0 = mesh->Verts[vindex0];
			const vec3& v1 = mesh->Verts[vindex1];
			const vec3& n0 = mesh->Normals[vindex0];
			const vec3& n1 = mesh->Normals[vindex1];
			vec3 vertex = mix(v0 + n0 * fatness, v1 + n1 * fatness, t0);
			vec3 normal = mix(n0, n1, t0);
			if (t1 != 0.0f)
			{
				size_t vindex2 = vindex + vertexOffsets[2];
				if (vindex2 >= mesh->Verts.size())
					return false; // out of bounds

				const vec3& v2 = mesh->Verts[vindex2];
				const vec3& n2 = mesh->Normals[vindex2];
				vertex = mix(vertex, v2 + n2 * fatness, t1);
				normal = mix(normal, n2, t1);
			}
			vec3 restPose = mesh->Verts[vindex];
			for (int i = 0; i < blendCount; i++)
			{
				BlendInfo& b = blends[i];
				if (b.weight > 0.0f)
				{
					size_t blendVindex0 = vindex + b.offsets[0];
					size_t blendVindex1 = vindex + b.offsets[1];

					// A blend's frames from before the mesh changed (a script's
					// change of mesh mid-blend) lie past the vertices: left out, as
					// the LOD mesh's draw leaves them
					if (blendVindex0 >= mesh->Verts.size() || blendVindex1 >= mesh->Verts.size())
						continue;

					const vec3& bv0 = mesh->Verts[blendVindex0];
					const vec3& bv1 = mesh->Verts[blendVindex1];
					vec3 blendVertex = mix(bv0, bv1, b.t0);

					if (b.t1 != 0.0f)
					{
						size_t blendVindex2 = vindex + b.offsets[2];
						if (blendVindex2 >= mesh->Verts.size())
							continue;
						const vec3& bv2 = mesh->Verts[blendVindex2];
						blendVertex = mix(blendVertex, bv2, b.t1);
					}

					vertex += (blendVertex - restPose) * b.weight;
				}
			}

			vertices[i].Point = (ObjectToWorld * vec4(vertex, 1.0f)).xyz();
			vertices[i].UV = { tri.UV[i].x * uscale, tri.UV[i].y * vscale };
			normals[i] = normalize(ObjectNormalToWorld * normal);
		}

		// The original's DrawMesh culls a face that faces away only when its
		// own flags are PF_Flat without PF_TwoSided or PF_Invisible
		if ((tri.PolyFlags & (PF_Flat | PF_TwoSided | PF_Invisible)) == PF_Flat && !FacesEye(vertices, eye))
			continue;

		if (renderflags & PF_Environment)
		{
			// The original's (Render.dll 0x10b0bab0): the eye's line to the
			// vertex mirrored by its normal, in the world's axes.
			vec3 viewer = frame->ViewLocation.xyz();
			for (int i = 0; i < 3; i++)
			{
				vec3 p = reflect(normalize(vertices[i].Point - viewer), normals[i]);
				vertices[i].UV = { (p.x + 1.0f) * 128.0f * uscale, (p.y + 1.0f) * 128.0f * vscale };
			}
		}

		// No vertex fog: D3DDrv draws a mesh in a fog zone unfogged, the
		// game's UseVertexFog off (dx-reverse-info/d3ddrv-dll.md, fog)
		for (int i = 0; i < 3; i++)
		{
			vertices[i].Light = vertexLight.GetVertexLight(vertices[i].Point, normals[i], !!(renderflags & PF_Unlit), !!(renderflags & PF_TwoSided));
			vertices[i].Fog = vec4(0.0f);
		}

		frame->Device->DrawGouraudPolygon(&frame->Frame, texinfo, vertices, 3, renderflags);
	}
	return needTranslucentPass;
}

bool VisibleMesh::DrawLodMeshDX(VisibleFrame* frame, UActor* actor, UActor* lightLocationActor, ULodMesh* mesh, const mat4& ObjectToWorld, const mat3& ObjectNormalToWorld, MeshPass pass)
{
	UActor* animSource = actor;
	if (actor->bAnimByOwner() && actor->Owner())
		animSource = actor->Owner();

	const MeshAnimSeq* seq = SequenceOrFirstFrame(mesh, animSource->AnimSequence());
	float animFrame = animSource->AnimFrame() * seq->NumFrames;

	int vertexOffsets[3];
	float t0, t1;

	if (animFrame >= 0.0f)
	{
		int frame0, frame1;
		SequenceFrames(seq, animFrame, frame0, frame1, t0);
		t1 = 0.0f;
		vertexOffsets[0] = (seq->StartFrame + frame0) * mesh->FrameVerts;
		vertexOffsets[1] = (seq->StartFrame + frame1) * mesh->FrameVerts;
		vertexOffsets[2] = 0;
	}
	else // Tween from old animation
	{
		t0 = animSource->TweenFromAnimFrame.T;
		t1 = clamp(animFrame + 1.0f, 0.0f, 1.0f);
		vertexOffsets[0] = animSource->TweenFromAnimFrame.V0;
		vertexOffsets[1] = animSource->TweenFromAnimFrame.V1;
		vertexOffsets[2] = seq->StartFrame * mesh->FrameVerts;
	}

	BlendInfo blends[4];
	int blendCount = 0;

	for (int i = 0; i < 4; i++)
	{
		if (animSource->BlendAnimSequence()[i].IsNone())
			continue;

		MeshAnimSeq* seq = mesh->GetSequence(animSource->BlendAnimSequence()[i]);
		if (!seq || seq->Name != animSource->BlendAnimSequence()[i])
			continue;

		float frame = animSource->BlendAnimFrame()[i] * seq->NumFrames;

		BlendInfo& b = blends[blendCount++];

		if (frame >= 0.0f)
		{
			int f0, f1;
			SequenceFrames(seq, frame, f0, f1, b.t0);
			b.t1 = 0.0f;

			b.offsets[0] = (seq->StartFrame + f0) * mesh->FrameVerts;
			b.offsets[1] = (seq->StartFrame + f1) * mesh->FrameVerts;
			b.offsets[2] = 0;

			b.weight = 1.0f;
		}
		else // Tween from old blend state to new frame 0
		{
			float tweenFactor = clamp(frame + 1.0f, 0.0f, 1.0f);

			if (animSource->TweenFromBlendAnimFrame[i].T < 0.0f)
			{
				b.offsets[0] = seq->StartFrame * mesh->FrameVerts;
				b.offsets[1] = seq->StartFrame * mesh->FrameVerts;
				b.offsets[2] = seq->StartFrame * mesh->FrameVerts;
				b.t0 = 0.0f;
				b.t1 = 0.0f;
				b.weight = tweenFactor;
			}
			else
			{
				b.offsets[0] = animSource->TweenFromBlendAnimFrame[i].V0;
				b.offsets[1] = animSource->TweenFromBlendAnimFrame[i].V1;
				b.offsets[2] = seq->StartFrame * mesh->FrameVerts;
				b.t0 = animSource->TweenFromBlendAnimFrame[i].T;
				b.t1 = tweenFactor;
				b.weight = 1.0f;
			}
		}
	}

	SetupMeshTextures(actor, mesh);
	FindAttachmentPoints(mesh, ObjectToWorld, vertexOffsets, t0, t1);
	return DrawLodMeshFaceDX(frame, actor, lightLocationActor, mesh, mesh->Faces, ObjectToWorld, ObjectNormalToWorld, mesh->SpecialVerts, vertexOffsets, t0, t1, pass, blends, blendCount);
}

bool VisibleMesh::DrawLodMeshFaceDX(VisibleFrame* frame, UActor* actor, UActor* lightLocationActor, ULodMesh* mesh, const Array<MeshFace>& faces, const mat4& ObjectToWorld, const mat3& ObjectNormalToWorld, int baseVertexOffset, const int* vertexOffsets, float t0, float t1, MeshPass pass, BlendInfo* blends, int blendCount)
{
	auto lightsys = &engine->Level->Light;
	float fatness = actor->Fatness() / 16.0f - 8.0f;

	uint32_t polyFlags = 0;
	switch (actor->Style())
	{
	default: break;
	case STY_None: break;
	case STY_AlphaBlend: break;
	case STY_Masked: polyFlags |= PF_Masked; break;
	case STY_Translucent: polyFlags |= PF_Translucent; break;
	case STY_Modulated: polyFlags |= PF_Modulated; break;
	}
	if (actor->bNoSmooth()) polyFlags |= PF_NoSmooth;
	if (actor->bSelected()) polyFlags |= PF_Selected;
	if (actor->bMeshEnviroMap()) polyFlags |= PF_Environment;
	if (actor->bMeshCurvy()) polyFlags |= PF_Flat;
	if (actor->bNoSmooth()) polyFlags |= PF_NoSmooth;
	if (actor->bUnlit() || actor->Region().ZoneNumber == 0) polyFlags |= PF_Unlit;

	UZoneInfo* zoneActor = engine->GetZoneActor(actor->Region().ZoneNumber);

	VertexLight vertexLight;
	lightsys->InitVertexLight(vertexLight, lightLocationActor, zoneActor);
	vec3 eye = frame->ViewLocation.xyz();

	bool needTranslucentPass = false;

	// Faces share their vertices, and a vertex's animated position, normal,
	// light and fog depend only on the vertex this draw (the light also on
	// the face's unlit and two-sided flags). Each was computed again for
	// every face using it; now once a draw, by the same arithmetic, so the
	// results are the same to the bit.
	if (++vertexCacheGeneration == 0)
	{
		for (CachedMeshVertex& v : vertexCache)
			v.Generation = 0;
		vertexCacheGeneration = 1;
	}
	if (vertexCache.size() < (size_t)mesh->FrameVerts)
		vertexCache.resize(mesh->FrameVerts);
	const uint32_t generation = vertexCacheGeneration;

	// The vertex's position and normal in the world; false if the mesh's data is out of bounds
	auto animateVertex = [&](size_t vindex, vec3& point, vec3& worldNormal) -> bool
	{
		size_t vindex0 = vindex + vertexOffsets[0];
		size_t vindex1 = vindex + vertexOffsets[1];

		if (vindex0 >= mesh->Verts.size() || vindex1 >= mesh->Verts.size())
			return false; // out of bounds

		const vec3& v0 = mesh->Verts[vindex0];
		const vec3& v1 = mesh->Verts[vindex1];
		const vec3& n0 = mesh->Normals[vindex0];
		const vec3& n1 = mesh->Normals[vindex1];
		vec3 vertex = mix(v0 + n0 * fatness, v1 + n1 * fatness, t0);
		vec3 normal = mix(n0, n1, t0);
		if (t1 != 0.0f)
		{
			size_t vindex2 = vindex + vertexOffsets[2];
			if (vindex2 >= mesh->Verts.size())
				return false; // out of bounds

			const vec3& v2 = mesh->Verts[vindex2];
			const vec3& n2 = mesh->Normals[vindex2];
			vertex = mix(vertex, v2 + n2 * fatness, t1);
			normal = mix(normal, n2, t1);
		}
		vec3 restPose = mesh->Verts[vindex];
		vec3 restNormal = mesh->Normals[vindex];
		for (int j = 0; j < blendCount; j++)
		{
			BlendInfo& b = blends[j];
			if (b.weight > 0.0f)
			{
				size_t blendVindex0 = vindex + b.offsets[0];
				size_t blendVindex1 = vindex + b.offsets[1];

				if (blendVindex0 >= mesh->Verts.size() || blendVindex1 >= mesh->Verts.size())
					continue;

				const vec3& bv0 = mesh->Verts[blendVindex0];
				const vec3& bv1 = mesh->Verts[blendVindex1];
				const vec3& bn0 = mesh->Normals[blendVindex0];
				const vec3& bn1 = mesh->Normals[blendVindex1];

				vec3 blendVertex = mix(bv0, bv1, b.t0);
				vec3 blendNormal = mix(bn0, bn1, b.t0);

				if (b.t1 != 0.0f)
				{
					size_t blendVindex2 = vindex + b.offsets[2];
					if (blendVindex2 >= mesh->Verts.size())
						continue;

					const vec3& bv2 = mesh->Verts[blendVindex2];
					const vec3& bn2 = mesh->Normals[blendVindex2];
					blendVertex = mix(blendVertex, bv2, b.t1);
					blendNormal = mix(blendNormal, bn2, b.t1);
				}

				vertex += (blendVertex - restPose) * b.weight;
				normal += (blendNormal - restNormal) * b.weight;
			}
		}

		point = (ObjectToWorld * vec4(vertex, 1.0f)).xyz();
		worldNormal = normalize(ObjectNormalToWorld * normal);
		return true;
	};

	TextureInfo texinfo;
	UTexture* texinfoTexture = nullptr;

	// The original's vertex budget, worked out each draw
	// (dx-reverse-info/render-dll.md, mesh detail): it falls as one over the
	// actor's depth in the view, sooner for a complex mesh and a wide
	// view. Faces whose FaceLevel is past the budget go; each kept corner
	// walks its collapse list until its vertex is within it; and the
	// vertices past the whole number under (1 - LODMorph) of the unclamped
	// budget slide toward the vertex they collapse to, texture coordinates
	// with them, so detail fades rather than pops, beginning before the
	// first vertex goes -- but for one that collapses to the first vertex.
	// A mesh with LODStrength 0 or without the tables is drawn whole.
	bool lodActive = false;
	bool lodMorph = false;
	uint32_t lodBudget = 0;
	int lodMorphStart = 0;
	float lodMorphScale = 0.0f;
	if (&faces == &mesh->Faces && mesh->LODStrength > 0.0f && mesh->ModelVerts > 0 &&
		mesh->FaceLevel.size() >= mesh->Faces.size() &&
		mesh->CollapseWedgeThus.size() >= mesh->Wedges.size() &&
		mesh->CollapsePointThus.size() >= mesh->ModelVerts)
	{
		float depth = std::max((frame->Frame.WorldToView * vec4(actor->Location(), 1.0f)).z - mesh->LODZDisplace, 1.0f);
		float resolutionTerm = 0.3f + 0.7f * frame->Frame.FX / 640.0f;
		float complexityTerm = 0.25f + 0.003f * (float)mesh->ModelVerts;
		float shapeLOD = 0.25f;
		float factor = 430.0f * resolutionTerm * actor->DrawScale() * actor->LODBias() * mesh->MeshScaleMax /
			(mesh->LODStrength * shapeLOD * std::tan(radians(frame->Frame.FovAngle) * 0.5f) * depth * complexityTerm);
		// Held to the mesh's vertices as floats, before the conversions: a
		// budget past an int's range (a LODBias or DrawScale from script, a
		// field of view near 0) converted to 0 and INT_MIN on x86-64, so every
		// corner morphed fully to its target
		float budget = std::max((float)mesh->LODMinVerts, (float)mesh->ModelVerts * factor);
		lodBudget = (uint32_t)std::min(budget, (float)mesh->ModelVerts);
		lodMorphStart = (int)std::min((1.0f - mesh->LODMorph) * budget, (float)mesh->ModelVerts);
		lodMorph = mesh->LODMorph > 0.0f && lodMorphStart < (int)lodBudget;
		lodMorphScale = lodMorph ? 1.0f / (budget * mesh->LODMorph) : 0.0f;
		lodActive = true;
	}

	// Consecutive faces with one texture and one set of flags go to the
	// device in one call, which sets up the texture and pipeline once
	faceBatch.clear();
	uint32_t batchFlags = 0;
	auto drawBatch = [&]()
	{
		if (!faceBatch.empty())
		{
			frame->Device->DrawGouraudTriangles(&frame->Frame, texinfo, faceBatch.data(), (int)(faceBatch.size() / 3), batchFlags);
			faceBatch.clear();
		}
	};

	GouraudVertex vertices[3];
	size_t faceIndex = (size_t)-1;
	for (const MeshFace& face : faces)
	{
		faceIndex++;
		if (lodActive && mesh->FaceLevel[faceIndex] > lodBudget)
			continue;

		if (face.MaterialIndex >= mesh->Materials.size())
			continue;

		// A face naming a wedge past the mesh's (a hand-made mesh's) is passed
		// over, as the load passes it over for the normals
		if (face.Indices[0] >= mesh->Wedges.size() || face.Indices[1] >= mesh->Wedges.size() || face.Indices[2] >= mesh->Wedges.size())
			continue;

		const MeshMaterial& material = mesh->Materials[face.MaterialIndex];

		if (material.PolyFlags & PF_Invisible)
			continue;

		uint32_t renderflags = material.PolyFlags | polyFlags;
		// A slot past the mesh's textures (a hand-made mesh's material) holds
		// another mesh's: none, so the environment map stands in
		UTexture* tex = material.TextureIndex >= 0 && (size_t)material.TextureIndex < mesh->Textures.size() ? engine->render->Mesh.textures[material.TextureIndex] : nullptr;
		if (!tex || (renderflags & PF_Environment))
			tex = engine->render->Mesh.envmap;

		// skip if no texture
		if (!tex)
			continue;
		renderflags |= tex->PolyFlags() & PF_Masked;

		bool isTranslucent = (renderflags & (PF_Translucent | PF_Modulated | PF_Highlighted)) != 0;
		if (isTranslucent && pass == MeshPass::Opaque)
		{
			needTranslucentPass = true;
			continue;
		}
		else if (!isTranslucent && pass == MeshPass::Translucent)
		{
			// We already drew the opaque surface
			continue;
		}

		uint32_t drawFlags = renderflags;
		if (tex != texinfoTexture || drawFlags != batchFlags)
			drawBatch();

		// The texture's info once for all its faces: its modified flag stays
		// set until the device takes it with the first batch drawn.
		if (tex != texinfoTexture)
		{
			texinfo = {};
			engine->render->UpdateTextureInfo(texinfo, tex);
			texinfoTexture = tex;
		}

		float uscale = (texinfo.Texture ? texinfo.Texture->UsedMipmaps.front().Width : 256) * (1.0f / 256.0f);
		float vscale = (texinfo.Texture ? texinfo.Texture->UsedMipmaps.front().Height : 256) * (1.0f / 256.0f);

		bool unlit = !!(renderflags & PF_Unlit);
		bool twosided = !!(renderflags & PF_TwoSided);
		uint32_t lightKey = 1 | (unlit ? 2 : 0) | (twosided ? 4 : 0);

		vec3 normals[3];
		CachedMeshVertex* cached[3] = {};
		for (int i = 0; i < 3; i++)
		{
			// A corner past the budget walks down its collapse list
			uint16_t wedgeIndex = face.Indices[i];
			if (lodActive)
			{
				size_t steps = 0;
				while (mesh->Wedges[wedgeIndex].Vertex >= lodBudget && steps++ < mesh->Wedges.size())
				{
					// A list leading past the wedges (a hand-made mesh's) ends here
					uint16_t next = mesh->CollapseWedgeThus[wedgeIndex];
					if (next >= mesh->Wedges.size())
						break;
					wedgeIndex = next;
				}
			}
			const MeshWedge& wedge = mesh->Wedges[wedgeIndex];

			size_t vbase = (size_t)wedge.Vertex + baseVertexOffset;
			size_t vindex = mesh->ReMapAnimVerts.empty() || vbase >= mesh->ReMapAnimVerts.size() ? vbase : mesh->ReMapAnimVerts[vbase];

			if (vindex < vertexCache.size())
			{
				CachedMeshVertex& c = vertexCache[vindex];
				if (c.Generation != generation)
				{
					if (!animateVertex(vindex, c.Point, c.Normal))
					{
						drawBatch();
						return false;
					}
					c.Generation = generation;
					c.LightKey = 0;
				}
				vertices[i].Point = c.Point;
				normals[i] = c.Normal;
				cached[i] = &c;
			}
			else if (!animateVertex(vindex, vertices[i].Point, normals[i]))
			{
				drawBatch();
				return false;
			}
			vertices[i].UV = { wedge.U * uscale, wedge.V * vscale };

			// The top of the budget slides toward what it collapses to; the
			// texture coordinates as bytes, rounded down
			uint16_t morphTarget = lodMorph && (int)wedge.Vertex > lodMorphStart && wedge.Vertex < mesh->CollapsePointThus.size() ? mesh->CollapsePointThus[wedge.Vertex] : 0;
			if (morphTarget != 0)
			{
				float morphT = std::min((float)((int)wedge.Vertex - lodMorphStart) * lodMorphScale, 1.0f);
				size_t tbase = (size_t)morphTarget + baseVertexOffset;
				size_t tindex = mesh->ReMapAnimVerts.empty() ? tbase : (tbase < mesh->ReMapAnimVerts.size() ? mesh->ReMapAnimVerts[tbase] : tbase);
				vec3 targetPoint, targetNormal;
				if (animateVertex(tindex, targetPoint, targetNormal))
					vertices[i].Point = mix(vertices[i].Point, targetPoint, morphT);
				uint16_t targetWedgeIndex = mesh->CollapseWedgeThus[wedgeIndex];
				if (targetWedgeIndex != wedgeIndex && targetWedgeIndex < mesh->Wedges.size())
				{
					const MeshWedge& targetWedge = mesh->Wedges[targetWedgeIndex];
					if (targetWedge.U != wedge.U || targetWedge.V != wedge.V)
					{
						uint8_t u = (uint8_t)(int)(((double)targetWedge.U - (double)wedge.U) * morphT + (double)wedge.U);
						uint8_t v = (uint8_t)(int)(((double)targetWedge.V - (double)wedge.V) * morphT + (double)wedge.V);
						vertices[i].UV = { u * uscale, v * vscale };
					}
				}
			}
		}

		// The original's DrawLodMesh draws a face that faces away only when
		// the actor's or its material's flags make it two-sided
		if (!twosided && !FacesEye(vertices, eye))
			continue;

		if (renderflags & PF_Environment)
		{
			// The original's (Render.dll 0x10b0bab0): the eye's line to the
			// vertex mirrored by its normal, in the world's axes.
			vec3 viewer = frame->ViewLocation.xyz();
			for (int i = 0; i < 3; i++)
			{
				vec3 p = reflect(normalize(vertices[i].Point - viewer), normals[i]);
				vertices[i].UV = { (p.x + 1.0f) * 128.0f * uscale, (p.y + 1.0f) * 128.0f * vscale };
			}
		}

		for (int i = 0; i < 3; i++)
		{
			CachedMeshVertex* c = cached[i];
			// No vertex fog, as DrawMeshDX's
			vertices[i].Fog = vec4(0.0f);
			if (c && c->LightKey == lightKey)
			{
				vertices[i].Light = c->Light;
			}
			else
			{
				vertices[i].Light = vertexLight.GetVertexLight(vertices[i].Point, normals[i], unlit, twosided);
				if (c)
				{
					c->Light = vertices[i].Light;
					c->LightKey = lightKey;
				}
			}
		}

		faceBatch.push_back(vertices[0]);
		faceBatch.push_back(vertices[1]);
		faceBatch.push_back(vertices[2]);
		batchFlags = drawFlags;
	}
	drawBatch();

	return needTranslucentPass;
}

void VisibleMesh::DrawDebugInfo(VisibleFrame* frame, UActor* actor)
{
	auto pawn = UObject::TryCast<UPawn>(actor);
	if (!pawn || !pawn->StateFrame)
		return;

	// Draw debug line to where pawn is trying to go
	{
		vec3 end;
		if (pawn->StateFrame->LatentState == LatentRunState::MoveTo)
		{
			end = pawn->Destination();
		}
		else if (pawn->StateFrame->LatentState == LatentRunState::MoveToward && pawn->MoveTarget())
		{
			end = pawn->MoveTarget()->Location();
		}
		else if (pawn->StateFrame->LatentState == LatentRunState::StrafeTo)
		{
			end = pawn->Destination();
		}
		else if (pawn->StateFrame->LatentState == LatentRunState::StrafeFacing)
		{
			end = pawn->Destination();
		}
		else
		{
			return;
		}

		vec3 start = pawn->Location();
		start.z += pawn->BaseEyeHeight();
		end.z += pawn->BaseEyeHeight();

		bool visible = pawn->FastTrace(end, start);
		engine->render->Device->Draw3DLine(
			&frame->Frame,
			visible ? vec4(1.0f, 1.0f, 1.0f, 1.0f) : vec4(1.0f, 0.0f, 0.0f, 1.0f),
			0,
			start,
			end);
	}

	// Draw desired rotation:
	{
		vec3 start = pawn->Location();
		start.z += pawn->BaseEyeHeight() * 0.5f;
		vec3 end = start + (Coords::Rotation(pawn->DesiredRotation()).ToMatrix() * vec4(1.0f, 0.0f, 0.0f, 1.0f)).xyz() * 100.0f;
		engine->render->Device->Draw3DLine(
			&frame->Frame,
			vec4(0.6f, 0.6f, 1.0f, 1.0f),
			0,
			start,
			end);

		end = pawn->Focus();
		engine->render->Device->Draw3DLine(
			&frame->Frame,
			vec4(0.6f, 1.0f, 0.6f, 1.0f),
			0,
			start,
			end);
	}

	// Draw last calculated path for the bot
	vec3 start = pawn->Location();
	start.z += pawn->BaseEyeHeight();

	for (UNavigationPoint* p : pawn->RouteCache())
	{
		if (!p)
			break;
		vec3 end = p->Location();
		end.z += pawn->BaseEyeHeight();
		engine->render->Device->Draw3DLine(
			&frame->Frame,
			vec4(1.0f, 1.0f, 0.0f, 1.0f),
			0,
			start,
			end);
		start = end;
	}
}
