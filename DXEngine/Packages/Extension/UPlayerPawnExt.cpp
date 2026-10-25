
#include "Precomp.h"
#include "UPlayerPawnExt.h"
#include "Engine.h"
#include "Packages/Engine/UViewport.h"
#include "Utils/Logger.h"
#include "Package/PackageManager.h"
#include "Packages/Extension/Windows/TabGroup/URootWindow.h"
#include "Render/RenderSubsystem.h"

void UPlayerPawnExt::InitRootWindow()
{
	// A root per pawn, made only when it has none, as the original's
	// InitRootWindow (Extension.dll 0x10037e0b) makes it; the engine draws,
	// ticks and feeds the viewport's own player's. Every Possess once made a
	// new one and stored it as the engine's whatever pawn called, so a client
	// joining a listen server took the host's root until the next map load.
	bool local = engine->viewport && engine->viewport->Actor() == this;
	if (RootWindow())
	{
		if (local)
			engine->dxRootWindow = RootWindow();
		return;
	}

	auto dxIni = engine->packages->GetIniFile("System");
	NameString dxRootClassName = dxIni->GetValue("Engine.Engine", "Root", "");
	UClass* cls = engine->packages->FindClass(dxRootClassName);
	if (cls)
	{
		URootWindow* root = UObject::Cast<URootWindow>(engine->packages->GetTransientPackage()->NewObject("dxRootWindow", cls, ObjectFlags::Transient));
		RootWindow() = root;
		if (local)
			engine->dxRootWindow = root;
		LogMessage(std::string("Root window made for ") + Name.ToString() + (local ? " (the engine's)" : " (the pawn's own)"));
		root->parentPawn() = this;
		root->bIsVisible() = true;
		root->bIsSensitive() = true;
		// The root's own window type, as XRootWindow::Init sets it: 3, the
		// root -- the anchor GetTabGroupWindow walks up to.
		root->windowType() = 3;
		// A root starts with the world drawn, as the original's init sets it;
		// only the snapshot and black backgrounds and the credits turn it off.
		root->bRender() = true;
		// And with its pointer shown, as the original's init sets it
		// (XRootWindow::Init): a window may hide it (ShowCursor).
		root->bCursorVisible() = true;
		// And with positional sound on, as the original's init sets it: a
		// window's sound turned by its place across the screen (PlaySound).
		root->bPositionalSound() = true;
		root->InitWindow();
	}
}

void UPlayerPawnExt::PreRenderWindows(UCanvas* canvas)
{
	engine->render->PreRenderWindows(canvas);
}

void UPlayerPawnExt::PostRenderWindows(UCanvas* canvas)
{
	engine->render->PostRenderWindows(canvas);
}
