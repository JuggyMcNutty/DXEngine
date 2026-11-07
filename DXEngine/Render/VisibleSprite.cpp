
#include "Precomp.h"
#include "VisibleSprite.h"
#include "Packages/Core/UClass.h"
#include "Packages/Engine/Resources/UPalette.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/UViewport.h"
#include "Engine.h"
#include "RenderSubsystem.h"

void VisibleSprite::Draw(VisibleFrame* frame, UActor* actor)
{
	Draw(frame, actor, actor->Location(), actor->DrawScale(), actor->ScaleGlow());
}

void VisibleSprite::Draw(VisibleFrame* frame, UActor* actor, const vec3& location, float drawscale, float scaleglow)
{
	UTexture* texture = actor->Texture();
	int style = actor->Style();
	bool noSmooth = actor->bNoSmooth();

	engine->render->UpdateTexture(texture);

	if (actor->DrawType() == DT_SpriteAnimOnce)
	{
		// The frame the actor's life has reached, held to the last, as the
		// original's DrawActorSprite holds it (render-dll.md, sprites): a life
		// of 0, forever, showed frame 0 again, or nothing past a chain's end;
		// a class default of 0 (0/0) is frame 0
		float defaultLife = actor->Class->GetDefaultObject<UActor>()->LifeSpan();
		float t = defaultLife > 0.0f ? clamp(1.0f - actor->LifeSpan() / defaultLife, 0.0f, 1.0f) : 0.0f;
		int count = std::max(texture->GetAnimTextureCount(), 1);
		int index = t > 0.0f ? std::min((int)std::floor(t * count), count - 1) : 0;
		for (int i = 0; i < index && texture; i++)
			texture = texture->AnimNext();
	}
	else
	{
		texture = texture->GetAnimTexture();
	}
	if (!texture || texture->UsedMipmaps.empty())
		return;

	// The viewer's Sprite, when it has one (Deus Ex's Matrix easter egg),
	// stands in for the texture, drawn at the actor's texture's size, as
	// the original's DrawActorSprite has it and SetupMeshTextures does for
	// a mesh
	float texwidth = (float)texture->UsedMipmaps.front().Width;
	float texheight = (float)texture->UsedMipmaps.front().Height;
	if (UPlayerPawn* viewer = UObject::TryCast<UPlayerPawn>(engine->viewport->Actor()))
	{
		if (viewer->Sprite() && !viewer->Sprite()->UsedMipmaps.empty())
			texture = viewer->Sprite();
	}

	engine->render->UpdateTexture(texture);

	TextureInfo texinfo;
	texinfo.Texture = texture;
	texinfo.CacheID = (uint64_t)(ptrdiff_t)texinfo.Texture;
	texinfo.bRealtimeChanged = texture->TakeChange();
	texinfo.Format = texinfo.Texture->UsedFormat;
	texinfo.Mips = texinfo.Texture->UsedMipmaps.data();
	texinfo.NumMips = (int)texinfo.Texture->UsedMipmaps.size();
	texinfo.USize = texinfo.Texture->USize();
	texinfo.VSize = texinfo.Texture->VSize();
	if (texinfo.Texture->Palette())
		texinfo.Palette = (TextureColor*)texinfo.Texture->Palette()->Colors.data();
	engine->render->ApplyDetailLOD(texinfo);

	// The actor's style's flags, as the original's DrawActorSprite takes
	// them (Render.dll 0x10b26230): masked, translucent, modulated.
	uint32_t renderflags = PF_TwoSided;
	if (style == 2)
		renderflags |= PF_Masked;
	else if (style == 3)
		renderflags |= PF_Translucent;
	else if (style == 4)
		renderflags |= PF_Modulated;
	if (noSmooth)
		renderflags |= PF_NoSmooth;
	if (texture->bMasked())
		renderflags |= PF_Masked;

	renderflags |= texture->PolyFlags();

	// As D3DDrv draws the original's sprite, a tile (RenderDevice::TileFlags)
	renderflags = RenderDevice::TileFlags(texinfo, renderflags);

	if (renderflags & PF_Invisible)
		return;

	drawscale *= 0.5f;

	vec3 sideAxis = frame->ViewRotation.YAxis * (texwidth * drawscale);
	vec3 upAxis = frame->ViewRotation.ZAxis * (texheight * drawscale);

	vec3 color = clamp(scaleglow, 0.0f, 1.0f);

	// Upright, as the original's tile: the texture's first row at the top,
	// and where a tile lies, half a pixel off the frame's polygons
	vec3 center = location + frame->TileOffset(location);
	GouraudVertex vertices[4];
	vertices[0].Point = center - sideAxis - upAxis;
	vertices[0].UV = { 0.0f, texheight };
	vertices[0].Light = color;
	vertices[0].Fog = { 0.0f };
	vertices[1].Point = center + sideAxis - upAxis;
	vertices[1].UV = { texwidth, texheight };
	vertices[1].Light = color;
	vertices[1].Fog = { 0.0f };
	vertices[2].Point = center + sideAxis + upAxis;
	vertices[2].UV = { texwidth, 0.0f };
	vertices[2].Light = color;
	vertices[2].Fog = { 0.0f };
	vertices[3].Point = center - sideAxis + upAxis;
	vertices[3].UV = { 0.0f, 0.0f };
	vertices[3].Light = color;
	vertices[3].Fog = { 0.0f };

	frame->Device->DrawGouraudPolygon(&frame->Frame, texinfo, vertices, 4, renderflags);

	engine->render->Stats.Actors++;
}
