
#include "Precomp.h"
#include "UWindow.h"
#include "UGC.h"
#include "UViewportWindow.h"
#include "TabGroup/URootWindow.h"
#include "TabGroup/UModalWindow.h"
#include "TabGroup/UTabGroupWindow.h"
#include "Text/UButtonWindow.h"
#include <algorithm>
#include <chrono>
#include <vector>
#include "Audio/AudioDevice.h"
#include "VM/ScriptCall.h"
#include "Engine.h"
#include "Render/RenderSubsystem.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/Properties/UFloatProperty.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/Subsystems/UDXEngineAudioDevice.h"
#include "Packages/Engine/Resources/UFont.h"
#include "Packages/Extension/UPlayerPawnExt.h"

// WindowReady for each window that shows and has not had it, its shown
// children first, bottom to top, then the window, as the original's root
// sends it each frame before it draws them (XRootWindow::InitializeWindows
// 0x1003bc80, InvokeWindowReady 0x1003bd20): the menus' first focus and a
// note list's scroll. No window is laid out here: each is laid out as it
// asks (AskParentForReconfigure), the root as the screen changes
// (URootWindow::ResizeRoot).
void UWindow::InvokeWindowReady()
{
	if (!bIsVisible())
		return;

	// The children as they were, as Tick walks them
	Array<UWindow*> children;
	for (UWindow* child = firstChild(); child; child = child->nextSibling())
		children.push_back(child);
	for (UWindow* child : children)
	{
		if (!child->Destroyed)
			child->InvokeWindowReady();
	}

	if (FirstDraw)
	{
		FirstDraw = false;
		WindowReady();
	}
}

// The root's children lie across the whole screen, as the original's root is
// the viewport, the 4:3 root centred in it: their x is the screen's, drawn
// less this.
float UWindow::RootOffsetX()
{
	UWindow* parent = parentOwner();
	if (!parent || parent != engine->dxRootWindow)
		return 0.0f;
	return -std::round(std::max(GetExtendedVirtualWidth() - parent->Width(), 0.0f) * 0.5f);
}

// The root is 4:3 (ConWindowActive.CalculateWindowSizes lays the
// conversation window out for it, and a wider screen extends the children
// across it, RootOffsetX), or the screen where that is narrower, as the
// original's root is the viewport: at 1280x1024 a 640-unit root, not 683
// centred 21 units off the screen's left edge.
float UWindow::GetVirtualWidth()
{
	int width, height;
	GetScreenSize(width, height);
	float screenWidth = width / GetVirtualScale();
	return std::min(std::round(GetVirtualHeight() * 4 / 3), screenWidth);
}

// The screen in the root's units, as the original's root is the viewport
// over its multiplier, unrounded (XRootWindow::ResizeRoot, Extension.dll
// 0x1003adf0): 853.33 by 533.33 at 2560x1600, so that the HUD's windows,
// placed and sized from it, lie where the original's do. The children
// extend across all of it instead of staying in the 4:3 box of the root
// window.
float UWindow::GetExtendedVirtualWidth()
{
	int width, height;
	GetScreenSize(width, height);
	return width / GetVirtualScale();
}

float UWindow::GetVirtualHeight()
{
	int width, height;
	GetScreenSize(width, height);
	return height / GetVirtualScale();
}

// A number through a script's printf format, as the original's appSprintf
// prints it (a list's float column, a scale's value). A format that is not a
// single floating-point conversion is taken as %f, where the original would
// print whatever it asks for.
std::string UWindow::FormatScriptFloat(const std::string& format, double value)
{
	int conversions = 0;
	bool valid = true;
	for (size_t i = 0; i < format.size() && valid; i++)
	{
		if (format[i] != '%')
			continue;
		i++;
		if (i < format.size() && format[i] == '%')
			continue;
		while (i < format.size() && strchr("-+ #0", format[i]))
			i++;
		while (i < format.size() && isdigit((unsigned char)format[i]))
			i++;
		if (i < format.size() && format[i] == '.')
		{
			i++;
			while (i < format.size() && isdigit((unsigned char)format[i]))
				i++;
		}
		valid = i < format.size() && strchr("fFeEgGaA", format[i]) != nullptr;
		conversions++;
	}
	char buffer[256];
	snprintf(buffer, sizeof(buffer), (valid && conversions == 1) ? format.c_str() : "%f", value);
	return buffer;
}

float UWindow::GetVirtualScale()
{
	// The original's multiplier (XRootWindow::ResizeRoot, Extension.dll
	// 0x1003adf0): the smaller of the viewport's width over 640 and its
	// height over 480, each truncated, at least 1 -- the largest whole
	// multiple of 640x480 the viewport holds
	int width, height;
	GetScreenSize(width, height);
	return (float)std::max(std::min(width / 640, height / 480), 1);
}

// A dedicated server has no viewport: the player's own root's windows, which
// its Possess makes there as the original's does (UPlayerPawnExt), are laid
// out on the base 640 by 480 and never drawn
void UWindow::GetScreenSize(int& width, int& height)
{
	if (engine->viewport)
	{
		width = engine->viewport->ViewportWidth();
		height = engine->viewport->ViewportHeight();
	}
	else
	{
		width = 640;
		height = 480;
	}
}

// The lookup under AddActorRef, RemoveActorRef and IsActorValid, as the
// original's (APlayerPawnExt::FindActor, Extension.dll 0x10038400): the list
// compacted first, a destroyed actor's entry and every None one dropped, then
// the actor's place in what is left, or -1.
static int FindActorRef(UPlayerPawnExt* playerPawn, UActor* target)
{
	int& count = playerPawn->actorCount();
	auto refs = playerPawn->actorList();
	int found = -1;
	int kept = 0;
	for (int i = 0; i < count; ++i)
	{
		ActorRef ref = refs[i];
		if (ref.Actor && ref.Actor->bDeleteMe())
			ref.Actor = nullptr;
		if (ref.Actor == target)
			found = kept;
		if (ref.Actor)
			refs[kept++] = ref;
	}
	count = kept;
	return found;
}

void UWindow::AddActorRef(UObject* refActor)
{
	UPlayerPawnExt* playerPawn = UObject::Cast<UPlayerPawnExt>(GetPlayerPawn());
	if (!playerPawn) return;
	UActor* target = UObject::Cast<UActor>(refActor);
	if (!target) return;

	int foundIndex = FindActorRef(playerPawn, target);
	int& count = playerPawn->actorCount();
	auto refs = playerPawn->actorList();

	if (foundIndex >= 0)
	{
		++refs[foundIndex].RefCount;
		return;
	}

	if (count >= 32)
	{
		for (int i = 1; i < count; ++i)
			refs[i - 1] = refs[i];
		--count;
	}

	refs[count].Actor = target;
	refs[count].RefCount = 1;
	++count;
}

// The original's (Extension.dll XWindow::AddTimer 0x1004fc20): due its time
// out, at least a microsecond, after now -- the windows' time since their
// last tick counted in -- and never 0 for its id; a window being destroyed
// takes none and gets 0. One with no function name calls nothing.
int UWindow::AddTimer(float TimeOut, std::optional<bool> bLoop, std::optional<int> clientData, std::optional<NameString> functionName)
{
	if (Destroyed)
		return 0;

	int id = ++NextTimerId;
	WTimer info;
	info.ClientData = clientData ? *clientData : 0;
	if (functionName)
		info.FunctionName = *functionName;
	info.Timeout = std::max(TimeOut, 0.000001f);
	info.Loop = bLoop ? *bLoop : false;
	info.TimeLeft = GetTickOffset() + info.Timeout;
	ActiveTimers[id] = info;
	return id;
}

void UWindow::RemoveTimer(int timerId)
{
	auto it = ActiveTimers.find(timerId);
	if (it != ActiveTimers.end())
		ActiveTimers.erase(it);
}

// The original's (XWindow::AskParentToShowArea 0x1004e230): of the area, the
// part inside the window; when the window's ancestors hide any of that, the
// parent is asked to show it -- a clip window scrolls to it -- and then asks
// its own parent, the area in its coordinates. Only a window that shows asks.
void UWindow::AskParentToShowArea(float areaX, float areaY, float areaWidth, float areaHeight)
{
	UWindow* parent = parentOwner();
	if (!parent || !IsShown())
		return;

	float x0 = std::max(areaX, 0.0f);
	float y0 = std::max(areaY, 0.0f);
	float x1 = std::min(areaX + areaWidth, Width());
	float y1 = std::min(areaY + areaHeight, Height());
	if (x1 <= x0 || y1 <= y0)
		return;

	// The part of the window its ancestors let show, in its coordinates
	// (the original keeps it as the window's clip rect).
	float visX0 = 0.0f, visY0 = 0.0f, visX1 = Width(), visY1 = Height();
	float offsetX = 0.0f, offsetY = 0.0f;
	for (UWindow* w = this; w->parentOwner(); w = w->parentOwner())
	{
		offsetX -= w->UsedX;
		offsetY -= w->UsedY;
		UWindow* ancestor = w->parentOwner();
		visX0 = std::max(visX0, offsetX);
		visY0 = std::max(visY0, offsetY);
		visX1 = std::min(visX1, offsetX + ancestor->Width());
		visY1 = std::min(visY1, offsetY + ancestor->Height());
	}
	float shownWidth = std::min(visX1, x1) - std::max(visX0, x0);
	float shownHeight = std::min(visY1, y1) - std::max(visY0, y0);
	if (shownWidth < x1 - x0 || shownHeight < y1 - y0)
	{
		parent->ChildRequestedShowArea(this, x0, y0, x1 - x0, y1 - y0);
		float parentX = 0.0f, parentY = 0.0f;
		ConvertCoordinates(this, x0, y0, parent, parentX, parentY);
		parent->AskParentToShowArea(parentX, parentY, x1 - x0, y1 - y0);
	}
}

std::string UWindow::CarriageReturn()
{
	// The original's execCarriageReturn (Extension.dll 0x10053e70) gives "\n"
	// alone; no window code handles '\r', which the GC drew as glyph 13 and
	// every note saved from a datacube or book carried.
	return "\n";
}

// The children as they were before the event, each told while still the
// window's: a handler that destroys one, or the next, cannot end the walk
static Array<UWindow*> ChildrenOf(UWindow* window)
{
	Array<UWindow*> children;
	for (UWindow* cur = window->firstChild(); cur; cur = cur->nextSibling())
		children.push_back(cur);
	return children;
}

static void ApplyStyleChange(UWindow* window)
{
	CallEvent(window, "StyleChanged", {});
	for (UWindow* cur : ChildrenOf(window))
	{
		if (cur->parentOwner() == window && !cur->Destroyed)
			ApplyStyleChange(cur);
	}
}

void UWindow::ChangeStyle()
{
	UWindow* cur = this;
	while (cur->parentOwner())
		cur = cur->parentOwner();
	ApplyStyleChange(cur);
}

void UWindow::ConvertCoordinates(UWindow* fromWin, float fromX, float fromY, UWindow* toWin, float& toX, float& toY)
{
	// Convert to global coordinates:
	float x = fromX;
	float y = fromY;
	for (UWindow* cur = fromWin; cur->parentOwner(); cur = cur->parentOwner())
	{
		x += cur->UsedX;
		y += cur->UsedY;
	}
	// Convert to local coordinates:
	for (UWindow* cur = toWin; cur->parentOwner(); cur = cur->parentOwner())
	{
		x -= cur->UsedX;
		y -= cur->UsedY;
	}
	toX = x;
	toY = y;
}

// The original's XWindow::ConvertScriptString (Extension.dll 0x10050710): a
// script's |n is a line break; every other | stays. The script natives that
// take text (SetText, AppendText, InsertText) pass it through this, so a
// window holds and GetText gives the break itself.
ScriptString UWindow::ConvertScriptString(const ScriptString& oldStr)
{
	ScriptString out;
	out.reserve(oldStr.size());
	for (size_t i = 0; i < oldStr.size(); i++)
	{
		if (oldStr[i] == '|' && i + 1 < oldStr.size() && oldStr[i + 1] == 'n')
		{
			out += '\n';
			i++;
		}
		else
		{
			out += oldStr[i];
		}
	}
	return out;
}

// A point in the world to this window's coordinates, as the original's
// (Extension.dll XRootWindow::ConvertVectorToCoordinates 0x10039f10, then
// XWindow's 0x1004faf0 down the windows): projected by the main scene's view
// into its pixels, then into the root window's coordinates as the root lays
// itself out -- a pixel over the UI's scale, less the offset that centres
// the 4:3 root on a wider screen -- truncated to whole units; a point at a
// depth of 1 or less at the view's middle, in whole pixels, unscaled. Less
// each window's place below the root. True when the point is in front of
// the eye, wherever it lands, so the frob highlight's box takes in a corner
// off the screen; under no root, the topmost window's middle, and false.
// In a viewport window, or a window inside one, through the nearest such
// window's own frame, as each window asks its parent and a viewport window
// answers itself (XViewportWindow's, 0x1004afc0): in that frame's pixels,
// less each window's place below it. One that has drawn no frame passes the
// question on, where the original's fails an assertion.
bool UWindow::ConvertVectorToCoordinates(const vec3& Location, float& relativeX, float& relativeY)
{
	for (UWindow* w = this; w; w = w->parentOwner())
	{
		UViewportWindow* viewport = UObject::TryCast<UViewportWindow>(w);
		if (viewport && viewport->LastFrame.Drawn)
		{
			float frameX, frameY;
			bool inFront = viewport->ProjectThroughFrame(Location, frameX, frameY);
			ConvertCoordinates(viewport, frameX, frameY, this, relativeX, relativeY);
			return inFront;
		}
	}

	UWindow* top = this;
	while (top->parentOwner())
		top = top->parentOwner();
	URootWindow* root = UObject::TryCast<URootWindow>(top);
	if (!root)
	{
		ConvertCoordinates(top, top->Width() * 0.5f, top->Height() * 0.5f, this, relativeX, relativeY);
		return false;
	}

	// No view to project through on a dedicated server
	if (!engine->render)
	{
		relativeX = 0.0f;
		relativeY = 0.0f;
		return false;
	}
	const SceneNode& view = engine->render->MainView;
	vec4 viewSpaceLocation = view.WorldToView * vec4(Location, 1.0f);
	bool inFront = viewSpaceLocation.z > 1.0f;
	float rootX, rootY;
	if (inFront)
	{
		vec4 projLocation = view.Projection * viewSpaceLocation;
		float rcpW = 1.0f / projLocation.w;
		float pixelX = view.XB + (projLocation.x * rcpW + 1.0f) * view.FX * 0.5f;
		float pixelY = view.YB + (projLocation.y * rcpW + 1.0f) * view.FY * 0.5f;
		float scale = GetVirtualScale();
		rootX = std::trunc(pixelX / scale - root->UsedX);
		rootY = std::trunc(pixelY / scale - root->UsedY);
	}
	else
	{
		rootX = std::trunc(view.FX2);
		rootY = std::trunc(view.FY2);
	}
	ConvertCoordinates(root, rootX, rootY, this, relativeX, relativeY);
	return inFront;
}

// The original's (Extension.dll XWindow::PreDestroy 0x10050500, CleanUp
// 0x1004bec0): a window that shows is hidden and every window made
// unselectable, so focus and grabs move away from it; its children are
// destroyed, then it gets DestroyWindow; then its parent gets ChildRemoved,
// every ancestor DescendantRemoved (a modal lets go of its preferred focus),
// the root lets go of it, and it leaves the tree.
void UWindow::Destroy()
{
	if (Destroyed)
		return;
	Destroyed = true;

	bool shown = true;
	for (UWindow* w = this; w; w = w->parentOwner())
	{
		if (!w->bIsVisible())
		{
			shown = false;
			break;
		}
	}
	if (shown)
		SetVisibility(false);
	SetSelectability(false);

	DestroyAllChildren();

	DestroyWindow();

	URootWindow* root = GetRootWindow();
	if (UWindow* parent = parentOwner())
	{
		parent->ChildRemoved(this);
		for (UWindow* ancestor = parent; ancestor; ancestor = ancestor->parentOwner())
		{
			ancestor->DescendantRemoved(this);
			if (UModalWindow* modal = UObject::TryCast<UModalWindow>(ancestor))
			{
				if (modal->preferredFocus() == this)
					modal->preferredFocus() = nullptr;
			}
		}
	}
	if (root)
	{
		if (root->lastButtonWindow() == this)
			root->lastButtonWindow() = nullptr;
		if (root->lastMouseWindow() == this)
			root->lastMouseWindow() = nullptr;
		if (root->grabbedWindow() == this)
			root->grabbedWindow() = nullptr;
	}
	ActiveTimers.clear();
	DetachFromParent();
}

void UWindow::DetachFromParent()
{
	if (prevSibling())
		prevSibling()->nextSibling() = nextSibling();
	if (nextSibling())
		nextSibling()->prevSibling() = prevSibling();
	if (parentOwner())
	{
		if (parentOwner()->firstChild() == this)
			parentOwner()->firstChild() = nextSibling();
		if (parentOwner()->lastChild() == this)
			parentOwner()->lastChild() = prevSibling();
	}
	prevSibling() = nullptr;
	nextSibling() = nullptr;
	parentOwner() = nullptr;
	winParent() = nullptr; // as the original's RemoveChild clears it
}

// Each child in turn, as the original's KillAllChildren (Extension.dll
// 0x100505d0) takes them: one already being destroyed (its handlers
// running, it still linked) is passed over, not waited on for ever, and a
// child a handler destroyed meanwhile has left the list.
void UWindow::DestroyAllChildren()
{
	for (;;)
	{
		UWindow* child = firstChild();
		while (child && child->Destroyed)
			child = child->nextSibling();
		if (!child)
			break;
		child->Destroy();
	}
}

void UWindow::DisableWindow()
{
	SetSensitivity(false);
}

void UWindow::EnableSpecialText(std::optional<bool> bEnable)
{
	bSpecialText() = !bEnable || *bEnable;
}

void UWindow::EnableTranslucentText(std::optional<bool> bEnable)
{
	bTextTranslucent() = !bEnable || *bEnable;
}

void UWindow::EnableWindow(std::optional<bool> bEnable)
{
	SetSensitivity(bEnable ? *bEnable : true);
}

UWindow* UWindow::FindWindow(float pointX, float pointY, float& relativeX, float& relativeY)
{
	// Convert to global coordinates and find root:
	float x = pointX;
	float y = pointY;
	UWindow* cur = this;
	while (cur->parentOwner())
	{
		x += cur->UsedX;
		y += cur->UsedY;
		cur = cur->parentOwner();
	}

	// Search the tree
	while (true)
	{
		UWindow* foundChild = nullptr;
		for (UWindow* child = cur->lastChild(); child; child = child->prevSibling())
		{
			if (child->bIsVisible() &&
				child->UsedX <= x &&
				child->UsedY <= y &&
				child->UsedX + child->Width() > x &&
				child->UsedY + child->Height() > y)
			{
				foundChild = child;
				break;
			}
		}
		if (!foundChild)
			break;
		cur = foundChild;
		x -= cur->UsedX;
		y -= cur->UsedY;
	}

	relativeX = x;
	relativeY = y;
	return cur;
}

UObject* UWindow::GetBottomChild(std::optional<bool> bVisibleOnly)
{
	bool visibleOnly = (!bVisibleOnly || *bVisibleOnly);
	for (UWindow* child = firstChild(); child; child = child->nextSibling())
	{
		if (!visibleOnly || child->bIsVisible())
			return child;
	}
	return nullptr;
}

UObject* UWindow::GetClientObject()
{
	return clientObject();
}

void UWindow::GetCursorPos(float& MouseX, float& MouseY)
{
	if (URootWindow* root = GetRootWindow())
	{
		ConvertCoordinates(root, root->MouseX(), root->MouseY(), this, MouseX, MouseY);
	}
	else
	{
		MouseX = 0.0f;
		MouseY = 0.0f;
	}
}

UObject* UWindow::GetFocusWindow()
{
	if (URootWindow* root = GetRootWindow())
	{
		return root->FocusWindow();
	}
	else
	{
		return nullptr;
	}
}

// The original's (XWindow::GetGC 0x1004ddc0): the one GC with the window's
// own state put in it, its state before kept until ReleaseGC
UObject* UWindow::GetGC()
{
	// A dedicated server has no render subsystem to keep the GC's states,
	// and draws nothing: its windows' scripts measure with the GC as it is
	if (engine->render)
		engine->render->PushWindowGC(this);
	return engine->dxgc;
}

UObject* UWindow::GetHigherSibling(std::optional<bool> bVisibleOnly)
{
	bool visibleOnly = (!bVisibleOnly || *bVisibleOnly);
	for (UWindow* cur = nextSibling(); cur; cur = cur->nextSibling())
	{
		if (!visibleOnly || cur->bIsVisible())
			return cur;
	}
	return nullptr;
}

UObject* UWindow::GetLowerSibling(std::optional<bool> bVisibleOnly)
{
	bool visibleOnly = (!bVisibleOnly || *bVisibleOnly);
	for (UWindow* cur = prevSibling(); cur; cur = cur->prevSibling())
	{
		if (!visibleOnly || cur->bIsVisible())
			return cur;
	}
	return nullptr;
}

UObject* UWindow::GetModalWindow()
{
	UWindow* cur = this;
	do
	{
		if (UObject::TryCast<UModalWindow>(cur))
			return cur;
		cur = cur->parentOwner();
	} while (cur);
	return nullptr;
}

UObject* UWindow::GetParent()
{
	return parentOwner();
}

UObject* UWindow::GetPlayerPawn()
{
	UWindow* cur = this;
	while (cur->parentOwner())
		cur = cur->parentOwner();
	URootWindow* root = UObject::TryCast<URootWindow>(cur);
	return root ? root->parentPawn() : nullptr;
}

URootWindow* UWindow::GetRootWindow()
{
	UWindow* cur = this;
	while (cur->parentOwner())
		cur = cur->parentOwner();
	return UObject::TryCast<URootWindow>(cur);
}

// The nearest tab group at or above the window (extension-dll.md, Small).
UObject* UWindow::GetTabGroupWindow()
{
	// The original walks up to the nearest window that is not a generic
	// child: a tab group, a modal or the root, any of which holds the
	// focus-order tables (Extension.dll XWindow::GetTabGroupWindow
	// 0x1004c560 -- ModalWindow extends TabGroupWindow, so a conversation
	// window is its choices' group).
	for (UWindow* cur = this; cur; cur = cur->parentOwner())
	{
		if (cur->windowType() != 0)
			return cur;
	}
	return nullptr;
}

// The windows' clock (XRootWindow::GetWindowsTickOffset 0x1003a3b0): real
// seconds since the root last ticked its windows, which restarts it -- the
// windows tick by it, not by the level's time. A held button's or scale's
// first repeat waits its delay plus the offset, so what of the frame had gone
// by when the press came is not lost (XWindow::GetTickOffset 0x1004fda0).
float UWindow::GetWindowsTickOffset(bool bRestart)
{
	using Clock = std::chrono::steady_clock;
	static Clock::time_point lastTick = Clock::now();
	Clock::time_point now = Clock::now();
	float offset = std::chrono::duration<float>(now - lastTick).count();
	if (bRestart)
		lastTick = now;
	return offset;
}

float UWindow::GetTickOffset()
{
	return GetWindowsTickOffset(false);
}

UObject* UWindow::GetTopChild(std::optional<bool> bVisibleOnly)
{
	bool visibleOnly = (!bVisibleOnly || *bVisibleOnly);
	for (UWindow* child = lastChild(); child; child = child->prevSibling())
	{
		if (!visibleOnly || child->bIsVisible())
			return child;
	}
	return nullptr;
}

void UWindow::GrabMouse()
{
	if (URootWindow* root = GetRootWindow())
		root->grabbedWindow() = this;
}

void UWindow::Hide()
{
	SetVisibility(false);
}

// The original's SetVisibility asks the window's parent: the parent's
// ChildRequestedVisibilityChange event decides, and the script's default
// calls SetChildVisibility back on the child; DeusExHUD's override also lays
// the HUD out again, so the InfoLink and the log push it around as they come
// and go. The root window, having no parent, sets its own
// (extension-dll.md, Showing and hiding).
void UWindow::SetVisibility(bool show)
{
	if (bIsVisible() == show)
		return;
	if (UWindow* parent = parentOwner())
		parent->ChildRequestedVisibilityChange(this, show);
	else
		SetChildVisibility(show);
}

bool UWindow::IsActorValid(UObject* refActor)
{
	UPlayerPawnExt* playerPawn = UObject::Cast<UPlayerPawnExt>(GetPlayerPawn());
	if (!playerPawn) return false;
	UActor* target = UObject::Cast<UActor>(refActor);
	if (!target) return false;
	return FindActorRef(playerPawn, target) >= 0;
}

bool UWindow::IsFocusWindow()
{
	if (auto root = GetRootWindow())
		return root->FocusWindow() == this;
	return false;
}

// The original's (XWindow::IsKeyDown 0x1004c980): the root's mark of the
// key, which every press it sees sets and every release clears.
bool UWindow::IsKeyDown(uint8_t Key)
{
	URootWindow* root = GetRootWindow();
	if (!root || Key > 0xFE)
		return false;
	return (&root->keyDownMap())[Key] != 0;
}

bool UWindow::IsPointInWindow(float pointX, float pointY)
{
	return pointX >= 0.0f && pointX < Width() && pointY >= 0.0f && pointY < Height();
}

// The original's execIsSensitive (Extension.dll 0x10051bd0): recursing
// unless told not to, and up through the parents -- the window and every
// ancestor sensitive -- not down through the children
bool UWindow::IsSensitive(std::optional<bool> bRecurse)
{
	if (!bRecurse.value_or(true))
		return bIsSensitive();
	for (UWindow* w = this; w; w = w->parentOwner())
	{
		if (!w->bIsSensitive())
			return false;
	}
	return true;
}

// The window and every ancestor visible: it shows if the root does. The
// original's natives walk this chain before acting (a scale's attributes, a
// shown area).
bool UWindow::IsShown()
{
	for (UWindow* w = this; w; w = w->parentOwner())
	{
		if (!w->bIsVisible())
			return false;
	}
	return true;
}

// As IsSensitive: the original's execIsVisible (0x10051a10) walks the
// parents by default, IsShown's walk
bool UWindow::IsVisible(std::optional<bool> bRecurse)
{
	if (!bRecurse.value_or(true))
		return bIsVisible();
	return IsShown();
}

void UWindow::Lower()
{
	if (!parentOwner())
		return;
	UWindow* owner = parentOwner();
	DetachFromParent();
	parentOwner() = owner;
	winParent() = owner; // the original's AddChild sets it again
	if (owner->firstChild())
	{
		nextSibling() = owner->firstChild();
		owner->firstChild()->prevSibling() = this;
		owner->firstChild() = this;
	}
	else
	{
		owner->firstChild() = this;
		owner->lastChild() = this;
	}
}

/*
UObject* UWindow::MoveFocus(EMove direction)
{
	auto root = GetRootWindow();
	if(root)
	{
		int hmref = root->handleMouseRef();
		if (!hmref)
			MoveTabGroup(dir);
		else {
			UTabGroupWindow* tabWindow = UObject::Cast<UTabGroupWindow*>(GetTabGroupWindow());
			if (tabWindow)
			{
				bool wrapFocus = bWrapFocus();
			}
		}
	}
}
*/

UObject* UWindow::MoveFocusDown()
{
	return MoveFocus(3);
}

UObject* UWindow::MoveFocusLeft()
{
	return MoveFocus(0);
}

UObject* UWindow::MoveFocusRight()
{
	return MoveFocus(1);
}

UObject* UWindow::MoveFocusUp()
{
	return MoveFocus(2);
}

// Whether focus may sit on this window (Extension.dll XWindow::IsTraversable
// 0x1004c460): the window itself must be selectable, and it and every
// ancestor visible and sensitive. With the topmost check, the nearest
// ancestor that is a modal (or the root) must also be the topmost modal: the
// root's topmost child that shows and is a modal, else the root. A window
// under a buried modal cannot take focus.
bool UWindow::IsTraversable(bool checkTopmost)
{
	if (!bIsSelectable())
		return false;
	UWindow* firstModalAncestor = nullptr;
	for (UWindow* i = this; i; i = i->parentOwner())
	{
		if (!i->bIsVisible() || !i->bIsSensitive())
			return false;
		if (checkTopmost)
		{
			if (!firstModalAncestor && i->windowType() >= 2)
				firstModalAncestor = i;
			if (i->windowType() == 3)
			{
				UWindow* top = nullptr;
				for (UWindow* c = i->lastChild(); c; c = c->prevSibling())
				{
					if (c->bIsVisible() && c->windowType() >= 2)
					{
						top = c;
						break;
					}
				}
				if ((top ? top : i) != firstModalAncestor)
					return false;
			}
		}
	}
	return true;
}

// The group's focus order, built on demand: the original keeps row- and
// column-major sorted lists of the group's selectable windows, resorted as
// children are configured (XTabGroupWindow::ResortWindowTables 0x10045140,
// sorted by position, ties broken by pointer); DXEngine walks the group's
// descendants and sorts the same way each time focus moves -- the same
// windows in the same order (XTabGroupWindow::AddWindowToTables only adds
// what SetSelectability made selectable, and IsTraversable is asked live at
// the walk anyway).
static void CollectGroupWindows(UWindow* w, UWindow* also, std::vector<UWindow*>& out)
{
	for (UWindow* c = w->firstChild(); c; c = c->nextSibling())
	{
		if (c->bIsSelectable() || c == also)
			out.push_back(c);
		if (c->windowType() == 0)
			CollectGroupWindows(c, also, out);
	}
}

// Each window's place on the screen, which the original sorts by: its
// clip rect's origin, in the root's coordinates (the comparers at
// 0x10045250 and 0x100452d0).
static std::vector<std::pair<UWindow*, vec2>> GroupWindowPlaces(UWindow* group, UWindow* also)
{
	std::vector<UWindow*> windows;
	CollectGroupWindows(group, also, windows);
	std::vector<std::pair<UWindow*, vec2>> places;
	URootWindow* root = group->GetRootWindow();
	for (UWindow* w : windows)
	{
		vec2 place = { 0.0f, 0.0f };
		if (root)
			group->ConvertCoordinates(w, 0.0f, 0.0f, root, place.x, place.y);
		places.push_back({ w, place });
	}
	return places;
}

static std::vector<UWindow*> GroupWindowsByRow(UWindow* group, UWindow* also = nullptr)
{
	auto places = GroupWindowPlaces(group, also);
	std::stable_sort(places.begin(), places.end(), [](const auto& a, const auto& b)
	{
		if (a.second.y != b.second.y) return a.second.y < b.second.y;
		if (a.second.x != b.second.x) return a.second.x < b.second.x;
		return a.first < b.first;
	});
	std::vector<UWindow*> out;
	for (const auto& place : places)
		out.push_back(place.first);
	return out;
}

static std::vector<UWindow*> GroupWindowsByColumn(UWindow* group, UWindow* also = nullptr)
{
	auto places = GroupWindowPlaces(group, also);
	std::stable_sort(places.begin(), places.end(), [](const auto& a, const auto& b)
	{
		if (a.second.x != b.second.x) return a.second.x < b.second.x;
		if (a.second.y != b.second.y) return a.second.y < b.second.y;
		return a.first < b.first;
	});
	std::vector<UWindow*> out;
	for (const auto& place : places)
		out.push_back(place.first);
	return out;
}

// Focus moves within the focus window's group, along its row- or
// column-major order (Extension.dll XWindow::MoveFocus 0x1004ef30): left
// and up decrement, right and down increment, the walk wraps, windows that
// are not traversable are passed over, and a group with nothing traversable
// hands the move to the tab groups, as does a move with no focus window at
// all -- which is how a conversation's choices light up the moment they
// appear. It returns the focus window, moved or not.
UObject* UWindow::MoveFocus(int dir)
{
	URootWindow* root = GetRootWindow();
	if (!root)
		return nullptr;

	UWindow* focus = root->FocusWindow();
	if (!focus)
		return MoveTabGroup(dir == 0 || dir == 2 ? false : true);

	UWindow* group = UObject::TryCast<UTabGroupWindow>(focus->GetTabGroupWindow());
	if (!group)
		return focus;

	// The focus stays in its group's lists until its selectability changes
	// after the focus has moved on (SetSelectability)
	std::vector<UWindow*> list = (dir < 2) ? GroupWindowsByRow(group, focus) : GroupWindowsByColumn(group, focus);
	size_t index = 0;
	for (size_t i = 0; i < list.size(); i++)
	{
		if (list[i] == focus)
		{
			index = i;
			break;
		}
	}

	bool decrement = (dir == 0 || dir == 2);
	for (size_t n = 0; n < list.size(); n++)
	{
		if (decrement)
			index = (index == 0) ? list.size() - 1 : index - 1;
		else
			index = (index + 1) % list.size();
		if (list[index]->IsTraversable(true))
		{
			root->SetRootFocusWindow(list[index]);
			return root->FocusWindow();
		}
	}

	// Nothing traversable in this group: the next group takes it.
	return MoveTabGroup(dir == 0 || dir == 2 ? false : true);
}

// The topmost visible modal child of the root, else the root: where focus
// moves among tab groups and where the focus is checked.
static UWindow* TopmostModal(URootWindow* root)
{
	for (UWindow* c = root->lastChild(); c; c = c->prevSibling())
	{
		if (c->bIsVisible() && c->windowType() >= 2)
			return c;
	}
	return root;
}

// A modal's table of tab groups (XModalWindow::AddTabGroupToTable): the
// modal itself, and every group under it but those under a modal further
// in, which keep their own.
static void CollectTabGroupTable(UWindow* w, std::vector<UWindow*>& out)
{
	for (UWindow* c = w->firstChild(); c; c = c->nextSibling())
	{
		if (c->windowType() >= 2)
			continue;
		if (c->windowType() == 1)
			out.push_back(c);
		CollectTabGroupTable(c, out);
	}
}

// Focus moves to the next or previous tab group (Extension.dll
// XWindow::MoveTabGroup 0x1004f440), in the topmost modal's table, which the
// original sorts top to bottom, then left to right, by each group's place
// (XModalWindow::ResortTabGroupTable 0x10036ed0): the corner of what shows of
// the first window of its row list that shows any of itself -- its clip
// rectangle with a width and a height above 0 -- and is traversable without
// the modal check, 0, 0 with none (XTabGroupWindow::ComputeTabGroupLocation
// 0x10045000). So a group's first window scrolled out of its clip window
// leaves the group's place to the first that shows. It starts at the group
// after or before the focus's, or at the table's first when the focus is
// not in that modal; the first window of a group's row list traversable
// without the modal check takes the focus, shown or not -- the clip window
// scrolls to it -- a group with none passed over, round the ends. The root
// window's script calls it for Tab and Shift+Tab (extension-dll.md, Small).
// It returns the focus window, moved or not.
UObject* UWindow::MoveTabGroup(bool next)
{
	URootWindow* root = GetRootWindow();
	if (!root)
		return nullptr;

	UWindow* holder = TopmostModal(root);
	std::vector<UWindow*> groups = { holder };
	CollectTabGroupTable(holder, groups);

	struct Entry { UWindow* group; float x, y; };
	std::vector<Entry> table;
	for (UWindow* group : groups)
	{
		Entry entry = { group, 0.0f, 0.0f };
		for (UWindow* w : GroupWindowsByRow(group))
		{
			const ClipRect& clip = w->winClipRect();
			if (clip.clipWidth > 0.0f && clip.clipHeight > 0.0f && w->IsTraversable(false))
			{
				entry.x = clip.originX + clip.clipX;
				entry.y = clip.originY + clip.clipY;
				break;
			}
		}
		table.push_back(entry);
	}
	std::sort(table.begin(), table.end(), [](const Entry& a, const Entry& b)
	{
		if (a.y != b.y) return a.y < b.y;
		if (a.x != b.x) return a.x < b.x;
		return a.group < b.group;
	});

	size_t count = table.size();
	size_t start = 0;
	UWindow* focus = root->FocusWindow();
	if (focus && focus->GetModalWindow() == holder)
	{
		UWindow* current = static_cast<UWindow*>(focus->GetTabGroupWindow());
		for (size_t i = 0; i < count; i++)
		{
			if (table[i].group == current)
			{
				start = next ? (i + 1) % count : (i + count - 1) % count;
				break;
			}
		}
	}

	size_t index = start;
	do
	{
		for (UWindow* w : GroupWindowsByRow(table[index].group))
		{
			if (w->IsTraversable(false))
			{
				root->SetRootFocusWindow(w);
				return root->FocusWindow();
			}
		}
		index = next ? (index + 1) % count : (index + count - 1) % count;
	} while (index != start);
	return root->FocusWindow();
}

// A focus that can no longer take it (Extension.dll XWindow::CheckFocusWindow
// 0x10050b80) goes to the topmost modal's preferred window when that can take
// it, else on as a move right takes it, else to nothing.
void UWindow::CheckFocusWindow()
{
	URootWindow* root = GetRootWindow();
	if (!root)
		return;
	UWindow* focus = root->FocusWindow();
	if (!focus || focus->IsTraversable(true))
		return;

	if (UModalWindow* modal = UObject::TryCast<UModalWindow>(TopmostModal(root)))
	{
		if (modal->preferredFocus() && modal->preferredFocus()->IsTraversable(true))
			root->SetRootFocusWindow(modal->preferredFocus());
	}
	if (root->FocusWindow() == focus && root->MoveFocus(1) == focus)
		root->SetRootFocusWindow(nullptr);
}

// The root lets go of a grab by a window that is hidden or insensitive, or
// under one (Extension.dll XWindow::CheckGrabbedWindow 0x10050c90).
void UWindow::CheckGrabbedWindow()
{
	URootWindow* root = GetRootWindow();
	if (!root)
		return;
	for (UWindow* w = root->grabbedWindow(); w; w = w->parentOwner())
	{
		if (!w->bIsVisible() || !w->bIsSensitive())
		{
			root->grabbedWindow() = nullptr;
			return;
		}
	}
}

UObject* UWindow::MoveTabGroupNext()
{
	return MoveTabGroup(true);
}

UObject* UWindow::MoveTabGroupPrev()
{
	return MoveTabGroup(false);
}

UObject* UWindow::NewChild(UObject* NewClass, std::optional<bool> bShow)
{
	bool show = !bShow || *bShow;
	//LogMessage(GetUClassFullName(this).ToString() + ": NewChild(" + NewClass->Name.ToString() + ", " + (show ? "true" : "false") + ")");
	// Named after its class and a number, as the original names it
	// (HUDKeypadWindow0).
	Package* transient = engine->packages->GetTransientPackage();
	UClass* childClass = UObject::Cast<UClass>(NewClass);
	// No class, no window: the original's CreateNewWindow (Extension.dll
	// 0x1004df70) answers None (a mod's PushWindow of an unset class)
	if (!childClass)
		return nullptr;
	auto child = UObject::Cast<UWindow>(transient->NewObject(transient->MakeUniqueObjectName(childClass), childClass, ObjectFlags::Transient));
	// The script's winParent too, which the original's AddChild sets beside
	// its own parentOwner (Extension.dll 0x10050a80): SpinningLogoWindow
	// bounces off its parent's size
	child->parentOwner() = this;
	child->winParent() = this;
	child->prevSibling() = lastChild();
	child->nextSibling() = nullptr;
	child->firstChild() = nullptr;
	child->lastChild() = nullptr;
	child->bIsVisible() = false;
	child->bIsSensitive() = true;
	// The window's type as the original's Init sets it (XTabGroupWindow::Init
	// 0x10044d90 and XModalWindow::Init 0x10036c00): a tab group, a modal (a
	// root child), or a generic child. Buttons are selectable from creation,
	// as the original's class defaults have them -- DXEngine's choice buttons
	// measured False beside the original's True.
	if (UObject::TryCast<URootWindow>(child))
		child->windowType() = 3;
	else if (UObject::TryCast<UModalWindow>(child) && windowType() == 3)
		child->windowType() = 2;
	else if (UObject::TryCast<UTabGroupWindow>(child))
	{
		// A modal anywhere but under the root stays the tab group it also is
		if (UObject::TryCast<UModalWindow>(child))
			LogMessage("Modal window must be a child of Root!");
		child->windowType() = 1;
	}
	if (UObject::TryCast<UButtonWindow>(child))
		child->bIsSelectable() = true;
	if (lastChild())
		lastChild()->nextSibling() = child;
	lastChild() = child;
	if (!firstChild())
		firstChild() = child;
	// The original's CreateNewWindow (Extension.dll 0x1004df70): the window is
	// made hidden and its parent told -- ChildAdded, then DescendantAdded to
	// the parent and each ancestor -- before its InitWindow runs; only then is
	// it shown, as Show shows it, so VisibilityChanged reaches it and the
	// windows its InitWindow made: a list given rows there selects its first.
	ChildAdded(child);
	for (UWindow* ancestor = this; ancestor; ancestor = ancestor->parentOwner())
		ancestor->DescendantAdded(child);
	child->InitWindow();
	// One that destroyed itself in its InitWindow is None to its maker, as
	// the original's is (its destroy held off while the window is locked,
	// done as the lock lifts, and None returned)
	if (child->Destroyed)
		return nullptr;
	if (show)
		child->SetVisibility(true);
	child->AskParentForReconfigure();
	return child;
}

// The original's (XWindow::PlaySound 0x10050050, extension-dll.md, window
// sounds): one unit from the player's place, not its eyes, along the view's
// yaw -- turned left or right by the point's place across the screen, a
// quarter turn at either edge, when the root has positional sound on and
// the window and every window it is in are shown --, with no actor and a
// channel of its own, a radius of 100 and the volume over 0.99. The point
// defaults to the window's centre, and a volume of -1 is the window's own,
// which only SetSoundVolume sets.
void UWindow::PlaySound(UObject* newsound, std::optional<float> Volume, std::optional<float> Pitch, std::optional<float> posX, std::optional<float> posY)
{
	USound* s = UObject::Cast<USound>(newsound);
	UPlayerPawnExt* player = UObject::Cast<UPlayerPawnExt>(GetPlayerPawn());
	URootWindow* root = GetRootWindow();
	if (s && player && root)
	{
		float volume = Volume ? *Volume : -1.0f;
		if (volume == -1.0f)
			volume = SoundVolume();

		UWindow* hidden = this;
		while (hidden && hidden->bIsVisible())
			hidden = hidden->parentOwner();

		float across = 0.0f;
		if (!hidden && root->bPositionalSound())
		{
			float rootX = 0.0f, rootY = 0.0f;
			ConvertCoordinates(this, posX ? *posX : Width() * 0.5f, posY ? *posY : Height() * 0.5f, root, rootX, rootY);
			across = std::clamp(rootX * 2.0f / std::max(root->Width(), 1.0f) - 1.0f, -1.0f, 1.0f);
		}

		Rotator rotation = player->ViewRotation();
		rotation.Yaw += (int)(across * 16384.0f);
		rotation.Pitch = 0;
		vec3 at, left, up;
		Coords::Rotation(rotation).GetAxes(at, left, up);
		const float radius = 100.0f, distance = 1.0f;
		engine->audiodev->PlaySound(nullptr, 2 * SLOT_None, s, player->Location() + at * distance, volume * radius / (radius - distance), radius, Pitch ? *Pitch : 1.0f, false);
	}
}

// The original's (XWindow::QueryGranularity 0x1004e9e0): a pixel unless the
// window says more, and never less.
void UWindow::QueryGranularity(float& hGranularity, float& vGranularity)
{
	hGranularity = 1.0f;
	vGranularity = 1.0f;
	ParentRequestedGranularity(hGranularity, vGranularity);
	hGranularity = std::max(hGranularity, 1.0f);
	vGranularity = std::max(vGranularity, 1.0f);
}

void UWindow::Raise()
{
	if (!parentOwner())
		return;
	UWindow* owner = parentOwner();
	DetachFromParent();
	parentOwner() = owner;
	winParent() = owner; // the original's AddChild sets it again
	if (owner->lastChild())
	{
		prevSibling() = owner->lastChild();
		owner->lastChild()->nextSibling() = this;
		owner->lastChild() = this;
	}
	else
	{
		owner->firstChild() = this;
		owner->lastChild() = this;
	}
}

void UWindow::ReleaseGC(UObject* GC)
{
	if (GC && engine->render)
		engine->render->PopWindowGC();
}

void UWindow::RemoveActorRef(UObject* refActor)
{
	UPlayerPawnExt* playerPawn = UObject::Cast<UPlayerPawnExt>(GetPlayerPawn());
	if (!playerPawn) return;
	UActor* target = UObject::Cast<UActor>(refActor);
	if (!target) return;

	int foundIndex = FindActorRef(playerPawn, target);
	int& count = playerPawn->actorCount();
	auto refs = playerPawn->actorList();

	if (foundIndex >= 0)
	{
		--refs[foundIndex].RefCount;
		if (refs[foundIndex].RefCount < 1)
		{
			for (int i = foundIndex + 1; i < count; i++)
				refs[i - 1] = refs[i];
			refs[count - 1].Actor = nullptr;
			refs[count - 1].RefCount = 0;
			count--;
		}
	}
}

// The original's (XWindow::SetAcceleratorText 0x1004f310, SetAcceleratorKey
// 0x1004f3b0): the character after the text's first |&, none past 254.
// The modal it lies in looks its accelerators up afresh each time.
void UWindow::SetAcceleratorText(const std::string& newStr)
{
	unsigned char accelerator = 0;
	size_t pos = newStr.find("|&");
	if (pos != std::string::npos && pos + 2 < newStr.size())
		accelerator = (unsigned char)newStr[pos + 2];
	int newKey = accelerator < 0xFF ? accelerator : 0;
	if (acceleratorKey() != newKey)
	{
		acceleratorKey() = newKey;
		if (UModalWindow* modal = UObject::TryCast<UModalWindow>(GetModalWindow()))
			modal->bDirtyAccelerators() = true;
	}
}

void UWindow::SetBackground(UObject* newBackground)
{
	Background() = UObject::Cast<UTexture>(newBackground);
	bDrawRawBackground() = true;
}

void UWindow::SetBackgroundSmoothing(bool newSmoothing)
{
	bSmoothBackground() = newSmoothing;
}

void UWindow::SetBackgroundStretching(bool newStretching)
{
	bStretchBackground() = newStretching;
}

void UWindow::SetBackgroundStyle(uint8_t NewStyle)
{
	backgroundStyle() = NewStyle;
}

void UWindow::SetBaselineData(std::optional<float> newBaselineOffset, std::optional<float> newUnderlineHeight)
{
	if (newBaselineOffset)
		BaselineOffset = *newBaselineOffset;
	if (newUnderlineHeight)
		UnderlineHeight = *newUnderlineHeight;
}

void UWindow::SetBoldFont(UObject* fn)
{
	boldFont() = UObject::Cast<UFont>(fn);
}

// VisibilityChanged goes to the window and down through the descendants whose
// own flags let the change reach them: a subtree hidden by its own flag did
// not change what can be seen.
static void NotifyVisibilityChanged(UWindow* window, bool bNewVisibility)
{
	window->VisibilityChanged(bNewVisibility);
	// Then the window's own sound for showing or hiding (the original's
	// InvokeVisibilityChange, 0x10050e30).
	if (USound* sound = bNewVisibility ? window->visibleSound() : window->invisibleSound())
		window->PlaySound(sound, {}, {}, {}, {});
	for (UWindow* child : ChildrenOf(window))
	{
		if (child->parentOwner() == window && !child->Destroyed && child->bIsVisible())
			NotifyVisibilityChanged(child, bNewVisibility);
	}
}

// The original's (XWindow::SetChildVisibility 0x1004ee20; extension-dll.md,
// Showing and hiding): the flag is set, and when that changes whether the
// window can be seen, focus and grabs move away from what is hidden, the
// window is marked to be laid out, VisibilityChanged goes to the window and
// its descendants, and the tree is laid out again (ReconfigureTree): a window
// shown, and each in it that asked while it could not be seen, at once.
void UWindow::SetChildVisibility(bool bNewVisibility)
{
	if (bIsVisible() == bNewVisibility)
		return;

	// The flag change only shows when every ancestor is visible.
	bool ancestorsVisible = true;
	for (UWindow* w = parentOwner(); w; w = w->parentOwner())
	{
		if (!w->bIsVisible())
		{
			ancestorsVisible = false;
			break;
		}
	}

	bIsVisible() = bNewVisibility;

	if (!ancestorsVisible)
		return;

	CheckFocusWindow();
	CheckGrabbedWindow();

	bNeedsReconfigure() = true;
	NotifyVisibilityChanged(this, bNewVisibility);
	ReconfigureTree();
}

void UWindow::SetClientObject(UObject* newClientObject)
{
	clientObject() = newClientObject;
}

void UWindow::SetCursorPos(float newMouseX, float newMouseY)
{
	if (URootWindow* root = GetRootWindow())
	{
		float x = 0.0f, y = 0.0f;
		ConvertCoordinates(this, newMouseX, newMouseY, root, x, y);
		root->SetRootCursorPos(x, y);
	}
}

// As the original's native (Extension.dll execSetDefaultCursor): every
// argument set, an omitted one its default -- no shadow, hot spots of -1
// (the texture's middle less one, QueryCursor's rule) and white. An omitted
// one once left the window's old value: the root's SetDefaultCursor names
// no colour, and the class's black with no alpha drew nothing.
void UWindow::SetDefaultCursor(UObject* tX, std::optional<UObject*> shadowTexture, std::optional<float> HotX, std::optional<float> HotY, std::optional<Color> cursorColor)
{
	defaultCursor() = UObject::Cast<UTexture>(tX);
	defaultCursorShadow() = shadowTexture ? UObject::Cast<UTexture>(*shadowTexture) : nullptr;
	defaultHotX() = HotX ? *HotX : -1.0f;
	defaultHotY() = HotY ? *HotY : -1.0f;
	defaultCursorColor() = cursorColor ? *cursorColor : Color{ 255, 255, 255, 255 };
}

void UWindow::SetFocusSounds(std::optional<UObject*> newFocusSound, std::optional<UObject*> newUnfocusSound)
{
	if (newFocusSound)
		focusSound() = UObject::Cast<USound>(*newFocusSound);
	if (newUnfocusSound)
		unfocusSound() = UObject::Cast<USound>(*newUnfocusSound);
}

bool UWindow::SetFocusWindow(UObject* NewFocusWindow)
{
	if (auto root = GetRootWindow())
		return root->SetRootFocusWindow(UObject::Cast<UWindow>(NewFocusWindow));
	return false;
}

void UWindow::SetFont(UObject* fn)
{
	normalFont() = UObject::Cast<UFont>(fn);
	boldFont() = UObject::Cast<UFont>(fn);
}

void UWindow::SetFonts(UObject* nFont, UObject* bFont)
{
	normalFont() = UObject::Cast<UFont>(nFont);
	boldFont() = UObject::Cast<UFont>(bFont);
}

void UWindow::SetNormalFont(UObject* fn)
{
	normalFont() = UObject::Cast<UFont>(fn);
}

// A focus that can no longer take it moves on (Extension.dll
// XWindow::SetSelectability 0x1004c390).
void UWindow::SetSelectability(bool newSelectability)
{
	if (bIsSelectable() == newSelectability)
		return;
	bIsSelectable() = newSelectability;
	CheckFocusWindow();
}

// The original's SensitivityChanged goes down the tree
// (XWindow::InvokeSensitivityChange, Extension.dll 0x10050ef0): to the window,
// then to each child whose own flag is on, with the sensitivity the window
// now has in effect
static void InvokeSensitivityChange(UWindow* window, bool sensitive)
{
	window->SensitivityChanged(sensitive);
	for (UWindow* child : ChildrenOf(window))
	{
		if (child->parentOwner() == window && !child->Destroyed && child->bIsSensitive())
			InvokeSensitivityChange(child, sensitive);
	}
}

// The original's (XWindow::SetSensitivity 0x1004c890): the flag set, and only
// when that changes what the window has in effect -- its own flag and every
// parent's -- the focus and a grab are checked, as hiding checks them, and
// SensitivityChanged goes to the window and its sensitive descendants. So a
// button disabled while it has the focus loses it, and one in a disabled
// parent hears of it. (The original marks its accelerator table too, which
// DXEngine has not.)
void UWindow::SetSensitivity(bool newSensitivity)
{
	if (bIsSensitive() == newSensitivity)
		return;
	bool before = IsSensitive(true);
	bIsSensitive() = newSensitivity;
	bool after = IsSensitive(true);
	if (before == after)
		return;
	CheckFocusWindow();
	CheckGrabbedWindow();
	InvokeSensitivityChange(this, after);
}

void UWindow::SetSoundVolume(float newVolume)
{
	SoundVolume() = newVolume;
}

void UWindow::SetTextColor(const Color& NewColor)
{
	TextColor() = NewColor;
}

void UWindow::SetTileColor(const Color& NewColor)
{
	tileColor() = NewColor;
}

void UWindow::SetVisibilitySounds(std::optional<UObject*> visSound, std::optional<UObject*> invisSound)
{
	if (visSound)
		visibleSound() = UObject::Cast<USound>(*visSound);
	if (invisSound)
		invisibleSound() = UObject::Cast<USound>(*invisSound);
}

void UWindow::Show(std::optional<bool> bShow)
{
	SetVisibility(!bShow || *bShow);
}

void UWindow::UngrabMouse()
{
	if (URootWindow* root = GetRootWindow())
		root->grabbedWindow() = nullptr;
}

// The original's ResetSize, ResetWidth and ResetHeight (Extension.dll
// 0x1004d410, 0x1004d4c0, 0x1004d550): a set size taken away, and the
// window laid out again -- its parent asked -- only when one was set.
void UWindow::ResetSize()
{
	if (FixedWidth || FixedHeight)
	{
		FixedWidth = false;
		FixedHeight = false;
		AskParentForReconfigure();
	}
}

void UWindow::ResetWidth()
{
	if (FixedWidth)
	{
		FixedWidth = false;
		AskParentForReconfigure();
	}
}

void UWindow::ResetHeight()
{
	if (FixedHeight)
	{
		FixedHeight = false;
		AskParentForReconfigure();
	}
}

// The original's Resize, SetWidth and SetHeight (Extension.dll 0x1004cf70,
// 0x1004d040, 0x1004d0e0): a size held at 0 or more, and the window laid
// out again -- its parent asked, which marks the window too -- only when
// the size changes.
void UWindow::SetSize(float newWidth, float NewHeight)
{
	newWidth = std::max(newWidth, 0.0f);
	NewHeight = std::max(NewHeight, 0.0f);
	if (!FixedWidth || !FixedHeight || hardcodedWidth() != newWidth || hardcodedHeight() != NewHeight)
	{
		hardcodedWidth() = newWidth;
		hardcodedHeight() = NewHeight;
		FixedWidth = true;
		FixedHeight = true;
		AskParentForReconfigure();
	}
}

void UWindow::SetWidth(float newWidth)
{
	newWidth = std::max(newWidth, 0.0f);
	if (!FixedWidth || hardcodedWidth() != newWidth)
	{
		hardcodedWidth() = newWidth;
		FixedWidth = true;
		AskParentForReconfigure();
	}
}

void UWindow::SetHeight(float NewHeight)
{
	NewHeight = std::max(NewHeight, 0.0f);
	if (!FixedHeight || hardcodedHeight() != NewHeight)
	{
		hardcodedHeight() = NewHeight;
		FixedHeight = true;
		AskParentForReconfigure();
	}
}

void UWindow::SetConfiguration(float newX, float newY, float newWidth, float NewHeight)
{
	SetPos(newX, newY);
	SetSize(newWidth, NewHeight);
}

// The original's (XWindow::Move 0x1004ce80): left- and top-aligned, x and y
// its margins, laid out again when that changes anything -- at once, where
// it shows: a parent that places its children places it again, else its
// margins place it, and it reads and draws there now.
void UWindow::SetPos(float newX, float newY)
{
	if ((EHAlign)winHAlign() != EHAlign::Left || (EVAlign)winVAlign() != EVAlign::Top || hMargin0() != newX || vMargin0() != newY)
	{
		winHAlign() = (uint8_t)EHAlign::Left;
		winVAlign() = (uint8_t)EVAlign::Top;
		hMargin0() = newX;
		vMargin0() = newY;
		AskParentForReconfigure();
	}
}

// The original's (XWindow::QueryPreferredSize 0x1004e790): a size the
// window was given (SetSize, SetWidth) counts as asked for; with either side
// not asked for, the window says (ParentRequestedPreferredSize, -1 standing
// for a side not asked for), and a side it leaves below 0 is its background's
// size, else its own size now -- a plain window's 10 by 10 from its start.
void UWindow::QueryPreferredSize(bool bWidthSpecified, float width, float* outWidth, bool bHeightSpecified, float height, float* outHeight)
{
	if (!bWidthSpecified)
	{
		if (FixedWidth)
		{
			bWidthSpecified = true;
			width = hardcodedWidth();
		}
		else
		{
			width = -1.0f;
		}
	}
	if (!bHeightSpecified)
	{
		if (FixedHeight)
		{
			bHeightSpecified = true;
			height = hardcodedHeight();
		}
		else
		{
			height = -1.0f;
		}
	}
	if (!bWidthSpecified || !bHeightSpecified)
	{
		float askedWidth = width, askedHeight = height;
		ParentRequestedPreferredSize(bWidthSpecified, askedWidth, bHeightSpecified, askedHeight);
		if (!bWidthSpecified)
			width = askedWidth;
		if (!bHeightSpecified)
			height = askedHeight;
		lastQueryWidth() = width;
		lastQueryHeight() = height;
		if (width < 0.0f)
			width = Background() ? (float)Background()->USize() : Width();
		if (height < 0.0f)
			height = Background() ? (float)Background()->VSize() : Height();
	}
	if (outWidth)
		*outWidth = width;
	if (outHeight)
		*outHeight = height;
}

void UWindow::QueryPreferredSize(float& preferredWidth, float& preferredHeight)
{
	QueryPreferredSize(false, 0.0f, &preferredWidth, false, 0.0f, &preferredHeight);
}

// The original's (XWindow::QueryPreferredWidth 0x1004e5f0, QueryPreferredHeight
// 0x1004e680): the other side asked for.
float UWindow::QueryPreferredWidth(float queryHeight)
{
	float width = 0.0f;
	QueryPreferredSize(false, 0.0f, &width, true, queryHeight, nullptr);
	return width;
}

float UWindow::QueryPreferredHeight(float queryWidth)
{
	float height = 0.0f;
	QueryPreferredSize(true, queryWidth, nullptr, false, 0.0f, &height);
	return height;
}

// The original's (XWindow::AskParentForReconfigure 0x1004e060): the window
// marked to be laid out, and laid out at once when it and every window it is
// in shows -- one made inside a window's InitWindow, before that window
// shows, asks as it shows (ReconfigureTree). The parent is asked with its
// own child, and one that does not take it -- every native container but the
// root: a tab group, a modal, a clip window, a tile -- asks its own parent in
// its own name, up to the root, which lays itself out (its
// ConfigurationChanged, nothing placed again); so a handler that looks at
// which child asks sees a child of its own, never a grandchild. Then a window
// still marked, one no parent's layout placed, places itself by its alignment
// (ResizeChild).
void UWindow::AskParentForReconfigure()
{
	//LogMessage(GetUClassFullName(this).ToString() + ": AskParentForReconfigure");
	bNeedsReconfigure() = true;
	if (!IsShown())
		return;
	UWindow* parent = parentOwner();
	if (!parent)
	{
		bNeedsReconfigure() = false;
		ConfigurationChanged();
		return;
	}
	if (!parent->ChildRequestedReconfiguration(this))
		parent->AskParentForReconfigure();
	if (bNeedsReconfigure())
		ResizeChild();
}

// The original's (XWindow::ResizeChild 0x1004eab0), the base Window script's
// answer to a child's request: the window placed at once by its alignment at
// its preferred size -- a full side the parent's less both margins, asked for
// as given -- centred (the half truncated) plus its first margin, right or
// bottom less it, else at it. Its x and y are that place. The root's children
// are placed across the whole screen (RootOffsetX), a full one that wide.
void UWindow::ResizeChild()
{
	//LogMessage(GetUClassFullName(this).ToString() + ": ResizeChild");
	UWindow* parent = parentOwner();
	float pWidth = parent ? parent->Width() : 0.0f;
	float pHeight = parent ? parent->Height() : 0.0f;
	if (parent && parent == engine->dxRootWindow)
		pWidth += std::max(GetExtendedVirtualWidth() - pWidth, 0.0f);

	EHAlign halign = (EHAlign)winHAlign();
	EVAlign valign = (EVAlign)winVAlign();
	bool fullWidth = halign == EHAlign::Full;
	bool fullHeight = valign == EVAlign::Full;
	float newWidth = fullWidth ? std::max(pWidth - hMargin0() - hMargin1(), 0.0f) : 0.0f;
	float newHeight = fullHeight ? std::max(pHeight - vMargin0() - vMargin1(), 0.0f) : 0.0f;
	QueryPreferredSize(fullWidth, newWidth, &newWidth, fullHeight, newHeight, &newHeight);

	float newX, newY;
	if (halign == EHAlign::Center)
		newX = std::trunc((pWidth - newWidth) * 0.5f) + hMargin0();
	else if (halign == EHAlign::Right)
		newX = pWidth - newWidth - hMargin0();
	else
		newX = hMargin0();
	if (valign == EVAlign::Center)
		newY = std::trunc((pHeight - newHeight) * 0.5f) + vMargin0();
	else if (valign == EVAlign::Bottom)
		newY = pHeight - newHeight - vMargin0();
	else
		newY = vMargin0();
	ConfigureChild(newX, newY, newWidth, newHeight);
}

// How deep the placings in progress go: the outermost clips (ConfigureChild).
static int ConfigureDepth = 0;

// The original's (XWindow::ConfigureChild 0x1004ec50) sets just what it is
// given -- a size set on the window only what it asks for, a full alignment
// only where no parent places it: a tile that fills its width widens a child
// that set its own -- and counts the window as placed. A window that does
// not show is only that, its place and size left as they are (TitleConsole:
// the HUD's mission start text stays 10 by 10 at 0, 0 until a message shows
// it). One whose size changes, or that asked to be laid out, is laid out at
// once (ChangeConfiguration); then the outermost placing, or a tab group's,
// clips the window and all in it (ClipTree).
void UWindow::ConfigureChild(float newX, float newY, float newWidth, float newHeight)
{
	//LogMessage(GetUClassFullName(this).ToString() + ": ConfigureChild(" + std::to_string(newX) + ", " + std::to_string(newY) + ", " + std::to_string(newWidth) + ", " + std::to_string(newHeight) + ")");

	bConfigured() = true;
	if (!bIsVisible())
		return;

	bool changed = Width() != newWidth || Height() != newHeight || bNeedsReconfigure();
	bNeedsReconfigure() = false;
	X() = newX;
	Y() = newY;
	Width() = newWidth;
	Height() = newHeight;
	// The root's own place is the screen's middle (URootWindow::ResizeRoot)
	if (parentOwner())
	{
		UsedX = X() + RootOffsetX();
		UsedY = Y();
	}

	ConfigureDepth++;
	if (changed)
		ChangeConfiguration();
	if (ConfigureDepth == 1 || windowType() != 0)
		ClipTree();
	ConfigureDepth--;
}

static bool IsLeftTopAligned(UWindow* window)
{
	return (EHAlign)window->winHAlign() == EHAlign::Left && (EVAlign)window->winVAlign() == EVAlign::Top;
}

// A window laid out (XWindow::ChangeConfiguration 0x10050f90): its children
// count as placed but each that shows and is not left- and top-aligned; then
// its ConfigurationChanged places what it places; then each of those it left
// out places itself by its alignment (ResizeChild). So a window a parent
// placed goes back to its alignment when the parent's next layout leaves it
// out, and a left- and top-aligned one stays where it was put.
void UWindow::ChangeConfiguration()
{
	for (UWindow* child = firstChild(); child; child = child->nextSibling())
		child->bConfigured() = !child->bIsVisible() || IsLeftTopAligned(child);
	ConfigurationChanged();
	for (UWindow* child : ChildrenOf(this))
	{
		if (child->parentOwner() == this && !child->Destroyed && child->bIsVisible() && !IsLeftTopAligned(child) && !child->bConfigured())
			child->ResizeChild();
	}
}

// The original's (XWindow::ReconfigureTree 0x1004e520), as a window comes to
// show: it, and each window in it that shows and asked to be laid out while
// it could not be seen, asks now, from the top down.
void UWindow::ReconfigureTree()
{
	if (!IsShown())
		return;
	if (bNeedsReconfigure())
		AskParentForReconfigure();
	for (UWindow* child : ChildrenOf(this))
	{
		if (child->parentOwner() == this && !child->Destroyed)
			child->ReconfigureTree();
	}
}

// The original's (XWindow::SetWindowAlignments 0x1004d300, from a script
// 0x10052360): a first margin not given is 0, a second the first; laid out
// again, at once, when that changes anything, the alignment placing the
// window unless its parent places it again.
void UWindow::SetWindowAlignments(uint8_t HAlign, uint8_t VAlign, std::optional<float> newHMargin0, std::optional<float> newVMargin0, std::optional<float> newHMargin1, std::optional<float> newVMargin1)
{
	float h0 = newHMargin0.value_or(0.0f);
	float v0 = newVMargin0.value_or(0.0f);
	float h1 = newHMargin1.value_or(h0);
	float v1 = newVMargin1.value_or(v0);
	if (winHAlign() != HAlign || winVAlign() != VAlign || hMargin0() != h0 || hMargin1() != h1 || vMargin0() != v0 || vMargin1() != v1)
	{
		winHAlign() = HAlign;
		winVAlign() = VAlign;
		hMargin0() = h0;
		vMargin0() = v0;
		hMargin1() = h1;
		vMargin1() = v1;
		AskParentForReconfigure();
	}
}

// A new window: its class's own defaults, each class's after its parent
// class's (as the original's Init chain sets them), then the script's
// InitWindow.
void UWindow::InitWindow()
{
	InitDefaults();
	CallEvent(this, "InitWindow");
}

// The original's XWindow::Init (Extension.dll 0x1004bb30): 10 by 10 at the
// parent's corner -- the size a window asked for one it does not set falls
// back to (an empty text window is a line high by it) -- green text, white
// tiles, the normal style, a line spacing of 1, special text on, the GC's
// underline 1 high and 2 up from a character's foot, and the fonts of the
// nearest ancestor that has any.
void UWindow::InitDefaults()
{
	X() = 0.0f;
	Y() = 0.0f;
	Width() = 10.0f;
	Height() = 10.0f;
	TextColor() = { 0, 255, 0, 255 };
	tileColor() = { 255, 255, 255, 255 };
	backgroundStyle() = (uint8_t)EDrawStyle::Normal;
	textVSpacing() = 1.0f;
	bSpecialText() = true;
	BaselineOffset = 2.0f;
	UnderlineHeight = 1.0f;
	for (UWindow* ancestor = parentOwner(); ancestor; ancestor = ancestor->parentOwner())
	{
		if (ancestor->normalFont() || ancestor->boldFont())
		{
			normalFont() = ancestor->normalFont();
			boldFont() = ancestor->boldFont();
			break;
		}
	}
	// Its clip rectangle all of it at the parent's corner, not yet cut by
	// the parents, until it is placed (XWindow::Init's)
	ClipRect& clip = winClipRect();
	clip = {};
	if (UWindow* parent = parentOwner())
	{
		clip.originX = parent->winClipRect().originX;
		clip.originY = parent->winClipRect().originY;
	}
	clip.clipWidth = Width();
	clip.clipHeight = Height();
}

void UWindow::DestroyWindow()
{
	CallEvent(this, "DestroyWindow");
}

void UWindow::WindowReady()
{
	CallEvent(this, "WindowReady");
}

void UWindow::ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight)
{
	CallEvent(this, "ParentRequestedPreferredSize", {
		ExpressionValue::BoolValue(bWidthSpecified),
		ExpressionValue::Variable(&preferredWidth, engine->floatprop),
		ExpressionValue::BoolValue(bHeightSpecified),
		ExpressionValue::Variable(&preferredHeight, engine->floatprop)
		});
}

void UWindow::ParentRequestedGranularity(float& hGranularity, float& vGranularity)
{
	CallEvent(this, "ParentRequestedGranularity", {
		ExpressionValue::Variable(&hGranularity, engine->floatprop),
		ExpressionValue::Variable(&vGranularity, engine->floatprop)
		});
}

void UWindow::ChildRequestedVisibilityChange(UWindow* childWin, bool bNewVisibility)
{
	CallEvent(this, "ChildRequestedVisibilityChange", {
		ExpressionValue::ObjectValue(childWin),
		ExpressionValue::BoolValue(bNewVisibility)
		});
}

bool UWindow::ChildRequestedReconfiguration(UWindow* childWin)
{
	return CallEvent(this, "ChildRequestedReconfiguration", { ExpressionValue::ObjectValue(childWin) }).ToBool();
}

void UWindow::ChildRequestedShowArea(UWindow* child, float showX, float showY, float showWidth, float showHeight)
{
	CallEvent(this, "ChildRequestedShowArea", {
		ExpressionValue::ObjectValue(child),
		ExpressionValue::FloatValue(showX),
		ExpressionValue::FloatValue(showY),
		ExpressionValue::FloatValue(showWidth),
		ExpressionValue::FloatValue(showHeight)
		});
}

void UWindow::ConfigurationChanged()
{
	CallEvent(this, "ConfigurationChanged");
}

void UWindow::VisibilityChanged(bool bNewVisibility)
{
	CallEvent(this, "VisibilityChanged", { ExpressionValue::BoolValue(bNewVisibility) });
}

void UWindow::SensitivityChanged(bool bNewSensitivity)
{
	CallEvent(this, "SensitivityChanged", { ExpressionValue::BoolValue(bNewSensitivity) });
}

void UWindow::MouseMoved(float newX, float newY)
{
	CallEvent(this, "MouseMoved", {
		ExpressionValue::FloatValue(newX),
		ExpressionValue::FloatValue(newY)
		});
}

bool UWindow::RawMouseButtonPressed(float pointX, float pointY, EInputKey button, EInputType iState)
{
	return CallEvent(this, "RawMouseButtonPressed", {
		ExpressionValue::FloatValue(pointX),
		ExpressionValue::FloatValue(pointY),
		ExpressionValue::ByteValue(button),
		ExpressionValue::ByteValue(iState)
		}).ToBool();
}

bool UWindow::RawKeyPressed(EInputKey key, EInputType iState, bool bRepeat)
{
	return CallEvent(this, "RawKeyPressed", {
		ExpressionValue::ByteValue(key),
		ExpressionValue::ByteValue(iState),
		ExpressionValue::BoolValue(bRepeat)
		}).ToBool();
}

bool UWindow::MouseButtonPressed(float pointX, float pointY, EInputKey button, int numClicks)
{
	return CallEvent(this, "MouseButtonPressed", {
		ExpressionValue::FloatValue(pointX),
		ExpressionValue::FloatValue(pointY),
		ExpressionValue::ByteValue(button),
		ExpressionValue::IntValue(numClicks)
		}).ToBool();
}

bool UWindow::MouseButtonReleased(float pointX, float pointY, EInputKey button, int numClicks)
{
	return CallEvent(this, "MouseButtonReleased", {
		ExpressionValue::FloatValue(pointX),
		ExpressionValue::FloatValue(pointY),
		ExpressionValue::ByteValue(button),
		ExpressionValue::IntValue(numClicks)
		}).ToBool();
}

bool UWindow::KeyPressed(const ScriptString& key)
{
	return CallEvent(this, "KeyPressed", { ExpressionValue::StringValue(key) }).ToBool();
}

bool UWindow::AcceleratorKeyPressed(const ScriptString& key)
{
	return CallEvent(this, "AcceleratorKeyPressed", { ExpressionValue::StringValue(key) }).ToBool();
}

bool UWindow::VirtualKeyPressed(EInputKey key, bool bRepeat)
{
	return CallEvent(this, "VirtualKeyPressed", {
		ExpressionValue::ByteValue(key),
		ExpressionValue::BoolValue(bRepeat)
		}).ToBool();
}

void UWindow::MouseEnteredWindow()
{
	CallEvent(this, "MouseEnteredWindow");
}

void UWindow::MouseLeftWindow()
{
	CallEvent(this, "MouseLeftWindow");
}

void UWindow::FocusEnteredWindow()
{
	CallEvent(this, "FocusEnteredWindow");
}

void UWindow::FocusLeftWindow()
{
	CallEvent(this, "FocusLeftWindow");
}

void UWindow::FocusEnteredDescendant(UWindow* enterWindow)
{
	CallEvent(this, "FocusEnteredDescendant", { ExpressionValue::ObjectValue(enterWindow) });
}

void UWindow::FocusLeftDescendant(UWindow* leaveWindow)
{
	CallEvent(this, "FocusLeftDescendant", { ExpressionValue::ObjectValue(leaveWindow) });
}

bool UWindow::ButtonActivated(UWindow* button)
{
	return CallEvent(this, "ButtonActivated", { ExpressionValue::ObjectValue(button) }).ToBool();
}

// A right click's activation (XWindow::ButtonActivatedRight 0x10003d50): the
// script function of that name, which Window declares no event for -- the
// game's menu choices define it, to step back a value.
bool UWindow::ButtonActivatedRight(UWindow* button)
{
	return CallEvent(this, "ButtonActivatedRight", { ExpressionValue::ObjectValue(button) }).ToBool();
}

bool UWindow::ToggleChanged(UWindow* button, bool bNewToggle)
{
	return CallEvent(this, "ToggleChanged", {
		ExpressionValue::ObjectValue(button),
		ExpressionValue::BoolValue(bNewToggle)
		}).ToBool();
}

bool UWindow::BoxOptionSelected(UWindow* box, int buttonNumber)
{
	return CallEvent(this, "BoxOptionSelected", {
		ExpressionValue::ObjectValue(box),
		ExpressionValue::IntValue(buttonNumber)
		}).ToBool();
}

bool UWindow::ScalePositionChanged(UWindow* scale, int newTickPosition, float newValue, bool bFinal)
{
	return CallEvent(this, "ScalePositionChanged", {
		ExpressionValue::ObjectValue(scale),
		ExpressionValue::IntValue(newTickPosition),
		ExpressionValue::FloatValue(newValue),
		ExpressionValue::BoolValue(bFinal)
		}).ToBool();
}

bool UWindow::ScaleRangeChanged(UWindow* scale, int fromTick, int toTick, float fromValue, float toValue, bool bFinal)
{
	return CallEvent(this, "ScaleRangeChanged", {
		ExpressionValue::ObjectValue(scale),
		ExpressionValue::IntValue(fromTick),
		ExpressionValue::IntValue(toTick),
		ExpressionValue::FloatValue(fromValue),
		ExpressionValue::FloatValue(toValue),
		ExpressionValue::BoolValue(bFinal)
		}).ToBool();
}

bool UWindow::ScaleAttributesChanged(UWindow* scale, int tickPosition, int tickSpan, int numTicks)
{
	return CallEvent(this, "ScaleAttributesChanged", {
		ExpressionValue::ObjectValue(scale),
		ExpressionValue::IntValue(tickPosition),
		ExpressionValue::IntValue(tickSpan),
		ExpressionValue::IntValue(numTicks)
		}).ToBool();
}

bool UWindow::ClipAttributesChanged(UWindow* clip, int newClipWidth, int newClipHeight, int newChildWidth, int newChildHeight)
{
	return CallEvent(this, "ClipAttributesChanged", {
		ExpressionValue::ObjectValue(clip),
		ExpressionValue::IntValue(newClipWidth),
		ExpressionValue::IntValue(newClipHeight),
		ExpressionValue::IntValue(newChildWidth),
		ExpressionValue::IntValue(newChildHeight)
		}).ToBool();
}

bool UWindow::ListRowActivated(UWindow* list, int rowId)
{
	return CallEvent(this, "ListRowActivated", {
		ExpressionValue::ObjectValue(list),
		ExpressionValue::IntValue(rowId)
		}).ToBool();
}

bool UWindow::ListSelectionChanged(UWindow* list, int numSelections, int focusRowId)
{
	return CallEvent(this, "ListSelectionChanged", {
		ExpressionValue::ObjectValue(list),
		ExpressionValue::IntValue(numSelections),
		ExpressionValue::IntValue(focusRowId)
		}).ToBool();
}

bool UWindow::ClipPositionChanged(UWindow* clip, int newCol, int newRow)
{
	return CallEvent(this, "ClipPositionChanged", {
		ExpressionValue::ObjectValue(clip),
		ExpressionValue::IntValue(newCol),
		ExpressionValue::IntValue(newRow)
		}).ToBool();
}

bool UWindow::TextChanged(UWindow* edit, bool bModified)
{
	return CallEvent(this, "TextChanged", {
		ExpressionValue::ObjectValue(edit),
		ExpressionValue::BoolValue(bModified)
		}).ToBool();
}

bool UWindow::EditActivated(UWindow* edit, bool bModified)
{
	return CallEvent(this, "EditActivated", {
		ExpressionValue::ObjectValue(edit),
		ExpressionValue::BoolValue(bModified)
		}).ToBool();
}

void UWindow::DrawBackground(UGC* gc)
{
	if (bDrawRawBackground())
	{
		gc->SetStyle((EDrawStyle)backgroundStyle());
		gc->EnableSmoothing(bSmoothBackground());
		gc->SetTileColor(tileColor());
		if (bStretchBackground())
		{
			if (UTexture* tex = UObject::Cast<UTexture>(Background()))
			{
				float swidth = (float)tex->USize();
				float sheight = (float)tex->VSize();
				Rectf dest = Rectf::xywh(gc->offsetX, gc->offsetY, Width(), Height());
				Rectf src = Rectf::xywh(0.0f, 0.0f, swidth, sheight);
				gc->DrawTile(tex, gc->ScaleRect(dest), src, tileColor(), gc->EffectivePolyFlags());
			}
		}
		else if (Background())
		{
			gc->DrawPattern(0.0f, 0.0f, Width(), Height(), 0.0f, 0.0f, Background());
		}
		// DrawDebugBox(gc);
	}
}

void UWindow::DrawWindow(UGC* gc)
{
	CallEvent(this, "DrawWindow", { ExpressionValue::ObjectValue(gc) });
}

void UWindow::PostDrawWindow(UGC* gc)
{
	CallEvent(this, "PostDrawWindow", { ExpressionValue::ObjectValue(gc) });
}

void UWindow::ChildAdded(UWindow* child)
{
	CallEvent(this, "ChildAdded", { ExpressionValue::ObjectValue(child) });
}

void UWindow::ChildRemoved(UWindow* child)
{
	CallEvent(this, "ChildRemoved", { ExpressionValue::ObjectValue(child) });
}

void UWindow::DescendantAdded(UWindow* descendant)
{
	CallEvent(this, "DescendantAdded", { ExpressionValue::ObjectValue(descendant) });
}

void UWindow::DescendantRemoved(UWindow* descendant)
{
	CallEvent(this, "DescendantRemoved", { ExpressionValue::ObjectValue(descendant) });
}

// Each window's clip rectangle (winClipRect), which the original keeps at all
// times (Extension.dll XWindow::ClipTree 0x10051160, from ConfigureChild
// 0x1004ec50 each time a window is placed): its origin, the window's corner
// in the root's coordinates, and the part of it its parents leave in view,
// from that origin -- its width or height 0 or less where none shows, as
// the original keeps them, uncut at 0 -- for the window and every window in
// it, shown or not. The root's is all of it; DXEngine's 4:3 root is drawn
// over the whole screen, its children across it (RootOffsetX), so the
// root's is the screen in its units, clipped again as the screen changes
// (URootWindow::ResizeRoot). The tab groups' places read them
// (MoveTabGroup), as the root's tick seeds the focus.
void UWindow::ClipTree()
{
	ClipRect& clip = winClipRect();
	if (UWindow* parent = parentOwner())
	{
		const ClipRect& outer = parent->winClipRect();
		clip.originX = outer.originX + UsedX;
		clip.originY = outer.originY + UsedY;
		float left = std::max(clip.originX, outer.originX + outer.clipX);
		float top = std::max(clip.originY, outer.originY + outer.clipY);
		float right = std::min(clip.originX + Width(), outer.originX + outer.clipX + outer.clipWidth);
		float bottom = std::min(clip.originY + Height(), outer.originY + outer.clipY + outer.clipHeight);
		clip.clipX = left - clip.originX;
		clip.clipY = top - clip.originY;
		clip.clipWidth = right - left;
		clip.clipHeight = bottom - top;
	}
	else
	{
		clip = {};
		clip.clipWidth = Width();
		clip.clipHeight = Height();
		if (engine->viewport)
		{
			float scale = GetVirtualScale();
			clip.clipX = -UsedX;
			clip.clipY = -UsedY;
			clip.clipWidth = engine->viewport->ViewportWidth() / scale;
			clip.clipHeight = engine->viewport->ViewportHeight() / scale;
		}
	}
	for (UWindow* child = firstChild(); child; child = child->nextSibling())
		child->ClipTree();
}

// The root's own tick (Extension.dll XRootWindow::Tick 0x1003a540): while
// nothing has the focus, the topmost modal -- the root itself with none up --
// seeds it, unless its focusMode is MFOCUS_EnterLeave (the keypad's): its
// preferred window if that can take the focus (SetFocusWindow keeps the last
// focus there), else as a move to the next tab group gives it, from the
// table's first. This is how a conversation's choices light up, the frame
// after they appear, and a menu's first button too. The original then ticks
// its modal half (an accelerator-table rebuild DXEngine has no table for).
// The tab groups go in the order of their windows' places, which a window
// has as soon as it is made (AskParentForReconfigure); the root's tick
// seeds it (URootWindow::Tick).
void UWindow::SeedFocus()
{
	URootWindow* root = GetRootWindow();
	if (!root || root->FocusWindow())
		return;
	UModalWindow* holder = UObject::TryCast<UModalWindow>(TopmostModal(root));
	if (holder && holder->focusMode() != (uint8_t)EMouseFocusMode::EnterLeave)
	{
		if (holder->preferredFocus() && holder->preferredFocus()->IsTraversable(true))
			root->SetRootFocusWindow(holder->preferredFocus());
		if (!root->FocusWindow())
			MoveTabGroup(true);
	}
}

void UWindow::Tick(float timeElapsed)
{
	if (bTickEnabled())
		CallEvent(this, "Tick", { ExpressionValue::FloatValue(timeElapsed) });

	// The original's (XWindow::InvokeTimers 0x10051340): each timer once a
	// tick, the newest first; one past its time calls its function once with
	// its id, how many of its time outs have passed (a single one 1, and then
	// gone) and its client data. One added meanwhile waits for the next tick.
	std::vector<int> timerIds;
	for (auto it = ActiveTimers.rbegin(); it != ActiveTimers.rend(); ++it)
		timerIds.push_back(it->first);
	for (int timerId : timerIds)
	{
		if (Destroyed)
			return;
		auto it = ActiveTimers.find(timerId);
		if (it == ActiveTimers.end())
			continue; // removed by an earlier one's function

		WTimer& timer = it->second;
		timer.TimeLeft -= timeElapsed;
		if (timer.TimeLeft >= 0.0f)
			continue;

		int invocations = 1;
		if (timer.Loop)
		{
			invocations = (int)(int64_t)(-timer.TimeLeft / timer.Timeout) + 1;
			timer.TimeLeft += invocations * timer.Timeout;
		}
		NameString functionName = timer.FunctionName;
		int clientData = timer.ClientData;
		if (!timer.Loop)
			ActiveTimers.erase(it);

		if (!functionName.IsNone())
			CallEvent(this, functionName, { ExpressionValue::IntValue(timerId), ExpressionValue::IntValue(invocations), ExpressionValue::IntValue(clientData) });
	}

	// The children as they were, the top one first, as the original's walk
	// (Extension.dll XRootWindow::TickTree 0x1003bfc0) ticks a window's
	// higher siblings before it: one that destroys itself (the keypad's
	// PopWindow) leaves its siblings to tick, as each window is locked
	// there; one destroyed meanwhile is passed over
	if (Destroyed)
		return;
	Array<UWindow*> children;
	for (auto cur = lastChild(); cur; cur = cur->prevSibling())
		children.push_back(cur);
	for (UWindow* child : children)
	{
		if (!child->Destroyed)
			child->Tick(timeElapsed);
	}
}

void UWindow::DrawDebugBox(UGC* gc)
{
	Rectf dest = gc->ScaleRect(Rectf::xywh(gc->offsetX, gc->offsetY, Width(), Height()));
	engine->render->Draw2DLine(vec4(1.0f), 0, vec3(dest.left, dest.top, 1.0f), vec3(dest.right, dest.top, 1.0f), false);
	engine->render->Draw2DLine(vec4(1.0f), 0, vec3(dest.left, dest.bottom, 1.0f), vec3(dest.right, dest.bottom, 1.0f), false);
	engine->render->Draw2DLine(vec4(1.0f), 0, vec3(dest.left, dest.top, 1.0f), vec3(dest.left, dest.bottom, 1.0f), false);
	engine->render->Draw2DLine(vec4(1.0f), 0, vec3(dest.right, dest.top, 1.0f), vec3(dest.right, dest.bottom, 1.0f), false);
}
