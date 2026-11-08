
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
// menu -- as the original draws it, before any window (Extension.dll
// DrawRawBackground 0x1003c720): over the root less what the scene covers
// (all of it with rendering on and no render viewport, nothing with
// rendering off, the viewport's rectangle with one set), each piece left
// stretched from the background's own size or tiled from its corner, in the
// raw colour and the normal style, smoothed as the root's GC has it --
// ShowSnapshot's SetBackgroundSmoothing turns it on for the snapshot
// (dx-reverse-info/extension-dll.md, the raw background). The original's
// root is the whole viewport (XRootWindow::ResizeRoot 0x1003adf0), so the
// raw background covers the screen; DXEngine's 4:3 root lies centred in it,
// the screen's left edge at -UsedX in its units. No window takes its place
// out of it: the original's viewport windows do (their bDrawRawBackground
// off), and none is up in the game while it shows.
void RenderSubsystem::DrawRawBackground(URootWindow* root)
{
	UTexture* tex = root->rawBackground();
	if (!tex)
		return;

	Rectf full = Rectf::xywh(-root->UsedX, -root->UsedY, UWindow::GetExtendedVirtualWidth(), root->Height());
	Array<Rectf> pieces;
	if (!root->bRender())
	{
		pieces.push_back(full);
	}
	else if (root->RenderViewportSet)
	{
		// The scene's rectangle as VisibleFrame::SetupSceneFrame places it,
		// one as wide or as tall as the root the screen's width or height.
		// Then the original's clip list subtraction: above the viewport,
		// below it, then left and right of it within its height.
		float renderLeft = root->renderX(), renderRight = root->renderX() + root->renderWidth();
		float renderTop = root->renderY(), renderBottom = root->renderY() + root->renderHeight();
		if (root->renderX() <= 0.0f && root->renderWidth() >= root->Width())
		{
			renderLeft = full.left;
			renderRight = full.right;
		}
		if (root->renderY() <= 0.0f && root->renderHeight() >= root->Height())
		{
			renderTop = full.top;
			renderBottom = full.bottom;
		}
		float cutLeft = std::max(renderLeft, full.left);
		float cutTop = std::max(renderTop, full.top);
		float cutRight = std::min(renderRight, full.right);
		float cutBottom = std::min(renderBottom, full.bottom);
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

	// The root's own GC, drawing on, the normal style and the raw colour;
	// its smoothing left as it is
	ResetWindowGC(root, root->UsedX, root->UsedY);
	engine->dxgc->SetStyle(EDrawStyle::Normal);
	engine->dxgc->SetTileColor(root->rawColor());
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

// The window's own GC as the original keeps it (XWindow::Init 0x1004bb30, its
// GC made by XGC::XGC 0x10026ad0): left and top aligned, word wrapped, the
// root's multipliers, and what the window's setters have put in it -- its
// fonts, its colours, its special and translucent text and its baseline, and
// its background's style (SetBackgroundStyle 0x1004d670, DSTY_None turning
// drawing off) and smoothing (SetBackgroundSmoothing 0x1004d700), with a
// background texture or none: what a window draws takes them, a button's
// texture its style.
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
	engine->dxgc->SetStyle((EDrawStyle)window->backgroundStyle());
	engine->dxgc->EnableSmoothing(window->bSmoothBackground());

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
	UGC::State state = gc->SaveState();
	WindowGCStack.push_back([gc, state]() { gc->RestoreState(state); });
	// A script that never releases: the oldest kept state dropped past 64
	if (WindowGCStack.size() > 64)
		WindowGCStack.erase(WindowGCStack.begin());
	ResetWindowGC(window, state.offsetX, state.offsetY);
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

	// The window's background, then its own drawing, its children and its
	// PostDrawWindow, as the original's XWindow::DrawTree has them, through
	// the window's own GC pushed once round them all: PostDrawWindow takes
	// the GC as DrawWindow left it, the children drawing through their own.
	ResetWindowGC(window, offsetX, offsetY);
	window->DrawBackground(engine->dxgc);
	window->DrawWindow(engine->dxgc);

	UGC::State drawn = engine->dxgc->SaveState();
	for (UWindow* child = window->firstChild(); child; child = child->nextSibling())
	{
		DrawWindow(child, offsetX, offsetY);
	}

	engine->dxgc->RestoreState(drawn);
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
	DrawText(font, color, 0.0f, 0.0f, curX, curY, curXL, curYL, false, UTF8ToScript(text), PF_NoSmooth | PF_Masked, false);
	for (UWindow* child = window->lastChild(); child; child = child->prevSibling())
	{
		DrawWindowInfo(font, child, depth + 1, curY);
	}
}
