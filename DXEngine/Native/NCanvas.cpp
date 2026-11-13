
#include "Precomp.h"
#include "NCanvas.h"
#include "VM/NativeFunc.h"
#include "Engine.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/UCanvas.h"
#include "Packages/Engine/UClient.h"
#include "Render/RenderSubsystem.h"
#include "Package/PackageManager.h"
#include "Utils/StrTools.h"
#include "Math/floating.h"

#ifdef _MSC_VER
#pragma warning(disable: 4244) // warning C4244: 'argument': conversion from 'float' to 'int', possible loss of data
#endif

void NCanvas::RegisterFunctions()
{
	RegisterVMNativeFunc_7("Canvas", "DrawClippedActor", &NCanvas::DrawClippedActor, 471);
	RegisterVMNativeFunc_9("Canvas", "DrawPortal", &NCanvas::DrawPortal, 480);
	RegisterVMNativeFunc_2("Canvas", "DrawText", &NCanvas::DrawText, 465);
	RegisterVMNativeFunc_2("Canvas", "DrawTextClipped", &NCanvas::DrawTextClipped, 469);
	RegisterVMNativeFunc_7("Canvas", "DrawTile", &NCanvas::DrawTile, 466);
	RegisterVMNativeFunc_7("Canvas", "DrawTileClipped", &NCanvas::DrawTileClipped, 468);
	RegisterVMNativeFunc_3("Canvas", "DrawActor", &NCanvas::DrawActor, 467);
	RegisterVMNativeFunc_3("Canvas", "StrLen", &NCanvas::StrLen, 464);
	RegisterVMNativeFunc_3("Canvas", "TextSize", &NCanvas::TextSize, 470);
}

// WireFrame draws the actor's mesh as lines, as the original draws the call
// with the player's RendMap at wire (Engine.dll UCanvas::execDrawActor
// 0x10377ed0; VisibleMesh::Wire); the game passes false. The actor is left
// hidden, as the original's leave it (RenderSubsystem::DrawActor).
void NCanvas::DrawActor(UObject* Self, UObject* A, bool WireFrame, std::optional<bool> ClearZ)
{
	engine->render->DrawActor(UObject::Cast<UActor>(A), ClearZ ? *ClearZ : false, true, nullptr, WireFrame);
}

void NCanvas::DrawClippedActor(UObject* Self, UObject* A, bool WireFrame, int X, int Y, int XB, int YB, std::optional<bool> ClearZ)
{
	engine->render->DrawClippedActor(UObject::Cast<UActor>(A), X, Y, XB, YB, ClearZ ? *ClearZ : false, WireFrame);
}

void NCanvas::DrawPortal(UObject* Self, int X, int Y, int Width, int Height, UObject* CamActor, const vec3& CamLocation, const Rotator& CamRotation, std::optional<int> FOV, std::optional<bool> ClearZ)
{
}

void NCanvas::DrawText(UObject* Self, const ScriptString& Text, std::optional<bool> CR)
{
	UCanvas* SelfCanvas = UObject::Cast<UCanvas>(Self);
	float& orgX = SelfCanvas->OrgX();
	float& orgY = SelfCanvas->OrgY();
	float& curX = SelfCanvas->CurX();
	float& curY = SelfCanvas->CurY();
	float& clipX = SelfCanvas->ClipX();
	float& clipY = SelfCanvas->ClipY();
	UFont*& font = SelfCanvas->Font();
	Color& color = SelfCanvas->DrawColor();
	float& curYL = SelfCanvas->CurYL();
	uint8_t& style = SelfCanvas->Style();
	bool center = SelfCanvas->bCenter();
	float& spaceX = SelfCanvas->SpaceX();
	float& spaceY = SelfCanvas->SpaceY();

	// No font: logged and nothing done, as the original's execDrawText
	// (Engine.dll 0x103778f0, its text); the renderer dereferenced it
	if (!font)
	{
		LogMessage("DrawText: No font");
		return;
	}

	bool newline = CR ? *CR : true;

	if (style != 0)
	{
		uint32_t renderflags = PF_NoSmooth | PF_Masked;
		if (style == 3)
			renderflags |= PF_Translucent;
		else if (style == 4)
			renderflags |= PF_Modulated;

		float curXL = 0.0f;
		engine->render->DrawText(font, { color.R / 255.0f, color.G / 255.0f, color.B / 255.0f, color.A / 255.0f }, orgX, orgY, curX, curY, curXL, curYL, newline, Text, renderflags, center, spaceX, spaceY, clipX, clipY);
	}
	else
	{
		if (newline)
		{
			curX = 0;
			curY += curYL;
			curYL = 0;
		}
	}
}

void NCanvas::DrawTextClipped(UObject* Self, const ScriptString& Text, std::optional<bool> bCheckHotKey)
{
	UCanvas* SelfCanvas = UObject::Cast<UCanvas>(Self);
	float& orgX = SelfCanvas->OrgX();
	float& orgY = SelfCanvas->OrgY();
	float& curX = SelfCanvas->CurX();
	float& curY = SelfCanvas->CurY();
	float& clipX = SelfCanvas->ClipX();
	float& clipY = SelfCanvas->ClipY();
	UFont*& font = SelfCanvas->Font();
	Color& color = SelfCanvas->DrawColor();
	uint8_t& style = SelfCanvas->Style();

	bool checkHotKey = bCheckHotKey && *bCheckHotKey;

	// As the original's execDrawTextClipped (Engine.dll 0x10378890)
	if (!font)
	{
		LogMessage("DrawTextClipped: No font");
		return;
	}

	// LargeFont and BigFont give way to MedFont, the canvas's Font left so,
	// where the language is not int (0x1037894c-0x10378972): the ini's
	// [Engine.Engine] Language, as Core's appInit takes it (Core.dll
	// 0x10149d5e-0x10149d76)
	static const bool intLanguage = StrTools::equals_ignore_case(engine->packages->GetIniValue("system", "Engine.Engine", "Language", "int"), "int");
	if (!intLanguage && (font == SelfCanvas->LargeFont() || font == SelfCanvas->BigFont()))
		font = SelfCanvas->MedFont();
	if (!font)
	{
		LogMessage("DrawTextClipped: No font");
		return;
	}

	if (style != 0)
	{
		uint32_t renderflags = PF_NoSmooth | PF_Masked;
		if (style == 3)
			renderflags |= PF_Translucent;
		else if (style == 4)
			renderflags |= PF_Modulated;

		engine->render->DrawTextClipped(font, { color.R / 255.0f, color.G / 255.0f, color.B / 255.0f, color.A / 255.0f }, orgX, orgY, curX, curY, Text, renderflags, checkHotKey, SelfCanvas->SpaceX(), clipX, clipY);
	}
}

// A tile's flags by the canvas's style, as the original's execDrawTile and
// execDrawTileClipped make them (Engine.dll 0x10377ccd, 0x10378657): two-sided,
// STY_Translucent's translucent bit, STY_Modulated's modulated one and
// bNoSmooth's; STY_Masked adds nothing, the canvas's DrawTile taking the
// texture's own masked bit (0x10376160; RenderSubsystem::DrawTile)
static uint32_t CanvasTileFlags(uint8_t style, bool noSmooth)
{
	uint32_t renderflags = PF_TwoSided;
	if (style == 3)
		renderflags |= PF_Translucent;
	else if (style == 4)
		renderflags |= PF_Modulated;
	if (noSmooth)
		renderflags |= PF_NoSmooth;
	return renderflags;
}

void NCanvas::DrawTile(UObject* Self, UObject* Tex, float XL, float YL, float U, float V, float UL, float VL)
{
	UCanvas* SelfCanvas = UObject::Cast<UCanvas>(Self);
	float& orgX = SelfCanvas->OrgX();
	float& orgY = SelfCanvas->OrgY();
	float& curX = SelfCanvas->CurX();
	float& curY = SelfCanvas->CurY();
	uint8_t& style = SelfCanvas->Style();
	Color& color = SelfCanvas->DrawColor();
	float& spaceX = SelfCanvas->SpaceX();
	float& curYL = SelfCanvas->CurYL();
	bool noSmooth = SelfCanvas->bNoSmooth();

	// As the original's execDrawTile (Engine.dll 0x10377ba0): no texture is
	// logged and moves nothing; any other tile is drawn in any style but
	// none, at the canvas's Z (0x10377d2e), the canvas's DrawTile passing
	// over one of no width or height, and moves CurX on by SpaceX and its
	// width, and CurYL to the larger of it and its height, whatever its
	// size, SpaceY no part of it
	if (!Tex)
	{
		LogMessage("DrawTile: Missing Texture");
		return;
	}

	if (style != 0)
		engine->render->DrawTile((UTexture*)Tex, orgX + curX, orgY + curY, XL, YL, U, V, UL, VL, SelfCanvas->Z(), { color.R / 255.0f, color.G / 255.0f, color.B / 255.0f, color.A / 255.0f }, { 0.0f }, CanvasTileFlags(style, noSmooth));

	curX = spaceX + curX + XL;
	if (!(curYL >= YL))
		curYL = YL;
}

void NCanvas::DrawTileClipped(UObject* Self, UObject* Tex, float XL, float YL, float U, float V, float UL, float VL)
{
	UCanvas* SelfCanvas = UObject::Cast<UCanvas>(Self);
	float& orgX = SelfCanvas->OrgX();
	float& orgY = SelfCanvas->OrgY();
	float& curX = SelfCanvas->CurX();
	float& curY = SelfCanvas->CurY();
	uint8_t& style = SelfCanvas->Style();
	Color& color = SelfCanvas->DrawColor();
	float& spaceX = SelfCanvas->SpaceX();
	float& curYL = SelfCanvas->CurYL();
	bool noSmooth = SelfCanvas->bNoSmooth();
	float& clipX = SelfCanvas->ClipX();
	float& clipY = SelfCanvas->ClipY();

	// As the original's execDrawTileClipped (Engine.dll 0x10378450): no
	// texture is logged and does nothing, nor does a tile of no width or
	// height; any other is cut first at the canvas's origin, a CurX or CurY
	// not at least 0 made 0 with as much of the tile and its texels taken
	// off, then at ClipX and ClipY, its texels in proportion, each in the
	// original's float order (0x1037859d-0x10378655); drawn there in any
	// style but none, at the canvas's Z (0x103786b8), CurX moved on by
	// SpaceX and the cut width, and CurYL to the larger of it and the cut
	// height
	if (!Tex)
	{
		LogMessage("DrawTileClipped: Missing Texture");
		return;
	}
	if (!(XL > 0.0f && YL > 0.0f))
		return;

	if (!(curX >= 0.0f))
	{
		float cut = UL * curX / XL;
		U = U - cut;
		UL = cut + UL;
		XL = XL + curX;
		curX = 0.0f;
	}
	if (!(curY >= 0.0f))
	{
		float cut = VL * curY / YL;
		V = V - cut;
		VL = cut + VL;
		YL = YL + curY;
		curY = 0.0f;
	}
	float roomX = clipX - curX;
	if (XL > roomX)
	{
		UL = (roomX - XL) * UL / XL + UL;
		XL = roomX;
	}
	float roomY = clipY - curY;
	if (YL > roomY)
	{
		VL = (roomY - YL) * VL / YL + VL;
		YL = roomY;
	}

	if (style != 0)
		engine->render->DrawTile((UTexture*)Tex, orgX + curX, orgY + curY, XL, YL, U, V, UL, VL, SelfCanvas->Z(), { color.R / 255.0f, color.G / 255.0f, color.B / 255.0f, color.A / 255.0f }, { 0.0f }, CanvasTileFlags(style, noSmooth));

	curX = spaceX + curX + XL;
	if (!(curYL >= YL))
		curYL = YL;
}

void NCanvas::StrLen(UObject* Self, const ScriptString& String, float& XL, float& YL)
{
	UCanvas* SelfCanvas = UObject::Cast<UCanvas>(Self);
	float& orgX = SelfCanvas->OrgX();
	float& orgY = SelfCanvas->OrgY();
	float& clipX = SelfCanvas->ClipX();
	float& clipY = SelfCanvas->ClipY();
	UFont*& font = SelfCanvas->Font();
	bool center = SelfCanvas->bCenter();
	float& spaceX = SelfCanvas->SpaceX();
	float& spaceY = SelfCanvas->SpaceY();

	// Measured whatever the canvas's Style: the original's execStrLen
	// (Engine.dll 0x10377650) lays the text out through WrappedPrint with a
	// style of none, which draws nothing and measures all, from CurX and CurY
	// at 0, and puts them back cut to whole pixels (0x10377721). No font
	// asserts there: 0 by 0 here, logged.
	if (!font)
	{
		LogMessage("StrLen: No font");
		XL = 0.0f;
		YL = 0.0f;
		return;
	}
	float& curX = SelfCanvas->CurX();
	float& curY = SelfCanvas->CurY();
	int x = ftol(curX);
	int y = ftol(curY);
	curX = 0.0f;
	curY = 0.0f;
	int width = 0;
	int height = 0;
	engine->render->WrappedPrint(font, vec4(1.0f), orgX, orgY, curX, curY, String, 0, center, spaceX, spaceY, clipX, clipY, true, width, height);
	curX = (float)x;
	curY = (float)y;
	XL = (float)width;
	YL = (float)height;
}

void NCanvas::TextSize(UObject* Self, const ScriptString& String, float& XL, float& YL)
{
	UCanvas* SelfCanvas = UObject::Cast<UCanvas>(Self);
	UFont*& font = SelfCanvas->Font();

	// No font: logged, the outputs left as they were, as the original's
	// execTextSize (Engine.dll 0x10378be0)
	if (!font)
	{
		LogMessage("TextSize: No font");
		return;
	}
	vec2 size = engine->render->GetTextSize(font, String);
	XL = size.x;
	YL = size.y;
}
