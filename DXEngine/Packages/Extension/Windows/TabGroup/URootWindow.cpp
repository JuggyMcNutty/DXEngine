
#include "Precomp.h"
#include <chrono>
#include "URootWindow.h"
#include "Utils/Logger.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/Resources/USound.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "Packages/Extension/Windows/UGC.h"
#include "Engine.h"
#include "Packages/Extension/Windows/UBorderWindow.h"
#include "Packages/Core/Properties/UStructProperty.h"
#include "Packages/Core/Properties/UObjectProperty.h"
#include "Packages/Core/Properties/UFloatProperty.h"
#include "Packages/Core/UFunction.h"
#include "VM/ScriptCall.h"
#include "GameWindow.h"
#include "RenderDevice/RenderDevice.h"
#include "Package/PackageManager.h"
#include "Packages/DeusEx/UDeusExSaveInfo.h"
#include "Packages/Engine/Resources/UPalette.h"
#include "Render/RenderSubsystem.h"

void URootWindow::EnablePositionalSound(std::optional<bool> bEnable)
{
	bPositionalSound() = !bEnable || *bEnable;
}

void URootWindow::EnableRendering(std::optional<bool> newRender)
{
	bRender() = !newRender || *newRender;
}

UObject* URootWindow::GenerateSnapshot(std::optional<bool> bFilter)
{
	return MakeSnapshot(nullptr, engine->packages->GetTransientPackage());
}

// The original's (dx-reverse-info/extension-dll.md, save pictures): the frame last
// drawn, averaged down to the size SetSnapshotSize gave -- each pixel the
// mean of the box of pixels it covers, its channels scaled by 256/255 --
// and kept as the mean of its three channels in an 8-bit texture with a
// grey palette, its sizes rounded up to powers of two. Into the texture
// given, or a new one in the package given.
UTexture* URootWindow::MakeSnapshot(UTexture* texture, Package* package)
{
	int width = snapshotWidth();
	int height = snapshotHeight();
	if (width <= 0 || height <= 0)
		return nullptr;

	Array<TextureColor> pixels;
	int srcWidth = 0, srcHeight = 0;
	if (!engine->render || !engine->render->ReadLastFrame(pixels, srcWidth, srcHeight))
		return nullptr;

	if (!texture)
	{
		UClass* textureClass = engine->packages->FindClass("Engine.Texture");
		texture = UObject::Cast<UTexture>(package->NewObject("Snapshot", textureClass, ObjectFlags::NoFlags));
	}

	UPalette* palette = texture->Palette();
	if (!palette || palette->package != texture->package)
	{
		UClass* paletteClass = engine->packages->FindClass("Engine.Palette");
		palette = UObject::Cast<UPalette>(texture->package->NewObject("SnapshotPalette", paletteClass, ObjectFlags::NoFlags));
	}
	palette->Colors.resize(256);
	for (uint32_t i = 0; i < 256; i++)
		palette->Colors[i] = i | (i << 8) | (i << 16) | 0xff000000;

	int usize = 1, ubits = 0;
	while (usize < width) { usize <<= 1; ubits++; }
	int vsize = 1, vbits = 0;
	while (vsize < height) { vsize <<= 1; vbits++; }

	UnrealMipmap mip;
	mip.Width = usize;
	mip.Height = vsize;
	mip.UBits = ubits;
	mip.VBits = vbits;
	mip.Data.resize((size_t)usize * vsize, 0);

	// The box walks the frame in steps of the frame's size over the
	// snapshot's, a step of whole pixels from where it truncates to.
	float xStep = srcWidth / (float)width;
	float yStep = srcHeight / (float)height;
	int dstY = 0;
	for (float srcY = 0.0f; srcY < (float)srcHeight; srcY += yStep, dstY++)
	{
		int dstX = 0;
		for (float srcX = 0.0f; srcX < (float)srcWidth; srcX += xStep, dstX++)
		{
			if (dstX >= width || dstY >= height)
				continue;

			double sum[3] = {};
			int count = 0;
			for (int i = 0; (float)i < xStep; i++)
			{
				for (int j = 0; (float)j < yStep; j++)
				{
					if ((float)i + srcX < (float)srcWidth && (float)j + srcY < (float)srcHeight)
					{
						const TextureColor& p = pixels[((size_t)srcY + j) * srcWidth + (size_t)srcX + i];
						sum[0] += p.R;
						sum[1] += p.G;
						sum[2] += p.B;
						count++;
					}
				}
			}
			if (count == 0)
				continue;

			int grey = 0;
			for (double channel : sum)
			{
				float scaled = (float)(channel / 255.0 / count) * 256.0f;
				grey += std::clamp((int)std::nearbyint(scaled - 0.5f), 0, 255);
			}
			mip.Data[(size_t)dstY * usize + dstX] = (uint8_t)(grey / 3);
		}
	}

	texture->Format() = (uint8_t)TextureFormat::P8;
	texture->Palette() = palette;
	texture->USize() = usize;
	texture->VSize() = vsize;
	texture->UBits() = (uint8_t)ubits;
	texture->VBits() = (uint8_t)vbits;
	texture->UClamp() = usize;
	texture->VClamp() = vsize;
	texture->MaxColor() = { 255, 255, 255, 255 };
	texture->bRealtimeChanged() = true;
	texture->UncompressedMipmaps.clear();
	texture->UncompressedMipmaps.push_back(std::move(mip));
	texture->UsedMipmaps = texture->UncompressedMipmaps;
	texture->UsedFormat = TextureFormat::P8;
	texture->TextureModified = true;
	return texture;
}

bool URootWindow::IsPositionalSoundEnabled()
{
	return bPositionalSound();
}

bool URootWindow::IsRenderingEnabled()
{
	return bRender();
}

// With movement locked the pointer stays where it is; with buttons locked the
// UI takes and ignores them. The Customize Keys screen locks movement while
// it waits for a key (extension-dll.md, The root window).
void URootWindow::LockMouse(std::optional<bool> bLockMove, std::optional<bool> bLockButton)
{
	bMouseMoveLocked() = bLockMove.value_or(false);
	bMouseButtonLocked() = bLockButton.value_or(false);
}

void URootWindow::SetDefaultEditCursor(std::optional<UObject*> newEditCursor)
{
	if (newEditCursor)
		defaultEditCursor() = UObject::Cast<UTexture>(*newEditCursor);
}

void URootWindow::SetDefaultMovementCursors(std::optional<UObject*> newMovementCursor, std::optional<UObject*> newHorizontalMovementCursor, std::optional<UObject*> newVerticalMovementCursor, std::optional<UObject*> newTopLeftMovementCursor, std::optional<UObject*> newTopRightMovementCursor)
{
	if (newMovementCursor)
		DefaultMoveCursor() = UObject::Cast<UTexture>(*newMovementCursor);
	if (newHorizontalMovementCursor)
		defaultHorizontalMoveCursor() = UObject::Cast<UTexture>(*newHorizontalMovementCursor);
	if (newVerticalMovementCursor)
		defaultVerticalMoveCursor() = UObject::Cast<UTexture>(*newVerticalMovementCursor);
	if (newTopLeftMovementCursor)
		defaultTopLeftMoveCursor() = UObject::Cast<UTexture>(*newTopLeftMovementCursor);
	if (newTopRightMovementCursor)
		defaultTopRightMoveCursor() = UObject::Cast<UTexture>(*newTopRightMovementCursor);
}

void URootWindow::SetRawBackground(std::optional<UObject*> NewTexture, std::optional<Color> NewColor)
{
	if (NewTexture)
		rawBackground() = UObject::Cast<UTexture>(*NewTexture);
	if (NewColor)
		rawColor() = *NewColor;
}

void URootWindow::SetRawBackgroundSize(float newWidth, float NewHeight)
{
	rawBackgroundWidth() = newWidth;
	rawBackgroundHeight() = NewHeight;
}

void URootWindow::SetRenderViewport(float newX, float newY, float newWidth, float NewHeight)
{
	renderX() = newX;
	renderY() = newY;
	renderWidth() = newWidth;
	renderHeight() = NewHeight;
	RenderViewportSet = true;
}

void URootWindow::ResetRenderViewport()
{
	RenderViewportSet = false;
}

void URootWindow::SetSnapshotSize(float newWidth, float NewHeight)
{
	snapshotWidth() = (int)newWidth;
	snapshotHeight() = (int)NewHeight;
}

void URootWindow::ShowCursor(std::optional<bool> bShow)
{
	bCursorVisible() = !bShow || *bShow;
}

void URootWindow::StretchRawBackground(std::optional<bool> bStretch)
{
	bStretchRawBackground() = !bStretch || *bStretch;
}

void URootWindow::WindowReady()
{
	SetRootCursorPos(GetVirtualWidth() * 0.5f, GetVirtualHeight() * 0.5f);
	// Multiple clicks: within half a second and 10 units (XRootWindow::Init)
	multiClickTimeout() = 0.5f;
	maxMouseDist() = 10.0f;
	UModalWindow::WindowReady();
}

// The root's own tick (Extension.dll XRootWindow::Tick 0x1003a540), which
// the original's walk runs before the root's script Tick, its timers and
// every other window's tick (XRootWindow::TickTree 0x1003bfc0): while nothing
// has the focus, the first focus is seeded (SeedFocus). So the windows made
// in a frame -- a conversation's choices, a menu's buttons -- take it at the
// windows' next tick, a frame on, their places laid out by then.
void URootWindow::Tick(float timeElapsed)
{
	SeedFocus();
	UModalWindow::Tick(timeElapsed);
}

// The root asks its script of a child's request, where every other tab group
// passes it up unasked (XRootWindow::ChildRequestedReconfiguration
// 0x1003a6d0): the base Window script's answer places the child by its
// alignment (ResizeChild).
bool URootWindow::ChildRequestedReconfiguration(UWindow* childWin)
{
	return UWindow::ChildRequestedReconfiguration(childWin);
}

// The original's (XRootWindow::ResizeRoot 0x1003adf0), each frame from its
// PreRender: the root's one multiplier, and its size the screen's in its
// units, laid out again only when that changes -- every child but a left-
// and top-aligned one placed again by its alignment (ChangeConfiguration).
// Every other window is laid out as it asks (AskParentForReconfigure).
// DXEngine's root is 4:3, centred on a wider screen, and its children lie
// across the whole screen (RootOffsetX): a change of the screen lays
// them out again whether the root's size changes or not, and moves the left-
// and top-aligned ones with it.
void URootWindow::ResizeRoot()
{
	int viewportWidth, viewportHeight;
	GetScreenSize(viewportWidth, viewportHeight);
	if (viewportWidth == LaidOutViewportWidth && viewportHeight == LaidOutViewportHeight)
		return;
	LaidOutViewportWidth = viewportWidth;
	LaidOutViewportHeight = viewportHeight;

	float virtualWidth = GetVirtualWidth();
	float virtualHeight = GetVirtualHeight();
	float virtualScale = GetVirtualScale();
	hMultiplier() = (int)virtualScale;
	vMultiplier() = (int)virtualScale;

	// The 4:3 root centred to a whole unit, as RootOffsetX takes its
	// children back by whole units: a root child at 0 then lies on a whole
	// unit at the screen's edge, as on the original's root, the
	// viewport, at 0, and each tile the GC puts on whole units (XGC::DrawTile,
	// Extension.dll 0x1002a820) on the original's pixels: at 2560x1440 the
	// root 107 units in, not 106.67.
	UsedX = std::round((viewportWidth - virtualWidth * virtualScale) * 0.5f / virtualScale);
	UsedY = 0.0f;

	bNeedsReconfigure() = true;
	ConfigureChild(0.0f, 0.0f, virtualWidth, virtualHeight);
	for (UWindow* child = firstChild(); child; child = child->nextSibling())
		child->UsedX = child->X() + child->RootOffsetX();
	ClipTree();
}

bool URootWindow::IsCursorVisible()
{
	// The pointer is drawn while the mouse is grabbed -- a modal grabs it as
	// it shows and lets it go as it hides -- and ShowCursor has not hidden
	// it, as the original's XRootWindow::PaintWindows draws it
	// (dx-reverse-info/extension-dll.md): the multiplayer message window
	// hides it while it is up.
	return bCursorVisible() && IsMouseGrabbed();
}

void URootWindow::PostDrawWindow(UGC* gc)
{
	UModalWindow::PostDrawWindow(gc);
	if (IsCursorVisible())
	{
		// The original's XWindow::QueryCursor (Extension.dll 0x1004f910), from
		// the window under the pointer up its parents: each asked
		// CursorRequested with the point in its own coordinates (a dragged
		// item's icon, a menu's tinted pointer), else its defaultCursor with
		// its shadow, hot spot and colour; a hot spot outside the texture is
		// its middle less one. Drawn as PaintWindows (0x1003aab0) draws it:
		// the shadow modulated, then the cursor masked in its colour, both at
		// the hot spot. (The nearest defaultCursor, drawn white and centred,
		// no CursorRequested asked, showed no dragged item, no shadow and no
		// theme's tint.)
		float relativeX = 0.0f, relativeY = 0.0f;
		UWindow* mouseWindow = GetCursorFocus(relativeX, relativeY);
		if (!mouseWindow)
		{
			mouseWindow = this;
			relativeX = MouseX();
			relativeY = MouseY();
		}

		UTexture* cursor = nullptr;
		UTexture* shadow = nullptr;
		float hotX = -1.0f, hotY = -1.0f;
		Color color = { 255, 255, 255, 255 };
		for (UWindow* w = mouseWindow; w && !cursor; w = w->parentOwner())
		{
			float pointX = relativeX, pointY = relativeY;
			if (w != mouseWindow)
				ConvertCoordinates(mouseWindow, relativeX, relativeY, w, pointX, pointY);
			// A resizeable border's own move cursor first, its native
			// CursorRequested's (XBorderWindow, 0x100016c0)
			if (UBorderWindow* border = UObject::TryCast<UBorderWindow>(w))
			{
				if (border->bResizeable())
					cursor = border->ResizeCursor(pointX, pointY);
			}
			UFunction* func = cursor ? nullptr : FindEventFunction(w, "CursorRequested");
			if (func)
			{
				if (!engine->colorprop->Struct && func->Properties.size() > 5)
					if (auto structProp = UObject::TryCast<UStructProperty>(func->Properties[5]))
						engine->colorprop->Struct = structProp->Struct;
				float askHotX = -1.0f, askHotY = -1.0f;
				Color askColor = { 255, 255, 255, 255 };
				UObject* askShadow = nullptr;
				ExpressionValue asked = CallEvent(w, "CursorRequested", {
					ExpressionValue::ObjectValue(mouseWindow),
					ExpressionValue::FloatValue(pointX),
					ExpressionValue::FloatValue(pointY),
					ExpressionValue::Variable(&askHotX, engine->floatprop),
					ExpressionValue::Variable(&askHotY, engine->floatprop),
					ExpressionValue::Variable(&askColor, engine->colorprop),
					ExpressionValue::Variable(&askShadow, engine->objprop) });
				cursor = UObject::TryCast<UTexture>(asked.ToObject());
				if (cursor)
				{
					shadow = UObject::TryCast<UTexture>(askShadow);
					hotX = askHotX;
					hotY = askHotY;
					color = askColor;
				}
			}
			if (!cursor)
			{
				if (UTexture* tex = UObject::Cast<UTexture>(w->defaultCursor()))
				{
					cursor = tex;
					shadow = w->defaultCursorShadow();
					hotX = w->defaultHotX();
					hotY = w->defaultHotY();
					color = w->defaultCursorColor();
				}
			}
		}

		if (cursor)
		{
			if (hotX < 0.0f || (float)cursor->USize() <= hotX)
				hotX = (float)((cursor->USize() >> 1) - 1);
			if (hotY < 0.0f || (float)cursor->VSize() <= hotY)
				hotY = (float)((cursor->VSize() >> 1) - 1);
			float x = MouseX() - hotX;
			float y = MouseY() - hotY;
			if (shadow)
			{
				gc->SetStyle(EDrawStyle::Modulated);
				gc->DrawIcon(x, y, shadow);
			}
			gc->SetStyle(EDrawStyle::Masked);
			gc->SetTileColor(color);
			gc->DrawIcon(x, y, cursor);
		}
	}
}

// The original's (XWindow::SetFocusWindow 0x1004f0e0): the focus goes to
// the nearest selectable window up from the one asked for, only if it can
// take it now (traversable in the topmost modal), and its modal keeps it as
// the window to come back to. The window that had the focus is told and
// each of its ancestors told it left; the new one is scrolled into view (a
// clip window shows it), told, and each of its ancestors told it came --
// a menu's help line follows the focus by this. Nothing asked for clears
// the focus.
bool URootWindow::SetRootFocusWindow(UWindow* newFocusWindow)
{
	UWindow* target = newFocusWindow;
	for (UWindow* w = newFocusWindow; w; w = w->parentOwner())
	{
		if (w->bIsSelectable())
		{
			target = w;
			break;
		}
	}
	if (FocusWindow() == target)
		return true;
	if (target)
	{
		if (!target->IsTraversable(true))
			return false;
		if (UModalWindow* modal = UObject::TryCast<UModalWindow>(target->GetModalWindow()))
			modal->preferredFocus() = target;
	}

	UWindow* oldFocusWindow = FocusWindow();
	FocusWindow() = target;
	if (oldFocusWindow)
	{
		oldFocusWindow->FocusLeftWindow();
		oldFocusWindow->PlaySound(oldFocusWindow->unfocusSound(), {}, {}, {}, {});
	}
	for (UWindow* w = oldFocusWindow; w; w = w->parentOwner())
		w->FocusLeftDescendant(oldFocusWindow);
	if (target)
	{
		target->AskParentToShowArea(0.0f, 0.0f, target->Width(), target->Height());
		target->PlaySound(target->focusSound(), {}, {}, {}, {});
		target->FocusEnteredWindow();
	}
	for (UWindow* w = target; w; w = w->parentOwner())
		w->FocusEnteredDescendant(target);
	return true;
}

void URootWindow::SetRootCursorPos(float newMouseX, float newMouseY)
{
	// Clip cursor to the entire screen, not the root window box:

	newMouseX += UsedX;
	newMouseY += UsedY;

	int width, height;
	GetScreenSize(width, height);
	float scale = GetVirtualScale();
	float realWidth = std::ceil(width / scale);
	float realHeight = std::ceil(height / scale);

	newMouseX = std::max(newMouseX, 0.0f);
	newMouseY = std::max(newMouseY, 0.0f);
	newMouseX = std::min(newMouseX, realWidth);
	newMouseY = std::min(newMouseY, realHeight);

	newMouseX -= UsedX;
	newMouseY -= UsedY;

	// Apply the new cursor pos:

	prevMouseX() = MouseX();
	prevMouseY() = MouseY();
	MouseX() = newMouseX;
	MouseY() = newMouseY;

	// The original's ProcessMouseMove (XRootWindow 0x1003af50): a new window
	// under the pointer is left and entered, each told alone, never its
	// parents; the topmost modal's mode moves the focus with it -- cleared
	// on leaving under MFOCUS_EnterLeave, given to the window entered (its
	// nearest selectable one) under MFOCUS_Enter and EnterLeave -- so a
	// conversation's choice and a menu's button take the focus the pointer
	// moves onto. MouseMoved only when the place changed.
	float relativeX = 0.0f, relativeY = 0.0f;
	UWindow* focus = GetCursorFocus(relativeX, relativeY);
	UModalWindow* modal = TopmostModal();
	uint8_t mode = modal ? modal->focusMode() : focusMode();

	auto lastFocus = UObject::Cast<UWindow>(lastMouseWindow());
	if (lastFocus != focus)
	{
		if (lastFocus)
		{
			lastFocus->MouseLeftWindow();
			if (mode >= (uint8_t)EMouseFocusMode::EnterLeave)
				SetRootFocusWindow(nullptr);
		}
		lastMouseWindow() = focus;
		focus->MouseEnteredWindow();
		if (mode >= (uint8_t)EMouseFocusMode::Enter)
			SetRootFocusWindow(focus);
	}

	if (newMouseX != movedMouseX || newMouseY != movedMouseY)
	{
		movedMouseX = newMouseX;
		movedMouseY = newMouseY;
		focus->MouseMoved(relativeX, relativeY);
	}
}

// The window under a point (Extension.dll XWindow::FindWindowByPoint
// 0x1004f740): one that shows, is sensitive and holds the point, its
// deepest such child that is no modal, topmost first.
static UWindow* FindWindowByPoint(UWindow* window, float x, float y, float& relativeX, float& relativeY)
{
	if (!window->bIsVisible() || !window->bIsSensitive() || x < 0.0f || y < 0.0f || x >= window->Width() || y >= window->Height())
		return nullptr;
	for (UWindow* child = window->lastChild(); child; child = child->prevSibling())
	{
		if (child->windowType() < 2)
		{
			if (UWindow* found = FindWindowByPoint(child, x - child->UsedX, y - child->UsedY, relativeX, relativeY))
				return found;
		}
	}
	relativeX = x;
	relativeY = y;
	return window;
}

// The window the mouse acts on (Extension.dll XRootWindow::GetMouseWindow
// 0x1003b0d0): the one that grabbed it, else the one under the pointer in
// the topmost modal, else that modal (or the root, with none up).
UWindow* URootWindow::GetCursorFocus(float& relativeX, float& relativeY)
{
	UWindow* window = grabbedWindow();
	if (!window)
	{
		UWindow* holder = MouseHolder();
		float x = 0.0f, y = 0.0f;
		ConvertCoordinates(this, MouseX(), MouseY(), holder, x, y);
		window = FindWindowByPoint(holder, x, y, relativeX, relativeY);
		if (!window)
			window = holder;
	}
	ConvertCoordinates(this, MouseX(), MouseY(), window, relativeX, relativeY);
	return window;
}

// The topmost modal that shows, else the root.
UWindow* URootWindow::MouseHolder()
{
	for (UWindow* child = lastChild(); child; child = child->prevSibling())
	{
		if (child->bIsVisible() && child->windowType() >= 2)
			return child;
	}
	return this;
}

bool URootWindow::OnWindowMouseMove(const Point& pos)
{
#if 0 // We currently handle this in OnWindowRawMouseMove
	if (IsCursorVisible())
		SetRootCursorPos((float)pos.x / scale, (float)pos.y / scale);
#endif
	return IsMouseGrabbed();
}

bool URootWindow::OnWindowMouseDown(const Point& pos, EInputKey key)
{
	return HandleButton(key, true);
}

// Wayland and Win32 send a double click in place of the second press; the
// presses count the clicks themselves.
bool URootWindow::OnWindowMouseDoubleclick(const Point& pos, EInputKey key)
{
	return HandleButton(key, true);
}

bool URootWindow::OnWindowMouseUp(const Point& pos, EInputKey key)
{
	return HandleButton(key, false);
}

// The root's mark of a key or button (keyDownMap; Extension.dll
// XRootWindow::Process 0x1003b3e0): every press sets it and every release
// clears it, whether the UI takes them or not. Whether it was down before:
// a press while down is a repeat, and a release while up comes from no
// press the root saw.
bool URootWindow::MarkKey(EInputKey key, bool down)
{
	if ((int)key >= 255)
		return false;
	uint8_t& mark = (&keyDownMap())[(int)key];
	bool wasDown = mark != 0;
	mark = down ? 1 : 0;
	return wasDown;
}

// The original's (dx-reverse-info/extension-dll.md, the root window): the
// UI takes the buttons only while a window holds the mouse, as every shown
// modal does. Each goes to the window the mouse acts on, then up its
// parents, each asked RawMouseButtonPressed then MouseButtonPressed or
// Released until one handles it (XRootWindow::HandleButtons 0x1003b960). A
// press grabs the mouse for that window, and gives it focus when the modal
// sets a focusMode; a release of any button lets the grab go. Presses of
// one button on one window, each within multiClickTimeout of the last and
// maxMouseDist of the first, count up, wrapped at the window's maxClicks.
bool URootWindow::HandleButton(EInputKey key, bool press)
{
	bool wasDown = MarkKey(key, press);
	if (!IsMouseGrabbed())
		return false;
	// A release of a button not down is taken, and goes to no window.
	if (!press && !wasDown)
		return true;
	if (bMouseButtonLocked())
		return true;

	UWindow* holder = MouseHolder();
	float relativeX = 0.0f, relativeY = 0.0f;
	UWindow* window = GetCursorFocus(relativeX, relativeY);

	if (press)
	{
		window->GrabMouse();
		UModalWindow* modal = UObject::TryCast<UModalWindow>(holder);
		if (modal && modal->focusMode() != 0)
		{
			// SetFocusWindow takes the nearest selectable window up from it
			UWindow* selectable = window;
			while (selectable && !selectable->bIsSelectable())
				selectable = selectable->parentOwner();
			if (selectable && selectable->IsTraversable(true))
			{
				for (UWindow* w = selectable; w; w = w->parentOwner())
				{
					if (UModalWindow* owner = UObject::TryCast<UModalWindow>(w))
					{
						owner->preferredFocus() = selectable;
						break;
					}
				}
				SetRootFocusWindow(selectable);
			}
		}
	}
	else
	{
		window->UngrabMouse();
	}

	// The windows' clock: real seconds (XRootWindow::GetWindowsTickOffset)
	static const auto clockStart = std::chrono::steady_clock::now();
	float now = std::chrono::duration<float>(std::chrono::steady_clock::now() - clockStart).count();
	float dx = std::abs(MouseX() - firstButtonMouseX());
	float dy = std::abs(MouseY() - firstButtonMouseY());
	float left = lastButtonPress() - now;
	if (left <= 0.0f || window != lastButtonWindow() || dx > maxMouseDist() || dy > maxMouseDist() || (int)key != lastButtonType())
	{
		left = 0.0f;
		lastButtonPress() = 0.0f;
		lastButtonType() = (int)key;
		lastButtonWindow() = window;
		clickCount() = 0;
	}
	if (press)
	{
		if (left > 0.0f)
			clickCount()++;
		lastButtonPress() = now + multiClickTimeout();
		if (clickCount() < 1)
		{
			firstButtonMouseX() = MouseX();
			firstButtonMouseY() = MouseY();
		}
	}

	for (UWindow* cur = window; cur; cur = cur->parentOwner())
	{
		ConvertCoordinates(this, MouseX(), MouseY(), cur, relativeX, relativeY);
		if (cur->RawMouseButtonPressed(relativeX, relativeY, key, press ? EInputType::IST_Press : EInputType::IST_Release))
			return true;
		int numClicks = clickCount();
		if (cur->maxClicks() > 0)
			numClicks %= cur->maxClicks();
		numClicks++;
		if (press ? cur->MouseButtonPressed(relativeX, relativeY, key, numClicks) : cur->MouseButtonReleased(relativeX, relativeY, key, numClicks))
			return true;
	}
	return true;
}

bool URootWindow::OnWindowMouseWheel(const Point& pos, EInputKey key)
{
	if (!OnWindowMouseDown(pos, key))
	{
		// The game takes the press and the release
		MarkKey(key, false);
		return false;
	}

	OnWindowMouseUp(pos, key);
	return true;
}

// The original's (XRootWindow::SetMouseDelta 0x1003b2e0; extension-dll.md,
// pointer movement): the mouse's counts add to the pointer in the root's
// units, not divided by the multiplier, both axes doubled when either moves
// more than 2 -- at 1280x960 a count moves the pointer two pixels, as there
bool URootWindow::OnWindowRawMouseMove(int dx, int dy)
{
	if (IsCursorVisible() && !bMouseMoveLocked())
	{
		float moveX = (float)dx;
		float moveY = (float)dy;
		if (std::abs(moveX) > 2.0f || std::abs(moveY) > 2.0f)
		{
			moveX += moveX;
			moveY += moveY;
		}
		SetRootCursorPos(MouseX() + moveX, MouseY() + moveY);
	}
	return IsMouseGrabbed();
}

// The original's (dx-reverse-info/extension-dll.md, the root window): while a
// window holds the keyboard -- every shown modal does -- a key goes to the
// focus window, else the topmost shown modal, and up its parents to the root
// until one handles it; windows that are not modal pass it over while Alt is
// down. The UI then takes every key, handled or not.
bool URootWindow::RouteKey(const std::function<bool(UWindow*)>& handle)
{
	if (!IsKeyboardGrabbed())
		return false;

	UWindow* start = FocusWindow() ? FocusWindow() : TopmostModal();
	bool alt = IsKeyDown(IK_Alt);
	for (UWindow* cur = start; cur; cur = cur->parentOwner())
	{
		if (alt && !UObject::TryCast<UModalWindow>(cur))
			continue;
		if (handle(cur))
			break;
	}
	return true;
}

// A typed character goes the keys' way, Alt held or not: each window sends
// what a key types with Alt as the original's gets it in a WM_SYSCHAR (the
// Win32 one that message, the X11, Wayland and SDL2 ones their text, the
// SDL3 one the layout's character), and a modal on top presses the window
// whose accelerator it is (UModalWindow::KeyPressed).
bool URootWindow::OnWindowKeyChar(const ScriptString& chars)
{
	return RouteKey([&](UWindow* w) { return w->KeyPressed(chars); });
}

bool URootWindow::OnWindowKeyDown(EInputKey key)
{
	bool repeat = MarkKey(key, true);
	return RouteKey([&](UWindow* w) { return w->RawKeyPressed(key, EInputType::IST_Press, repeat) || w->VirtualKeyPressed(key, repeat); });
}

// Keys held as the window loses the keyboard are let go unseen: none counts
// as down when it comes back (an Alt held into Alt+Tab would else stay).
void URootWindow::ForgetKeys()
{
	for (int i = 0; i < 255; i++)
		(&keyDownMap())[i] = 0;
}

bool URootWindow::OnWindowKeyUp(EInputKey key)
{
	// To do: fire these for specific window types
	// event bool ButtonActivated(Window button)
	// event bool ToggleChanged(Window button, bool bNewToggle)
	// event bool BoxOptionSelected(Window box, int buttonNumber)
	// event bool ListRowActivated(window list, int rowId)
	// event bool ListSelectionChanged(window list, int numSelections, int focusRowId)

	// A release of a key not down is taken while the keyboard is grabbed, and
	// goes to no window; with no grab, the game gets it.
	if (!MarkKey(key, false))
		return IsKeyboardGrabbed();

	return RouteKey([&](UWindow* w) { return w->RawKeyPressed(key, EInputType::IST_Release, false); });
}

UModalWindow* URootWindow::TopmostModal()
{
	for (UWindow* child = lastChild(); child; child = child->prevSibling())
	{
		if (child->bIsVisible())
		{
			if (UModalWindow* modal = UObject::TryCast<UModalWindow>(child))
				return modal;
		}
	}
	return nullptr;
}

void URootWindow::GrabKeyboardEvents()
{
	if (handleKeyboardRef() <= 0)
	{
		for (int i = 0; i < 255; i++)
			if (!IsMouseButton((EInputKey)i))
				(&keyDownMap())[i] = 0;
	}
	handleKeyboardRef()++;
}

void URootWindow::UngrabKeyboardEvents()
{
	if (handleKeyboardRef() > 0)
		handleKeyboardRef()--;
}

void URootWindow::GrabMouseEvents()
{
	if (handleMouseRef() <= 0)
	{
		for (int i = 0; i < 255; i++)
			if (IsMouseButton((EInputKey)i))
				(&keyDownMap())[i] = 0;
	}
	handleMouseRef()++;
}

void URootWindow::UngrabMouseEvents()
{
	if (handleMouseRef() > 0)
		handleMouseRef()--;
}

bool URootWindow::IsMouseButton(EInputKey key)
{
	switch (key)
	{
	case IK_LeftMouse: case IK_RightMouse: case IK_MiddleMouse:
	case IK_MouseX: case IK_MouseY: case IK_MouseZ: case IK_MouseW:
	case IK_MouseWheelUp: case IK_MouseWheelDown:
		return true;
	default:
		return false;
	}
}
