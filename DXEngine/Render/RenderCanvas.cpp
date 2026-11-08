
#include "Precomp.h"
#include "RenderSubsystem.h"
#include "VisibleMesh.h"
#include "RenderDevice/RenderDevice.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/Inventory/UWeapon.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/UCanvas.h"
#include "Packages/Engine/UConsole.h"
#include "Packages/Engine/Resources/UFont.h"
#include "Packages/Engine/Resources/UPalette.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "GameWindow.h"
#include "VM/ScriptCall.h"
#include "Engine.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Math/floating.h"
#include <chrono>

void RenderSubsystem::ResetCanvas()
{
	// The canvas is the viewport, pixel for pixel, as the original's: the
	// console and the level's big messages drawn at their fonts' own size
	SceneNode frame;
	Canvas.Frame.XB = 0;
	Canvas.Frame.YB = 0;
	Canvas.Frame.X = engine->viewport->ViewportWidth();
	Canvas.Frame.Y = engine->viewport->ViewportHeight();
	Canvas.Frame.FX = (float)engine->viewport->ViewportWidth();
	Canvas.Frame.FY = (float)engine->viewport->ViewportHeight();
	Canvas.Frame.FX2 = Canvas.Frame.FX * 0.5f;
	Canvas.Frame.FY2 = Canvas.Frame.FY * 0.5f;
	Canvas.Frame.ObjectToWorld = mat4::identity();
	Canvas.Frame.WorldToView = mat4::identity();
	Canvas.Frame.FovAngle = HorPlusFovAngle(engine->CameraFovAngle, Canvas.Frame.FX, Canvas.Frame.FY);
	float Aspect = Canvas.Frame.FY / Canvas.Frame.FX;
	float RProjZ = (float)std::tan(radians(Canvas.Frame.FovAngle) * 0.5f);
	Canvas.Frame.Projection = mat4::frustum(-RProjZ, RProjZ, -Aspect * RProjZ, Aspect * RProjZ, 1.0f, 32768.0f, handedness::left, clipzrange::zero_positive_w);

	int sizeX = engine->viewport->ViewportWidth();
	int sizeY = engine->viewport->ViewportHeight();
	engine->canvas->CurX() = 0.0f;
	engine->canvas->CurY() = 0.0f;
	engine->console->FrameX() = (float)sizeX;
	engine->console->FrameY() = (float)sizeY;
	engine->canvas->ClipX() = (float)sizeX;
	engine->canvas->ClipY() = (float)sizeY;
	engine->canvas->SizeX() = sizeX;
	engine->canvas->SizeY() = sizeY;

	//engine->viewport->bShowWindowsMouse() = true; // bShowWindowsMouse is set to true by WindowConsole if mouse cursor should be visible
	//engine->viewport->bWindowsMouseAvailable() = true; // if true then RenderUWindow updates mouse pos from (WindowsMouseX,WindowsMouseY), otherwise it uses KeyEvent(IK_MouseX, delta) + KeyEvent(IK_MouseY, delta). Maybe used for windowed mode?
	//engine->viewport->WindowsMouseX() = 10.0f;
	//engine->viewport->WindowsMouseY() = 200.0f;
	CallEvent(engine->canvas, EventName::Reset);
}

void RenderSubsystem::PreRender()
{
	Device->SetSceneNode(&Canvas.Frame);
	CallEvent(engine->console, EventName::PreRender, { ExpressionValue::ObjectValue(engine->canvas) });
	if (engine->viewport->Actor())
		CallEvent(engine->viewport->Actor(), EventName::PreRender, { ExpressionValue::ObjectValue(engine->canvas) });
}

// The original's DrawWorld after the world (Render.dll 0x10b1cb90): the
// depth buffer cleared, then RenderOverlays sent to the player's view
// target, the player with none, and to nothing in behind view or with the
// actors hidden (ShowFlags' SHOW_Actors). So the first-person weapon meets
// no depth of the world: a rifle held against a wall draws whole
// (dx-reverse-info render-dll.md, the overlays).
void RenderSubsystem::RenderOverlays()
{
	Device->SetSceneNode(&Canvas.Frame);
	const int SHOW_Actors = 8;
	UPlayerPawn* player = engine->viewport->Actor();
	if (player && !player->bBehindView() && (player->ShowFlags() & SHOW_Actors))
	{
		Device->ClearZ();
		UActor* target = player->ViewTarget() ? player->ViewTarget() : player;
		CallEvent(target, EventName::RenderOverlays, { ExpressionValue::ObjectValue(engine->canvas) });
	}
}

void RenderSubsystem::PostRender()
{
	Device->SetSceneNode(&Canvas.Frame);
	if (engine->viewport->Actor())
		CallEvent(engine->viewport->Actor(), EventName::PostRender, { ExpressionValue::ObjectValue(engine->canvas) });
	CallEvent(engine->console, EventName::PostRender, { ExpressionValue::ObjectValue(engine->canvas) });
	// The original renderer's statistics, after the console's PostRender,
	// with the viewer's RendMap 3 to 6 (URender::PostRender, Render.dll
	// 0x10b14700)
	UPlayerPawn* viewer = engine->viewport->Actor();
	if (viewer && viewer->RendMap() >= 3 && viewer->RendMap() <= 6)
		DrawStats();
	DrawTimedemoStats();
}

// The original's DrawStats (Render.dll 0x10b13a50): the frame's end stamped
// and the last 32 frames' times taken in, then STAT FPS's line, white in the
// canvas's small font, centred 10 pixels over the view's bottom: the frame
// rate, the time between frames' ends and the render's from its start, each
// the mean of the last 32 frames, the BSP nodes and polygons drawn (each a
// surface drawn, OccludeBsp 0x10b197db-0x10b197f4 counting both at once),
// the textures the frame used and their size in K (bugs.md WIN.29). The
// other panels -- Global, Net, Hardware, Mesh, Zone, Occlusion, Game, Cache
// -- list the original's own timings, connection counts and caches, which
// DXEngine does not keep so: their words toggle, drawing nothing.
void RenderSubsystem::DrawStats()
{
	double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
	FrameStats.EndTime = now;
	FrameStats.Index = (FrameStats.Index + 1) % 32;
	FrameStats.FrameTimes[FrameStats.Index] = now - FrameStats.LastEndTime;
	FrameStats.RenderTimes[FrameStats.Index] = now - FrameStats.StartTime;
	if (!StatPanels[StatFps])
		return;
	double frame = 0.0, rendered = 0.0;
	for (int i = 0; i < 32; i++)
	{
		frame += FrameStats.FrameTimes[i];
		rendered += FrameStats.RenderTimes[i];
	}
	frame /= 32.0;
	rendered /= 32.0;
	UFont* font = engine->canvas->SmallFont();
	if (!font)
		return;
	char text[256];
	std::snprintf(text, sizeof(text), "FPS=%05.1f Frame=%05.1f MSEC Render=%05.1f MSEC Nodes=%03i Polys=%03i Tex=%03i TexMem=%06.1fK",
		frame > 0.0 ? 1.0 / frame : 0.0, frame * 1000.0, rendered * 1000.0, Stats.Surfaces, Stats.Surfaces, FrameStats.Textures, FrameStats.TextureMem / 1024.0);
	float curX = 0.0f;
	float curY = (float)engine->canvas->SizeY() - 10.0f;
	float curXL = 0.0f, curYL = 0.0f;
	DrawText(font, vec4(1.0f), 0.0f, 0.0f, curX, curY, curXL, curYL, false, UTF8ToScript(text), PF_NoSmooth | PF_Masked, true, 0.0f, 0.0f, engine->canvas->ClipX());
}

void RenderSubsystem::PostRenderFlash()
{
	Device->SetSceneNode(&Canvas.Frame);
	if (engine->viewport->Actor())
		CallEvent(engine->viewport->Actor(), "PostRenderFlash", {ExpressionValue::ObjectValue(engine->canvas)});
}

void RenderSubsystem::DrawActor(UActor* actor, bool ClearZ, bool hideAfter, const Rectf* constrain, bool wire)
{
	// Constrained, the scene's frame narrows to where it meets the rectangle,
	// its projection made again for that size about its middle -- the same
	// angle over the narrower width -- as the original's XGC::DrawActor does
	// for bConstrain (Extension.dll 0x1002aa60, FSceneNode::ComputeRenderSize;
	// extension-dll.md, actors in a window), and is put back after. The scene
	// is the main view's, or a viewport window's while its overlays draw.
	VisibleFrame& sceneFrame = *CanvasScene;
	SceneNode scene = sceneFrame.Frame;
	if (constrain)
	{
		float left = std::max((float)scene.XB, constrain->left);
		float top = std::max((float)scene.YB, constrain->top);
		float right = std::min((float)(scene.XB + scene.X), constrain->right);
		float bottom = std::min((float)(scene.YB + scene.Y), constrain->bottom);
		if (right <= left || bottom <= top)
			return;
		SceneNode& frame = sceneFrame.Frame;
		frame.XB = (int)(left - scene.XB);
		frame.YB = (int)(top - scene.YB);
		frame.X = (int)(right - left);
		frame.Y = (int)(bottom - top);
		if (frame.X <= 0 || frame.Y <= 0)
		{
			frame = scene;
			return;
		}
		frame.FX = (float)frame.X;
		frame.FY = (float)frame.Y;
		frame.FX2 = frame.FX * 0.5f;
		frame.FY2 = frame.FY * 0.5f;
		frame.FovAngle = HorPlusFovAngle(engine->CameraFovAngle, frame.FX, frame.FY);
		float Aspect = frame.FY / frame.FX;
		float RProjZ = (float)std::tan(radians(frame.FovAngle) * 0.5f);
		frame.Projection = mat4::frustum(-RProjZ, RProjZ, -Aspect * RProjZ, Aspect * RProjZ, 1.0f, 32768.0f, handedness::left, clipzrange::zero_positive_w);
	}

	Device->SetSceneNode(&sceneFrame.Frame);
	if (ClearZ)
		Device->ClearZ();

	// Deus Ex's renderer stamps an actor drawn on its own as drawn, as one
	// the scene draws (Render.dll URender::DrawActor 0x10b262c0): whenever
	// its sprite has a rectangle on the scene's frame, whatever is in
	// front of it -- an NPC the vision augmentation draws through a wall
	// counts as drawn. GC.DrawActor and Canvas.DrawActor both come here.
	float minX, minY, maxX, maxY, z;
	if (sceneFrame.SpriteRectangle(actor, minX, minY, maxX, maxY, z))
		actor->LastRenderTime() = engine->LevelInfo->TimeSeconds();

	// The actor draws as if not hidden. Canvas.DrawActor leaves it hidden,
	// whatever it was, as the original's does (Engine.dll
	// UCanvas::execDrawActor 0x10377ed0; OverlayConsole); GC.DrawActor puts
	// the flag back, as the original's puts all back (extension-dll.md,
	// actors in a window), so a visible one -- the vision augmentation's heat
	// sources -- keeps it.
	bool oldHidden = actor->bHidden();
	actor->bHidden() = false;
	VisibleMesh vismesh;
	vismesh.Wire = wire;
	vismesh.OnItsOwn = true;
	vismesh.DrawMesh(&sceneFrame, actor, MeshPass::FaceOrder);
	actor->bHidden() = hideAfter || oldHidden;

	sceneFrame.Frame = scene;
	Device->SetSceneNode(&Canvas.Frame);
}

void RenderSubsystem::DrawClippedActor(UActor* actor, int X, int Y, int XB, int YB, bool ClearZ, bool wire)
{
	SceneNode frame;
	frame.XB = XB;
	frame.YB = YB;
	frame.X = X;
	frame.Y = Y;
	frame.FX = (float)X;
	frame.FY = (float)Y;
	frame.FX2 = frame.FX * 0.5f;
	frame.FY2 = frame.FY * 0.5f;
	frame.PolygonsAtFX15 = true;
	frame.ObjectToWorld = Coords::ViewToRenderDev().ToMatrix();
	frame.WorldToView = mat4::identity();
	frame.FovAngle = HorPlusFovAngle(engine->CameraFovAngle, frame.FX, frame.FY);
	float Aspect = frame.FY / frame.FX;
	float RProjZ = (float)std::tan(radians(frame.FovAngle) * 0.5f);
	frame.Projection = mat4::frustum(-RProjZ, RProjZ, -Aspect * RProjZ, Aspect * RProjZ, 1.0f, 32768.0f, handedness::left, clipzrange::zero_positive_w);
	Device->SetSceneNode(&frame);

	if (ClearZ)
		Device->ClearZ();

	// Drawn through a frame of its own: the eye at the origin of the scene
	// node above, the actor's location relative to it, so its faces are
	// culled, its environment map reflected and its vertex budget set from
	// that eye, not the player's in the world (the device draws through the
	// scene node set; the main frame's eye once served for the rest). The
	// actor is left hidden, as Canvas.DrawClippedActor leaves it (Engine.dll
	// UCanvas::execDrawClippedActor 0x103780d0; OverlayConsole).
	ActorFrame.Device = Device;
	ActorFrame.Frame = frame;
	ActorFrame.ViewLocation = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	ActorFrame.ViewRotation = Coords::Identity();
	actor->bHidden() = false;
	VisibleMesh vismesh;
	vismesh.Wire = wire;
	vismesh.OnItsOwn = true;
	vismesh.DrawMesh(&ActorFrame, actor, MeshPass::FaceOrder);
	actor->bHidden() = true;

	Device->SetSceneNode(&Canvas.Frame);
}

void RenderSubsystem::DrawTile(UTexture* Tex, float x, float y, float XL, float YL, float U, float V, float UL, float VL, float Z, vec4 color, vec4 fog, uint32_t flags)
{
	if (!Tex)
		return;
	UpdateTexture(Tex);
	Tex = Tex->GetAnimTexture();
	UpdateTexture(Tex);

	TextureInfo texinfo;
	texinfo.CacheID = (uint64_t)(ptrdiff_t)Tex;
	texinfo.Texture = Tex;
	texinfo.Format = texinfo.Texture->UsedFormat;
	texinfo.Mips = Tex->UsedMipmaps.data();
	texinfo.NumMips = (int)Tex->UsedMipmaps.size();
	texinfo.USize = Tex->USize();
	texinfo.VSize = Tex->VSize();
	if (Tex->Palette())
		texinfo.Palette = (TextureColor*)Tex->Palette()->Colors.data();
	ApplyDetailLOD(texinfo);
	texinfo.bRealtimeChanged = Tex->TakeChange();

	if (Tex->bMasked())
		flags |= PF_Masked;

	Device->DrawTile(&Canvas.Frame, texinfo, x, y, XL, YL, U, V, UL, VL, Z, color, fog, flags);
}

void RenderSubsystem::DrawTileClipped(UTexture* Tex, float orgX, float orgY, float curX, float curY, float XL, float YL, float U, float V, float UL, float VL, float Z, vec4 color, vec4 fog, uint32_t flags, float clipX, float clipY)
{
	if (!Tex)
		return;
	UpdateTexture(Tex);
	Tex = Tex->GetAnimTexture();
	UpdateTexture(Tex);

	TextureInfo texinfo;
	texinfo.CacheID = (uint64_t)(ptrdiff_t)Tex;
	texinfo.Texture = Tex;
	texinfo.Format = texinfo.Texture->UsedFormat;
	texinfo.Mips = Tex->UsedMipmaps.data();
	texinfo.NumMips = (int)Tex->UsedMipmaps.size();
	texinfo.USize = Tex->USize();
	texinfo.VSize = Tex->VSize();
	if (Tex->Palette())
		texinfo.Palette = (TextureColor*)Tex->Palette()->Colors.data();
	ApplyDetailLOD(texinfo);

	if (Tex->bMasked())
		flags |= PF_Masked;

	Rectf clipBox = Rectf::xywh(orgX, orgY, clipX, clipY);
	Rectf dest = Rectf::xywh(orgX + curX, orgY + curY, XL, YL);
	Rectf src = Rectf::xywh(U, V, UL, VL);
	DrawTile(texinfo, dest, src, clipBox, Z, color, fog, flags);
}

// A glyph's page as the device takes it: its own page's image, size and
// palette, keyed by the page
static void GlyphTextureInfo(const FontGlyph& glyph, TextureInfo& texinfo)
{
	texinfo = {};
	texinfo.CacheID = (uint64_t)(ptrdiff_t)glyph.Texture;
	texinfo.Texture = glyph.Texture;
	texinfo.Format = glyph.Texture->UsedFormat;
	texinfo.Mips = glyph.Texture->UsedMipmaps.data();
	texinfo.NumMips = (int)glyph.Texture->UsedMipmaps.size();
	texinfo.USize = glyph.Texture->USize();
	texinfo.VSize = glyph.Texture->VSize();
	if (glyph.Texture->Palette())
		texinfo.Palette = (TextureColor*)glyph.Texture->Palette()->Colors.data();
}

// The original's line drawer (Engine.dll 0x10376d20), from x, each glyph
// found as the canvas's text finds it (GetPageGlyph), drawn at the canvas's
// origin plus its place cut to a whole pixel, the step each time the glyph's
// width and SpaceX cut to a whole pixel; a glyph its font lacks, or its page
// has no texture for, draws nothing and takes no step. Clipped
// (DrawTextClipped), a glyph wholly off the clip is drawn nowhere and one
// across it cut to it, the rest going on. With hot keys, an & takes the
// character after it for one, drawn and then the font's _ over the same
// place, as wide as it at most, the step the character's; an & at the end
// ends the line, and && draws an & and makes the next & a hot key's. The
// line's width in all its steps.
int RenderSubsystem::DrawTextLine(UFont* font, vec4 color, float orgX, float orgY, int x, int y, const ScriptString& text, size_t count, uint32_t polyflags, float spaceX, bool clip, bool hotKeys, float clipX, float clipY)
{
	int step = 0;
	bool underline = false;
	int hotWidth = 0;
	for (size_t i = 0; i < count && i < text.size() && text[i] != 0; i++)
	{
		char16_t c = text[i];
		bool hot = false;
		if (hotKeys)
		{
			if (underline)
				c = '_';
			if (text[i] == '&')
			{
				char16_t next = i + 1 < count && i + 1 < text.size() ? text[i + 1] : 0;
				if (next == 0)
					break;
				if (next != '&')
				{
					hot = true;
					c = next;
				}
			}
		}

		FontGlyph glyph = font->GetPageGlyph(c);
		if (!glyph.Texture)
			continue;

		int width = underline ? std::min(hotWidth, glyph.USize) : glyph.USize;
		int stepWidth = width;
		int height = glyph.VSize;
		int u = glyph.StartU;
		int v = glyph.StartV;
		int dx = step + x;
		int dy = y;
		bool draw = true;
		if (clip)
		{
			if (dx + width <= 0 || (float)dx > clipX || height + dy <= 0 || (float)dy > clipY)
			{
				draw = false;
			}
			else
			{
				if (dx < 0)
				{
					width += dx;
					u -= dx;
					dx = 0;
				}
				if (dy < 0)
				{
					height += dy;
					v -= dy;
					dy = 0;
				}
				if ((float)(width + dx) > clipX)
					width = ftol(X87::Round((double)clipX - dx));
				if ((float)(height + dy) > clipY)
					height = ftol(X87::Round((double)clipY - dy));
			}
		}
		if (draw)
		{
			TextureInfo texinfo;
			GlyphTextureInfo(glyph, texinfo);
			float left = (float)ftol(X87::Round((double)dx + orgX));
			float top = (float)ftol(X87::Round((double)dy + orgY));
			Device->DrawTile(&Canvas.Frame, texinfo, left, top, (float)width, (float)height, (float)u, (float)v, (float)width, (float)height, 1.0f, color, vec4(0.0f), polyflags);
		}

		if (underline)
			stepWidth = hotWidth;
		if (hot)
			hotWidth = glyph.USize;
		else
			step += ftol(X87::Round((double)stepWidth + spaceX));
		underline = hot;
	}
	return step;
}

// The original's UCanvas::WrappedPrint (Engine.dll 0x10376790), which
// DrawText and StrLen lay their text out through (dx-reverse-info
// engine-dll.md, the canvas's text). A line runs from CurX's whole pixels,
// each glyph found as the canvas's text finds it (GetPageGlyph) stepping by
// its width and SpaceX cut to a whole pixel, and stops at a glyph whose step
// passes ClipX, a line feed or the text's end: it ends after the last word
// that fits, or within the first word, before any word has ended, after its
// last glyph that fits; a space it would wrap at stays out of its width. A
// line with nothing in it ends the print: a first glyph wider than the room,
// or a line feed, the text after it never laid out (0x103768ca). Each line
// is drawn, unless the print only measures or the line lies above or below
// the frame, at its X -- CurX, or with bCenter CurX plus half the room its
// end leaves (0x10376a58) -- and CurY, each cut to a whole pixel, through
// the line drawer, unclipped; then CurX is 0, CurY moves down by the line's
// height, its tallest glyph and SpaceY, and the spaces after it are passed
// over. XL is the furthest a line ended, from CurX's whole pixels for the
// first; YL the lines' heights summed.
void RenderSubsystem::WrappedPrint(UFont* font, vec4 color, float orgX, float orgY, float& curX, float& curY, const ScriptString& text, uint32_t polyflags, bool center, float spaceX, float spaceY, float clipX, float clipY, bool noDraw, int& XL, int& YL)
{
	XL = 0;
	YL = 0;
	if (clipX < 0.0f || clipY < 0.0f)
		return;

	auto at = [&](size_t i) -> char16_t { return i < text.size() ? text[i] : 0; };
	size_t pos = 0;
	do
	{
		int x = ftol(curX);
		size_t i = 0;
		size_t end = 0;
		int endX = 0;
		int lineHeight = 0;
		int endHeight = 0;
		bool wordEnded = false;
		for (char16_t c = at(pos); c != 0 && c != '\n'; c = at(pos + i))
		{
			FontGlyph glyph = font->GetPageGlyph(c);
			x += ftol(X87::Round((double)glyph.USize + spaceX));
			lineHeight = std::max(lineHeight, glyph.VSize + ftol(spaceY));
			if ((float)x > clipX)
				break;
			i++;
			char16_t next = at(pos + i);
			bool boundary = next == ' ' || next == '\n' || next == 0;
			if (boundary || !wordEnded)
			{
				endX = x;
				end = i;
				endHeight = lineHeight;
				wordEnded = wordEnded || boundary;
			}
		}
		if (end == 0)
			break;

		if (!noDraw && orgY + curY < (float)Canvas.Frame.Y && (float)endHeight + orgY + curY > 0.0f)
		{
			float lineX = center ? (clipX - (float)endX) * 0.5f + curX : curX;
			DrawTextLine(font, color, orgX, orgY, ftol(lineX), ftol(curY), text.substr(pos, end), end, polyflags, spaceX, false, false, clipX, clipY);
		}

		curX = 0.0f;
		curY += (float)endHeight;
		YL += endHeight;
		XL = std::max(XL, endX);
		pos += end;
		while (at(pos) == ' ')
			pos++;
	} while (at(pos) != 0);
}

// The original's execDrawText (Engine.dll 0x103778f0) after its
// WrappedPrint: CurX moved on by the furthest a line ended, so that with
// CurX at 0 after the lines it is that end, CurYL the greater of it and the
// lines' heights; with a CR, CurY moved down by CurYL and CurX and CurYL 0.
void RenderSubsystem::DrawText(UFont* font, vec4 color, float orgX, float orgY, float& curX, float& curY, float& curXL, float& curYL, bool newlineAtEnd, const ScriptString& text, uint32_t polyflags, bool center, float spaceX, float spaceY, float clipX, float clipY, bool noDraw)
{
	int XL = 0;
	int YL = 0;
	WrappedPrint(font, color, orgX, orgY, curX, curY, text, polyflags, center, spaceX, spaceY, clipX, clipY, noDraw, XL, YL);
	curX = (float)XL + curX;
	curXL = std::max(curXL, (float)XL);
	curYL = std::max(curYL, (float)YL);

	if (newlineAtEnd)
	{
		curX = 0;
		curY += curYL;
		curXL = 0;
		curYL = 0;
	}
}

// The original's execDrawTextClipped (Engine.dll 0x10378890): one line from
// CurX and CurY each cut to a whole pixel, never centred, through the line
// drawer clipped to ClipX and ClipY, with hot keys as asked (0x10376d20).
void RenderSubsystem::DrawTextClipped(UFont* font, vec4 color, float orgX, float orgY, float curX, float curY, const ScriptString& text, uint32_t polyflags, bool checkHotKey, float spaceX, float clipX, float clipY)
{
	DrawTextLine(font, color, orgX, orgY, ftol(curX), ftol(curY), text, text.size(), polyflags, spaceX, true, checkHotKey, clipX, clipY);
}

// A realtime texture's change taken by a tile the device draws, as the
// original's canvas locks no tile it rejects (Engine.dll UCanvas::DrawTile,
// 0x10376160): a tile clipped away leaves it to the next draw
static void TakeTileChange(TextureInfo& texinfo)
{
	if (texinfo.Texture && texinfo.Texture->TakeChange())
		texinfo.bRealtimeChanged = true;
}

void RenderSubsystem::DrawTile(TextureInfo& texinfo, const Rectf& dest, const Rectf& src, const Rectf& clipBox, float Z, vec4 color, vec4 fog, uint32_t flags)
{
	if (dest.left > dest.right || dest.top > dest.bottom)
		return;

	if (dest.left >= clipBox.left && dest.top >= clipBox.top && dest.right <= clipBox.right && dest.bottom <= clipBox.bottom)
	{
		TakeTileChange(texinfo);
		Device->DrawTile(&Canvas.Frame, texinfo, dest.left, dest.top, dest.right - dest.left, dest.bottom - dest.top, src.left, src.top, src.right - src.left, src.bottom - src.top, Z, color, fog, flags);
	}
	else
	{
		Rectf d = dest;
		Rectf s = src;

		float scaleX = (s.right - s.left) / (d.right - d.left);
		float scaleY = (s.bottom - s.top) / (d.bottom - d.top);

		if (d.left < clipBox.left)
		{
			s.left += scaleX * (clipBox.left - d.left);
			d.left = clipBox.left;
		}
		if (d.right > clipBox.right)
		{
			s.right += scaleX * (clipBox.right - d.right);
			d.right = clipBox.right;
		}
		if (d.top < clipBox.top)
		{
			s.top += scaleY * (clipBox.top - d.top);
			d.top = clipBox.top;
		}
		if (d.bottom > clipBox.bottom)
		{
			s.bottom += scaleY * (clipBox.bottom - d.bottom);
			d.bottom = clipBox.bottom;
		}

		if (d.left < d.right && d.top < d.bottom)
		{
			TakeTileChange(texinfo);
			Device->DrawTile(&Canvas.Frame, texinfo, d.left, d.top, d.right - d.left, d.bottom - d.top, s.left, s.top, s.right - s.left, s.bottom - s.top, Z, color, fog, flags);
		}
	}
}

void RenderSubsystem::Draw2DLine(vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2)
{
	Device->Draw2DLine(&Canvas.Frame, Color, LineFlags, P1, P2);
}

void RenderSubsystem::DrawTile(TextureInfo& Info, float X, float Y, float XL, float YL, float U, float V, float UL, float VL, float Z, vec4 Color, vec4 Fog, uint32_t PolyFlags)
{
	Device->DrawTile(&Canvas.Frame, Info, X, Y, XL, YL, U, V, UL, VL, Z, Color, Fog, PolyFlags);
}

// The original's execTextSize (Engine.dll 0x10378be0) with no SpaceX or
// SpaceY: each glyph found as the canvas's text finds it (GetPageGlyph), the
// widths summed and the tallest
vec2 RenderSubsystem::GetTextSize(UFont* font, const ScriptString& text, float spaceX, float spaceY)
{
	float x = 0.0f;
	float y = 0.0f;
	for (char16_t c : text)
	{
		FontGlyph glyph = font->GetPageGlyph(c);
		x += (float)(int)((float)glyph.USize + spaceX);
		y = std::max(y, (float)(glyph.VSize + (int)spaceY));
	}
	return { x, y };
}

void RenderSubsystem::DrawTimedemoStats()
{
	Canvas.framesDrawn++;
	if (Canvas.startFPSTime == 0 || engine->lastTime - Canvas.startFPSTime >= 1'000'000)
	{
		Canvas.fps = Canvas.framesDrawn;
		Canvas.startFPSTime = engine->lastTime;
		Canvas.framesDrawn = 0;
	}

	if (ShowRenderStats)
	{
		Array<ScriptString> lines;
		lines.push_back(UTF8ToScript(std::to_string(Canvas.fps) + " FPS"));
		lines.push_back(UTF8ToScript(std::to_string(engine->Level->Actors.size()) + " actors"));
		lines.push_back(UTF8ToScript(std::to_string(GC::GetStats().numObjects) + " GC objects"));
		lines.push_back(UTF8ToScript(std::to_string(GC::GetStats().memoryUsage / (1024 * 1024)) + " mb memory used"));

		UFont* font = engine->canvas->MedFont();
		if (font)
		{
			float curY = 180;
			for (const ScriptString& text : lines)
			{
				float curX = engine->viewport->ViewportWidth() - GetTextSize(font, text).x - 16;
				float curXL = 0.0f;
				float curYL = 0.0f;
				DrawText(font, vec4(1.0f), 0.0f, 0.0f, curX, curY, curXL, curYL, false, text, PF_NoSmooth | PF_Masked, false);
				curY += curYL;
			}
		}
	}
}
