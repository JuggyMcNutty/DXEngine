#pragma once

#include "Packages/Engine/Actors/UActor.h"

class VisibleFrame;

class VisibleActor
{
public:
	void Process(VisibleFrame* frame, UActor* actor);
	// Whether the original's SetupDynamics passes over the actor in a frame
	// portalDepth mirrors and portals deep: so it is not drawn, and a mover
	// is not filed again
	static bool PassedOver(UActor* actor, int portalDepth);
	void DrawOpaque(VisibleFrame* frame);
	void DrawTranslucent(VisibleFrame* frame);

	EDrawType Type = DT_None;
	UActor* Actor = nullptr;
};
