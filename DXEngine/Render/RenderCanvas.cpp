
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
	DrawTimedemoStats();
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
	texinfo.bRealtimeChanged = Tex->ChangedThisFrame(TextureFrameCounter);

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
	texinfo.bRealtimeChanged = Tex->ChangedThisFrame(TextureFrameCounter);

	if (Tex->bMasked())
		flags |= PF_Masked;

	Rectf clipBox = Rectf::xywh(orgX, orgY, clipX, clipY);
	Rectf dest = Rectf::xywh(orgX + curX, orgY + curY, XL, YL);
	Rectf src = Rectf::xywh(U, V, UL, VL);
	DrawTile(texinfo, dest, src, clipBox, Z, color, fog, flags);
}

Array<ScriptString> RenderSubsystem::FindTextBlocks(const ScriptString& text)
{
	// Split text into words, whitespace or newline
	Array<ScriptString> textBlocks;
	size_t pos = 0;
	while (pos < text.size())
	{
		if (text[pos] == '\n')
		{
			textBlocks.push_back(u"\n");
			pos++;
		}
		else if (text[pos] == ' ')
		{
			size_t end = std::min(text.find_first_not_of(u' ', pos + 1), text.size());
			textBlocks.push_back(text.substr(pos, end - pos));
			pos = end;
		}
		else
		{
			size_t end = std::min(text.find_first_of(u" \n", pos + 1), text.size());
			textBlocks.push_back(text.substr(pos, end - pos));
			pos = end;
		}
	}
	return textBlocks;
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

void RenderSubsystem::DrawTextBlockRange(float x, float y, const Array<ScriptString>& textBlocks, size_t start, size_t end, UFont* font, vec4 color, uint32_t polyflags, float spaceX)
{
	for (size_t i = start; i < end; i++)
	{
		for (char16_t c : textBlocks[i])
		{
			FontGlyph glyph = font->GetGlyph(c);

			if (!glyph.Texture)
				continue;

			TextureInfo texinfo;
			GlyphTextureInfo(glyph, texinfo);

			int width = glyph.USize;
			int height = glyph.VSize;
			float StartU = (float)glyph.StartU;
			float StartV = (float)glyph.StartV;
			float USize = (float)glyph.USize;
			float VSize = (float)glyph.VSize;

			Device->DrawTile(&Canvas.Frame, texinfo, x, y, (float)width, (float)height, StartU, StartV, USize, VSize, 1.0f, color, vec4(0.0f), polyflags);

			// A whole pixel's step, the glyph's width and SpaceX truncated, as
			// the original's WrappedPrint steps (Engine.dll 0x10376790)
			x += (float)(int)((float)width + spaceX);
		}
	}
}

// The original's UCanvas::WrappedPrint (Engine.dll 0x10376790): a line drawn
// from whole pixels, its X the canvas's CurX (plus half the room the line
// leaves, centred) and its Y CurY, each truncated; the line wraps where
// CurX's whole pixels and its glyphs pass ClipX, and CurX is where the last
// line ended (dx-reverse-info engine-dll.md, the loading screen).
void RenderSubsystem::DrawText(UFont* font, vec4 color, float orgX, float orgY, float& curX, float& curY, float& curXL, float& curYL, bool newlineAtEnd, const ScriptString& text, uint32_t polyflags, bool center, float spaceX, float spaceY, float clipX, float clipY, bool noDraw)
{
	float totalWidth = 0.0f;
	float totalHeight = 0.0f;
	auto lineX = [&](float lineWidth) { return (float)(int)(center ? curX + (clipX - lineWidth) * 0.5f : curX); };

	Array<ScriptString> textBlocks = FindTextBlocks(text);
	size_t lineBegin = 0;
	float lineWidth = 0.0f;
	float lineHeight = 0.0f;
	for (size_t pos = 0; pos < textBlocks.size(); pos++)
	{
		if (textBlocks[pos].front() == '\n')
		{
			if (pos != lineBegin)
			{
				if (!noDraw)
					DrawTextBlockRange(orgX + lineX(lineWidth), orgY + (float)(int)curY, textBlocks, lineBegin, pos, font, color, polyflags, spaceX);
				curY += lineHeight;
				totalHeight += lineHeight;
				totalWidth = std::max(totalWidth, lineWidth);
			}

			curX = 0;
			lineBegin = pos + 1;
			lineWidth = 0.0f;
			lineHeight = 0.0f;
		}
		else
		{
			vec2 blockSize = GetTextSize(font, textBlocks[pos], spaceX, spaceY);
			if ((float)(int)curX + lineWidth + blockSize.x > clipX)
			{
				if (!noDraw)
					DrawTextBlockRange(orgX + lineX(lineWidth), orgY + (float)(int)curY, textBlocks, lineBegin, pos, font, color, polyflags, spaceX);

				curX = 0;
				curY += lineHeight;
				totalHeight += lineHeight;
				totalWidth = std::max(totalWidth, lineWidth);

				if (textBlocks[pos].front() == ' ')
				{
					// Ignore whitespace at the beginning of a word wrapped line
					lineBegin = pos + 1;
					lineWidth = 0.0f;
					lineHeight = 0.0f;
				}
				else
				{
					lineBegin = pos;
					lineWidth = blockSize.x;
					lineHeight = blockSize.y;
				}
			}
			else
			{
				lineWidth += blockSize.x;
				lineHeight = std::max(lineHeight, blockSize.y);
			}
		}
	}

	if (lineBegin < textBlocks.size())
	{
		float x = lineX(lineWidth);
		if (!noDraw)
			DrawTextBlockRange(orgX + x, orgY + (float)(int)curY, textBlocks, lineBegin, textBlocks.size(), font, color, polyflags, spaceX);
		curX = x + lineWidth;
		curY += lineHeight;
		totalHeight += lineHeight;
		totalWidth = std::max(totalWidth, lineWidth);
	}

	curXL = std::max(curXL, totalWidth);
	curYL = std::max(curYL, totalHeight);

	if (newlineAtEnd)
	{
		curX = 0;
		curY += curYL;
		curXL = 0;
		curYL = 0;
	}
}

// The original's execDrawTextClipped (Engine.dll 0x10378890): one line from
// CurX and CurY each truncated to a whole pixel, never centred -- its line
// drawer (0x10376d20) takes the place it is given and reads no bCenter.
void RenderSubsystem::DrawTextClipped(UFont* font, vec4 color, float orgX, float orgY, float curX, float curY, const ScriptString& text, uint32_t polyflags, bool checkHotKey, float clipX, float clipY)
{
	curX = (float)(int)curX;
	curY = (float)(int)curY;

	FontGlyph uglyph = font->GetGlyph('_');
	int uwidth = uglyph.USize;
	int uheight = uglyph.VSize;
	float uStartU = (float)uglyph.StartU;
	float uStartV = (float)uglyph.StartV;
	float uUSize = (float)uglyph.USize;
	float uVSize = (float)uglyph.VSize;

	Rectf clipBox = Rectf::xywh(orgX, orgY, clipX, clipY);

	bool foundAmpersand = false;
	int maxY = 0;
	for (char16_t c : text)
	{
		if (checkHotKey && c == '&' && !foundAmpersand)
		{
			foundAmpersand = true;
		}
		else if (foundAmpersand && c != '&')
		{
			foundAmpersand = false;

			// The hot key's letter, then the '_' glyph under it, each from its
			// own page
			FontGlyph glyph = font->GetGlyph(c);
			if (curX + glyph.USize > (int)clipX)
				break;

			if (glyph.Texture)
			{
				TextureInfo texinfo;
				GlyphTextureInfo(glyph, texinfo);
				Rectf dest = Rectf::xywh(orgX + curX, orgY + curY, (float)glyph.USize, (float)glyph.VSize);
				Rectf src = Rectf::xywh((float)glyph.StartU, (float)glyph.StartV, (float)glyph.USize, (float)glyph.VSize);
				DrawTile(texinfo, dest, src, clipBox, 1.0f, color, vec4(0.0f), polyflags);
			}
			if (uglyph.Texture)
			{
				TextureInfo texinfo;
				GlyphTextureInfo(uglyph, texinfo);
				Rectf dest = Rectf::xywh(orgX + curX + (glyph.USize - uwidth) / 2, orgY + curY, (float)uwidth, (float)uheight);
				Rectf src = Rectf::xywh(uStartU, uStartV, uUSize, uVSize);
				DrawTile(texinfo, dest, src, clipBox, 1.0f, color, vec4(0.0f), polyflags);
			}

			curX += glyph.USize;
			maxY = std::max(maxY, glyph.VSize);
		}
		else
		{
			foundAmpersand = false;

			// A character the font lacks, with no space to stand in (a mod's
			// font), draws nothing and takes no room
			FontGlyph glyph = font->GetGlyph(c);
			if (curX + glyph.USize > (int)clipX)
				break;

			if (glyph.Texture)
			{
				TextureInfo texinfo;
				GlyphTextureInfo(glyph, texinfo);
				Rectf dest = Rectf::xywh(orgX + curX, orgY + curY, (float)glyph.USize, (float)glyph.VSize);
				Rectf src = Rectf::xywh((float)glyph.StartU, (float)glyph.StartV, (float)glyph.USize, (float)glyph.VSize);
				DrawTile(texinfo, dest, src, clipBox, 1.0f, color, vec4(0.0f), polyflags);
			}

			curX += glyph.USize;
			maxY = std::max(maxY, glyph.VSize);
		}
	}
}

void RenderSubsystem::DrawTile(TextureInfo& texinfo, const Rectf& dest, const Rectf& src, const Rectf& clipBox, float Z, vec4 color, vec4 fog, uint32_t flags)
{
	if (dest.left > dest.right || dest.top > dest.bottom)
		return;

	if (dest.left >= clipBox.left && dest.top >= clipBox.top && dest.right <= clipBox.right && dest.bottom <= clipBox.bottom)
	{
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
			Device->DrawTile(&Canvas.Frame, texinfo, d.left, d.top, d.right - d.left, d.bottom - d.top, s.left, s.top, s.right - s.left, s.bottom - s.top, Z, color, fog, flags);
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

vec2 RenderSubsystem::GetTextSize(UFont* font, const ScriptString& text, float spaceX, float spaceY)
{
	float x = 0.0f;
	float y = 0.0f;
	for (char16_t c : text)
	{
		FontGlyph glyph = font->GetGlyph(c);
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
