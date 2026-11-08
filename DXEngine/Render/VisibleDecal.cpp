
#include "Precomp.h"
#include "VisibleDecal.h"
#include "VisibleFrame.h"
#include "RenderSubsystem.h"
#include "RenderDevice/RenderDevice.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/UPalette.h"
#include "Packages/Engine/Actors/UDecal.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Engine.h"
#include "Packages/Engine/UDXEngineClient.h"

void VisibleDecal::DrawDecals(VisibleFrame* frame, BspNode* node)
{
	// Deus Ex draws decals only while the client's Decals is on (the
	// Display menu's Decals; Render.dll's DrawFrame), and leaves them
	// unstamped when off.
	if (!engine->client->Decals)
		return;

	for (auto& leveldecal : node->Decals)
	{
		// One whose texture is None drawn with the level's DefaultTexture, as
		// the original's DrawFrame draws it (Render.dll 0x10b1b154)
		UTexture* decalTexture = leveldecal.Decal->Texture();
		if (!decalTexture)
			decalTexture = engine->LevelInfo->DefaultTexture();
		if (decalTexture)
		{
			// Its LastRenderedTime is stamped in the walk, when it shows
			// (VisibleFrame).
			engine->render->UpdateTexture(decalTexture);

			UTexture* texture = decalTexture->GetAnimTexture();
			// The viewer's Sprite, the Matrix cheat's, in its place
			if (UTexture* sprite = engine->render->ViewerSprite())
			{
				engine->render->UpdateTexture(sprite);
				texture = sprite->GetAnimTexture();
			}
			engine->render->UpdateTexture(texture);

			TextureInfo texinfo;
			texinfo.CacheID = (uint64_t)(ptrdiff_t)texture;
			texinfo.Texture = texture;
			texinfo.Format = texinfo.Texture->UsedFormat;
			texinfo.Mips = texinfo.Texture->UsedMipmaps.data();
			texinfo.NumMips = (int)texinfo.Texture->UsedMipmaps.size();
			texinfo.USize = texinfo.Texture->USize();
			texinfo.VSize = texinfo.Texture->VSize();
			if (texinfo.Texture->Palette())
				texinfo.Palette = (TextureColor*)texinfo.Texture->Palette()->Colors.data();
			engine->render->ApplyDetailLOD(texinfo);

			vec3 depthOffset = -frame->ViewRotation.XAxis;

			int count = (int)leveldecal.Positions.size();
			GouraudVertex* points = engine->render->GetTempGouraudVertexBuffer(count);
			for (int i = 0; i < count; i++)
			{
				points[i].Light = vec3(1.0f);
				points[i].Point = leveldecal.Positions[i] + depthOffset;
				points[i].UV = leveldecal.UVs[i];
			}

			frame->Device->DrawGouraudPolygon(&frame->Frame, texinfo, points, count, PF_Modulated);
		}
	}
}
