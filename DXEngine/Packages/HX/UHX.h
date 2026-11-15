#pragma once

#include "Packages/DeusEx/UDeusExDecoration.h"
#include "Packages/DeusEx/UDeusExPlayer.h"
#include "Packages/DeusEx/UScriptedPawn.h"
#include "Packages/Engine/URenderIterator.h"
#include "Packages/Extension/Windows/UWindow.h"
#include "Packages/Extension/Windows/Text/UTextLogWindow.h"

// HX, the co-op mod by Sebastian Kaufel (its last build, 0.9.89.4): the C++
// of its HX.dll that DXEngine carries, for the player's own HX.u
// (dx-reverse-info hx-dll.md). None of it runs unless HX's package is
// loaded: these are the classes of HX's objects.

// HX's own properties that its C++ reads, found in HX.u's classes the first
// time they are asked for: HX's package is loaded by then, its objects
// existing.
struct HXPropertyOffsets
{
	PropertyDataOffset DecorationBurnTime;
	PropertyDataOffset DecorationPushTime;
	PropertyDataOffset DecorationRenderProxy;
	PropertyDataOffset DecorationSwimPhi;
	PropertyDataOffset DecorationSwimAlpha;
	PropertyDataOffset DecorationSwimOmega;
	PropertyDataOffset DecorationSwimAmp;
	PropertyDataOffset PlayerLipSynchTime;
	PropertyDataOffset PlayerNanoVirusTime;
	PropertyDataOffset PlayerWindowScale;
	PropertyDataOffset PawnLipSynchTime;
	PropertyDataOffset EffectsTime;
	PropertyDataOffset FlagCollections[16];
	PropertyDataOffset ClientFlagCollections;
	PropertyDataOffset WindowPlayerPawn;
	PropertyDataOffset WindowRootWindow;
	PropertyDataOffset TextLogTextColor;
	PropertyDataOffset IteratorSavedLocation;
	PropertyDataOffset IteratorSavedRotation;
};
const HXPropertyOffsets& HXProps();

// Whether an object is of HX's class of that name or one of its children.
bool IsHXClass(UObject* obj, const char* className);

// HX.dll's classes with no script that HX's script names, added to its
// package as it loads: HXGameEngine, the game engine whose part DXEngine
// plays when the ini names it (HXPlayerPawn.VerifyGameEngine loads it).
class PackageManager;
class Package;
void AddHXRuntimeClasses(PackageManager* packages, Package* hx);

// HX's decoration (AHXDecoration::Tick): on a full tick its float on water
// blends in and out (SwimAlpha), whether or not it waits for its owner;
// once its tick is not deferred, its burn and push countdowns run out into
// burnTimer and pushTimer, which stand in for the game's Timer.
class UHXDecoration : public UDeusExDecoration
{
public:
	using UDeusExDecoration::UDeusExDecoration;

	void Tick(float elapsed) override;
};

// HX's player (AHXPlayerPawn): no flag base of its own, HX's Steve holding
// the game's flags; on a full tick not deferred, TickStuff for any role on a
// level not the Entry, and its lip-sync and nano-virus countdowns.
class UHXPlayerPawn : public UDeusExPlayer
{
public:
	using UDeusExPlayer::UDeusExPlayer;

	void InitNativeDefaults() override;
	void Tick(float elapsed) override;
	void RemoveAdditionalView(UActor* actor) override;
};

// HX's NPC (AHXScriptedPawn::Tick): after the game's NPC tick, its lip-sync
// countdown on a full tick not deferred.
class UHXScriptedPawn : public UScriptedPawn
{
public:
	using UScriptedPawn::UScriptedPawn;

	void Tick(float elapsed) override;
};

// HX's effects (AHXEffects::Tick): its EffectsTime counts down into
// EffectsTimer on a full tick not deferred.
class UHXEffects : public UActor
{
public:
	using UActor::UActor;

	void Tick(float elapsed) override;
};

// HX's render iterators (UHXRenderIterator): the copy of an actor's look
// onto another (CopyRenderProperties) that its decorations' iterator draws
// a carried decoration's stand-in with.
class UHXRenderIterator : public URenderIterator
{
public:
	using URenderIterator::URenderIterator;

	static void CopyRenderProperties(UActor* dest, UActor* source);
};

// An iterator whose every step is its script's function of that name
// (UHXScriptedRenderIterator): Init with the viewer, First, Next, IsDone and
// CurrentItem. Its Init leaves Observer to the script.
class UHXScriptedRenderIterator : public UHXRenderIterator
{
public:
	using UHXRenderIterator::UHXRenderIterator;

	void Init(UPlayerPawn* camera) override;
	void First() override;
	bool IsDone() override;
	UActor* CurrentItem() override;
	void Next() override;
};

// Every HX decoration's own iterator (UHXDecorationRenderIterator), one item
// a frame: the decoration bobbing on water, or carried before its carrier --
// for the local carrier a translucent stand-in (HXRenderProxy) -- its place
// and rotation put back once drawn.
class UHXDecorationRenderIterator : public UHXRenderIterator
{
public:
	using UHXRenderIterator::UHXRenderIterator;

	void Init(UPlayerPawn* camera) override;
	void First() override;
	bool IsDone() override;
	UActor* CurrentItem() override;
	void Next() override;
};

// HX's window (UHXWindow::Init): its root and the root's player, an HX
// player or a fatal error, set before the script's InitWindow.
class UHXWindow : public UWindow
{
public:
	using UWindow::UWindow;

	void InitDefaults() override;
};

// HX's text log (UHXTextLogWindow): a line with its own colour and time
// (TextColor and the log's timeout by default), and each line leaving as its
// own time runs out, not only the oldest first.
class UHXTextLogWindow : public UTextLogWindow
{
public:
	using UTextLogWindow::UTextLogWindow;

	void Tick(float timeElapsed) override;

	void AddLine(const std::string& text, std::optional<Color> color, std::optional<float> timeout);
	int NumLines() const { return (int)logs.size(); }
};
