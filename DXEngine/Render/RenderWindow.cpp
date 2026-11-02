
#include "Precomp.h"
#include "RenderSubsystem.h"
#include "RenderDevice/RenderDevice.h"
#include "GameWindow.h"
#include "VM/ScriptCall.h"
#include "Engine.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/UCanvas.h"
#include "Packages/Engine/Resources/UFont.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Extension/Windows/UGC.h"
#include "Packages/Extension/Windows/TabGroup/URootWindow.h"

void RenderSubsystem::PreRenderWindows(UCanvas* canvas)
{
	if (!engine->dxRootWindow)
		return;

	// The root sized to the screen, as the original's PreRender sizes it
	// (URootWindow::ResizeRoot); every other window is laid out as it asks.
	// Then WindowReady for each window shown that has not had it.
	engine->dxRootWindow->ResizeRoot();
	engine->dxRootWindow->InvokeWindowReady();
}

void RenderSubsystem::PostRenderWindows(UCanvas* canvas)
{
	if (!engine->dxRootWindow)
		return;

	Device->SetSceneNode(&Canvas.Frame);
	Device->ClearZ();

	float virtualScale = UWindow::GetVirtualScale();
	float realVirtualWidth = engine->viewport->ViewportWidth() / virtualScale;
	float realVirtualHeight = engine->viewport->ViewportHeight() / virtualScale;
	engine->dxgc->ResetClip(engine->dxgc->ScaleRect(Rectf::xywh(0.0f, 0.0f, realVirtualWidth, realVirtualHeight)));

	DrawRawBackground(engine->dxRootWindow);
	DrawWindow(engine->dxRootWindow, 0.0f, 0.0f);

	if (engine->getDXWindowDebugMode())
	{
		float curY = 100.0f;
		DrawWindowInfo(engine->canvas->SmallFont(), engine->dxRootWindow, 0, curY);
	}
}

// The root window's raw background -- DeusExRootWindow's snapshot under a
// menu -- as the original draws it, before any window: over the root less
// what the scene covers (all of it with rendering on and no render viewport,
// nothing with rendering off, the viewport's rectangle with one set), each
// piece left stretched from the background's own size or tiled from its
// corner, in the raw colour and unsmoothed (dx-reverse-info/extension-dll.md, the
// raw background). Every window lets it through, as every window of the
// original's is made with bDrawRawBackground on.
void RenderSubsystem::DrawRawBackground(URootWindow* root)
{
	UTexture* tex = root->rawBackground();
	if (!tex)
		return;

	Rectf full = Rectf::xywh(0.0f, 0.0f, root->Width(), root->Height());
	Array<Rectf> pieces;
	if (!root->bRender())
	{
		pieces.push_back(full);
	}
	else if (root->RenderViewportSet)
	{
		// The original's clip list subtraction: above the viewport, below
		// it, then left and right of it within its height.
		float cutLeft = std::max(root->renderX(), full.left);
		float cutTop = std::max(root->renderY(), full.top);
		float cutRight = std::min(root->renderX() + root->renderWidth(), full.right);
		float cutBottom = std::min(root->renderY() + root->renderHeight(), full.bottom);
		if (cutRight <= cutLeft || cutBottom <= cutTop)
		{
			pieces.push_back(full);
		}
		else
		{
			pieces.push_back(Rectf(full.left, full.top, full.right, cutTop));
			pieces.push_back(Rectf(full.left, cutBottom, full.right, full.bottom));
			pieces.push_back(Rectf(full.left, cutTop, cutLeft, cutBottom));
			pieces.push_back(Rectf(cutRight, cutTop, full.right, cutBottom));
		}
	}

	ResetWindowGC(root, root->UsedX, root->UsedY);
	engine->dxgc->SetStyle(EDrawStyle::Normal);
	engine->dxgc->SetTileColor(root->rawColor());
	engine->dxgc->EnableSmoothing(false);
	for (const Rectf& piece : pieces)
	{
		float w = piece.right - piece.left;
		float h = piece.bottom - piece.top;
		if (w <= 0.0f || h <= 0.0f)
			continue;
		if (root->bStretchRawBackground())
			engine->dxgc->DrawStretchedTexture(piece.left, piece.top, w, h, 0.0f, 0.0f, root->rawBackgroundWidth(), root->rawBackgroundHeight(), tex);
		else
			engine->dxgc->DrawPattern(piece.left, piece.top, w, h, piece.left, piece.top, tex);
	}
}

void RenderSubsystem::ResetWindowGC(UWindow* window, float offsetX, float offsetY)
{
	// Reset everything first
	engine->dxgc->HAlign() = (uint8_t)EHAlign::Left;
	engine->dxgc->PolyFlags() = 0;
	engine->dxgc->Style() = 0;
	engine->dxgc->TextColor() = { 255, 255, 255, 255 };
	engine->dxgc->VAlign() = (uint8_t)EVAlign::Top;
	engine->dxgc->bDrawEnabled() = true;
	engine->dxgc->bFree() = false;
	engine->dxgc->bMasked() = false;
	engine->dxgc->bModulated() = false;
	engine->dxgc->bParseMetachars() = false;
	engine->dxgc->bSmoothed() = false;
	engine->dxgc->bTextTranslucent() = false;
	engine->dxgc->bTranslucent() = false;
	engine->dxgc->bWordWrap() = true;
	engine->dxgc->baselineOffset() = 0.0f;
	engine->dxgc->boldFont() = nullptr;
	engine->dxgc->gcClipRect() = {};
	engine->dxgc->gcCount() = 0;
	engine->dxgc->gcFree() = nullptr;
	engine->dxgc->gcOwner() = nullptr;
	engine->dxgc->gcStack() = nullptr;
	// The root's multipliers, as the original's GC is given them
	// (XGC::SetMultipliers from XRootWindow::PaintWindows)
	engine->dxgc->hMultiplier() = (int)UWindow::GetVirtualScale();
	engine->dxgc->normalFont() = nullptr;
	//engine->dxgc->textPlane() = {}
	engine->dxgc->textPolyFlags() = 0;
	engine->dxgc->textVSpacing() = 0.0f;
	engine->dxgc->tileColor() = { 255, 255, 255, 255 };
	//engine->dxgc->tilePlane() = {};
	engine->dxgc->underlineHeight() = 0.0f;
	engine->dxgc->underlineTexture() = engine->dxgc->DefaultUnderlineTexture;
	engine->dxgc->vMultiplier() = (int)UWindow::GetVirtualScale();
	engine->dxgc->SpecialTextEnabled = false;

	// Copy state from window
	engine->dxgc->EnableSpecialText(window->bSpecialText());
	engine->dxgc->EnableTranslucentText(window->bTextTranslucent());
	engine->dxgc->SetBaselineData(window->BaselineOffset, window->UnderlineHeight);
	engine->dxgc->SetFonts(window->normalFont(), window->boldFont());
	engine->dxgc->SetTextColor(window->TextColor());
	engine->dxgc->SetTextVSpacing(window->textVSpacing());
	engine->dxgc->SetTileColor(window->tileColor());

	// Setup origin
	engine->dxgc->offsetX = offsetX;
	engine->dxgc->offsetY = offsetY;
}

// The original's XWindow::GetGC (Extension.dll 0x1004ddc0) pushes the one
// GC's state and copies the window's own into it; ReleaseGC (0x1004de50)
// pops it. So a script measuring text outside a draw (a menu title's
// preferred size) takes its own window's fonts and settings, not the last
// drawn window's leftovers. The state is kept as a closure that puts it back.
void RenderSubsystem::PushWindowGC(UWindow* window)
{
	UGC* gc = engine->dxgc;
	auto hAlign = gc->HAlign(); auto polyFlags = gc->PolyFlags(); auto style = gc->Style(); auto textColor = gc->TextColor(); auto vAlign = gc->VAlign();
	bool drawEnabled = gc->bDrawEnabled(), free = gc->bFree(), masked = gc->bMasked(), modulated = gc->bModulated(), parseMetachars = gc->bParseMetachars();
	bool smoothed = gc->bSmoothed(), textTranslucent = gc->bTextTranslucent(), translucent = gc->bTranslucent(), wordWrap = gc->bWordWrap();
	auto baselineOffset = gc->baselineOffset(); auto boldFont = gc->boldFont(); auto clipRect = gc->gcClipRect(); auto gcCount = gc->gcCount(); auto gcFree = gc->gcFree(); auto gcOwner = gc->gcOwner(); auto gcStack = gc->gcStack();
	auto hMultiplier = gc->hMultiplier(); auto normalFont = gc->normalFont(); auto textPolyFlags = gc->textPolyFlags(); auto textVSpacing = gc->textVSpacing(); auto tileColor = gc->tileColor();
	auto underlineHeight = gc->underlineHeight(); auto underlineTexture = gc->underlineTexture(); auto vMultiplier = gc->vMultiplier(); bool specialText = gc->SpecialTextEnabled;
	float offsetX = gc->offsetX, offsetY = gc->offsetY;
	WindowGCStack.push_back([=]()
	{
		gc->HAlign() = hAlign; gc->PolyFlags() = polyFlags; gc->Style() = style; gc->TextColor() = textColor; gc->VAlign() = vAlign;
		gc->bDrawEnabled() = drawEnabled; gc->bFree() = free; gc->bMasked() = masked; gc->bModulated() = modulated; gc->bParseMetachars() = parseMetachars;
		gc->bSmoothed() = smoothed; gc->bTextTranslucent() = textTranslucent; gc->bTranslucent() = translucent; gc->bWordWrap() = wordWrap;
		gc->baselineOffset() = baselineOffset; gc->boldFont() = boldFont; gc->gcClipRect() = clipRect; gc->gcCount() = gcCount; gc->gcFree() = gcFree; gc->gcOwner() = gcOwner; gc->gcStack() = gcStack;
		gc->hMultiplier() = hMultiplier; gc->normalFont() = normalFont; gc->textPolyFlags() = textPolyFlags; gc->textVSpacing() = textVSpacing; gc->tileColor() = tileColor;
		gc->underlineHeight() = underlineHeight; gc->underlineTexture() = underlineTexture; gc->vMultiplier() = vMultiplier; gc->SpecialTextEnabled = specialText;
		gc->offsetX = offsetX; gc->offsetY = offsetY;
	});
	// A script that never releases: the oldest kept state dropped past 64
	if (WindowGCStack.size() > 64)
		WindowGCStack.erase(WindowGCStack.begin());
	ResetWindowGC(window, offsetX, offsetY);
}

void RenderSubsystem::PopWindowGC()
{
	if (WindowGCStack.empty())
		return;
	WindowGCStack.back()();
	WindowGCStack.pop_back();
}

// The original's XWindow::DrawTree (Extension.dll 0x10051490) draws a window
// only while it is shown and some of it shows, its clip rectangle's width and
// height both above 0 (UWindow::ClipTree). Of one scrolled out of its clip
// window, of no width or height, or past its parent's edge it draws nothing:
// no background, no DrawWindow or PostDrawWindow, none of its children.
void RenderSubsystem::DrawWindow(UWindow* window, float offsetX, float offsetY)
{
	const ClipRect& clip = window->winClipRect();
	if (!window->bIsVisible() || clip.clipWidth <= 0.0f || clip.clipHeight <= 0.0f)
		return;

	offsetX += window->UsedX;
	offsetY += window->UsedY;

	if (window->parentOwner())
		engine->dxgc->PushClip(engine->dxgc->ScaleRect(Rectf::xywh(offsetX, offsetY, window->Width(), window->Height())));

	// The window's background, then its own drawing, as the original's
	// XWindow::DrawTree has them.
	ResetWindowGC(window, offsetX, offsetY);
	window->DrawBackground(engine->dxgc);
	window->DrawWindow(engine->dxgc);

	for (UWindow* child = window->firstChild(); child; child = child->nextSibling())
	{
		DrawWindow(child, offsetX, offsetY);
	}

	ResetWindowGC(window, offsetX, offsetY);
	window->PostDrawWindow(engine->dxgc);

	if (window->parentOwner())
		engine->dxgc->PopClip();
}

void RenderSubsystem::DrawWindowInfo(UFont* font, UWindow* window, int depth, float& curY)
{
	if (!window->bIsVisible())
		return;

	std::string text = UObject::GetUClassFullName(window).ToString();
	text += " - ";
	text += typeid(*window).name();

	float x = window->UsedX;
	float y = window->UsedY;
	float w = window->Width();
	float h = window->Height();
	text += " x = " + std::to_string((int)x);
	text += " y = " + std::to_string((int)y);
	text += " w = " + std::to_string((int)w);
	text += " h = " + std::to_string((int)h);

	vec4 color = vec4(!window->bNeedsReconfigure() ? 1.0f : 0.5f);
	if (!window->bNeedsReconfigure() && (window->Width() <= 0.5f || window->Height() <= 0.5f))
		color = vec4(1.0f, 0.2f, 0.2f, 1.0f);
	if (window == engine->dxRootWindow->FocusWindow())
		color = vec4(0.5f, 1.0f, 0.5f, 1.0f);
	if (window == engine->dxRootWindow->lastMouseWindow())
		color = vec4(0.5f, 0.5f, 1.0f, 1.0f);
	float curX = depth * 20.0f, curXL = 0.0f, curYL = 0.0f;
	DrawText(font, color, 0.0f, 0.0f, curX, curY, curXL, curYL, false, text, PF_NoSmooth | PF_Masked, false);
	for (UWindow* child = window->lastChild(); child; child = child->prevSibling())
	{
		DrawWindowInfo(font, child, depth + 1, curY);
	}
}
