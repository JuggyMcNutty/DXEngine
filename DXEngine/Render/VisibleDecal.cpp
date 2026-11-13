
#include "Precomp.h"
#include "VisibleDecal.h"
#include "VisibleFrame.h"
#include "RenderSubsystem.h"
#include "RenderDevice/RenderDevice.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/Level/UPolys.h"
#include "Packages/Engine/Resources/UPalette.h"
#include "Packages/Engine/Actors/UDecal.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Collision/TopLevel/CollisionSystem.h"
#include "Math/coords.h"
#include "Engine.h"
#include "Packages/Engine/UDXEngineClient.h"

// A decal as the original's ClipDecal draws it on one of its surface's
// polygons (Render.dll 0x10b1bda0): its corners on the surface's base --
// turned on a mover's polygon by the mover's turn since it was laid, each of
// its rotation's three numbers less the one it was laid at (Deus Ex's
// MoverRotation) -- lifted 0.75 units along the surface's unit normal
// (0x10b2b304), then cut by the planes through the eye and each edge of the
// polygon, so that on screen it fits the polygon's outline. Its texture runs
// along its first edge and its last from its first corner, over its draw
// scale as it is drawn.
bool VisibleDecal::Clip(const vec3& eye, const LevelDecal& decal, const vec3& base, const vec3& normal, UMover* mover, const Array<vec3>& poly, Array<vec3>& points, Array<vec2>* uvs)
{
	points.clear();
	if (uvs)
		uvs->clear();
	if (poly.size() < 3)
		return false;

	vec3 corners[4];
	for (int k = 0; k < 4; k++)
		corners[k] = decal.Vertices[k];
	if (mover)
	{
		mat4 turn = Coords::Rotation(mover->Rotation() - decal.MoverRotation).ToMatrix();
		for (int k = 0; k < 4; k++)
			corners[k] = (turn * vec4(corners[k], 0.0f)).xyz();
	}

	vec3 lift = normal;
	float sq = dot(lift, lift);
	if (sq >= 1e-8f)
		lift = lift * (float)(1.0 / std::sqrt((double)sq));
	lift = lift * 0.75f;
	for (int k = 0; k < 4; k++)
		points.push_back((corners[k] + base) + lift);

	// Each edge's plane through the eye, the side its polygon lies on kept,
	// a point put in where an edge of the decal crosses it
	Array<float> dist;
	vec3 prev = poly.back();
	for (const vec3& cur : poly)
	{
		vec3 n = cross(prev - cur, eye - cur);
		float w = dot(cur, n);
		dist.clear();
		for (const vec3& p : points)
			dist.push_back(dot(p, n) - w);
		for (size_t i = 0; i < points.size(); i++)
		{
			size_t next = (i + 1) % points.size();
			if ((dist[i] > 0.0f && dist[next] < 0.0f) || (dist[i] < 0.0f && dist[next] > 0.0f))
			{
				vec3 p = points[i] + (points[next] - points[i]) * (dist[i] / (dist[i] - dist[next]));
				points.insert(points.begin() + i + 1, p);
				dist.insert(dist.begin() + i + 1, 0.0f);
				i++;
			}
		}
		size_t kept = 0;
		for (size_t i = 0; i < points.size(); i++)
		{
			if (dist[i] >= 0.0f)
				points[kept++] = points[i];
		}
		points.resize(kept);
		if (points.empty())
			return false;
		prev = cur;
	}

	if (uvs)
	{
		// A decal of no size, all its corners at one point, draws nothing
		vec3 u = corners[1] - corners[0];
		vec3 v = corners[3] - corners[0];
		float uu = dot(u, u), vv = dot(v, v);
		float scale = decal.Decal->DrawScale();
		if (!(uu > 0.0f && vv > 0.0f && scale != 0.0f))
			return false;
		u = u * (float)(1.0 / std::sqrt((double)uu));
		v = v * (float)(1.0 / std::sqrt((double)vv));
		for (const vec3& p : points)
		{
			vec3 offset = (p - base) - corners[0];
			uvs->push_back(vec2(dot(offset, u) / scale, dot(offset, v) / scale));
		}
	}
	return true;
}

// The original stamps a decal's LastRenderedTime as it draws it, where
// ClipDecal leaves some of it on a polygon of its surface the walk saved, the
// polygon of a node any span of which shows, cut to the view's sides: no
// span buffer is asked of the decal itself, so one behind nearer geometry,
// on a surface that shows elsewhere, is stamped and hidden by the depth
// buffer (DrawFrame's loop, Render.dll 0x10b1b0e6-0x10b1b2f0). The walk calls
// this where it finds such a node.
void VisibleDecal::StampDecals(VisibleFrame* frame, const Array<LevelDecal>& decals, const vec3& base, const vec3& normal, UMover* mover, const vec3* points, int count)
{
	if (decals.empty() || !engine->client->Decals)
		return;
	static Array<vec3> poly;
	static Array<vec3> clipped;
	frame->ViewPolygon(points, count, poly);
	vec3 eye = frame->ViewLocation.xyz();
	float now = engine->LevelInfo->TimeSeconds();
	for (const LevelDecal& decal : decals)
	{
		if (Clip(eye, decal, base, normal, mover, poly, clipped, nullptr))
			decal.Decal->LastRenderedTime() = now;
	}
}

void VisibleDecal::DrawDecals(VisibleFrame* frame, BspNode* node, const vec3* points, int count)
{
	if (node->Decals.empty() || node->Surf < 0)
		return;
	UModel* model = engine->Level->Model;
	const BspSurface& surface = model->Surfaces[node->Surf];
	Draw(frame, node->Decals, model->Points[surface.pBase], model->Vectors[surface.vNormal], nullptr, points, count);
}

void VisibleDecal::DrawMoverDecals(VisibleFrame* frame, UMover* mover, int surf, const vec3* points, int count)
{
	Array<LevelDecal>* decals = engine->Level->Collision.FiledDecals(surf);
	vec3 base, normal;
	if (!decals || decals->empty() || !UDecal::SurfaceFrame(engine->Level, surf, base, normal))
		return;
	Draw(frame, *decals, base, normal, mover, points, count);
}

void VisibleDecal::Draw(VisibleFrame* frame, const Array<LevelDecal>& decals, const vec3& base, const vec3& normal, UMover* mover, const vec3* points, int count)
{
	// Deus Ex draws decals only while the client's Decals is on (the
	// Display menu's Decals; Render.dll's DrawFrame), and leaves them
	// unstamped when off.
	if (!engine->client->Decals)
		return;

	// The decals drawn and clipped, timed and counted for STAT GLOBAL's
	// Decal, DecalClip and Decal#, as the original's DrawFrame does
	// (Render.dll 0x10b1b0ac-0x10b1b382)
	auto& panel = engine->render->PanelStats;
	StatClock clock(panel.Decal);
	static Array<vec3> poly;
	static Array<vec3> clipped;
	static Array<vec2> uvs;
	frame->ViewPolygon(points, count, poly);
	vec3 eye = frame->ViewLocation.xyz();

	for (const LevelDecal& leveldecal : decals)
	{
		bool kept;
		{
			StatClock clipClock(panel.DecalClip);
			kept = Clip(eye, leveldecal, base, normal, mover, poly, clipped, &uvs);
		}
		if (!kept)
			continue;

		// Its texture, the level's default one where it has none, as the
		// original's DrawFrame takes it (0x10b1b154); its LastRenderedTime
		// is stamped in the walk (StampDecals)
		UTexture* texture = leveldecal.Decal->Texture();
		if (!texture)
			texture = engine->LevelInfo->DefaultTexture();
		if (!texture)
			continue;
		engine->render->UpdateTexture(texture);
		texture = texture->GetAnimTexture();
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

		int pointCount = (int)clipped.size();
		GouraudVertex* vertices = engine->render->GetTempGouraudVertexBuffer(pointCount);
		for (int i = 0; i < pointCount; i++)
		{
			vertices[i].Light = vec3(1.0f);
			vertices[i].Point = clipped[i];
			vertices[i].UV = uvs[i];
		}

		frame->Device->DrawGouraudPolygon(&frame->Frame, texinfo, vertices, pointCount, PF_Modulated);
		panel.Decals++;
	}
}
