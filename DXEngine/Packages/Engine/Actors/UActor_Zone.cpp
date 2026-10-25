
#include "Precomp.h"
#include "UActor.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "VM/ScriptCall.h"
#include "Engine.h"

PointRegion UActor::FindRegion(const vec3& offset)
{
	return XLevel()->Model->FindRegion(Location() + offset, Level());
}

// A spawned actor's zone, as the original's SpawnActor sets it: the region
// alone, no event and no change of physics -- Surreal Engine gave an actor
// spawned into water PHYS_Swimming, which moves only a pawn, so an AirBubble
// (an Effects with PHYS_Projectile) hung where it was spawned
// (dx-reverse-info/engine-dll.md, the zone an actor is in)
void UActor::InitActorZone()
{
	Region() = FindRegion();
}

void UActor::UpdateActorZone()
{
	if (bDeleteMe())
		return;

	PointRegion oldregion = Region();
	PointRegion newregion = FindRegion();

	if (oldregion.Zone != newregion.Zone)
	{
		// ZoneChange still sees the zone being left in Region: scripts
		// compare the two (a decoration splashes only coming out of dry).
		if (oldregion.Zone)
			CallEvent(oldregion.Zone, EventName::ActorLeaving, { ExpressionValue::ObjectValue(this) });
		if (newregion.Zone)
			CallEvent(this, EventName::ZoneChange, { ExpressionValue::ObjectValue(newregion.Zone) });
		Region() = newregion;
		if (newregion.Zone)
			CallEvent(newregion.Zone, EventName::ActorEntered, { ExpressionValue::ObjectValue(this) });
	}
	else
	{
		Region() = newregion;
	}

	// Destroying what enters a destructive zone, and inventory left in a
	// no-inventory zone, are Deus Ex's scripts' own (ZoneInfo.ActorEntered
	// gives stray inventory 1.5 seconds; decorations and fragments check
	// bDestructive): its engine does neither.
}
