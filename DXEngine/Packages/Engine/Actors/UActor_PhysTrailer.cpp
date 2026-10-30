
#include "Precomp.h"
#include "UActor.h"
#include "Engine.h"

// AActor::physTrailer (Engine.dll 0x103d7850): an actor with an owner
// follows it. A sprite goes to the owner's place plus its PrePivot with
// bTrailerPrePivot, else, with bTrailerSameRotation, the owner's place less
// its facing x the trailer's Mass, else the owner's place. Anything else
// goes to the owner's place and turns: to the owner's rotation with
// bTrailerSameRotation, else against the owner's velocity, or straight up
// when the owner has none. Each a far move without a check.
void UActor::TickTrailer(float elapsed)
{
	UActor* owner = Owner();
	if (!owner)
		return;

	if (DrawType() == DT_Sprite)
	{
		vec3 place = owner->Location();
		if (bTrailerPrePivot())
			place += PrePivot();
		else if (bTrailerSameRotation())
			place -= Coords::Rotation(owner->Rotation()).XAxis * Mass(); // the original's FCoords from a rotator: its sine table's
		SetLocation(place, true);
		return;
	}

	SetLocation(owner->Location(), true);
	Rotator rot;
	const vec3& v = owner->Velocity();
	if (bTrailerSameRotation())
		rot = owner->Rotation();
	else if (std::abs(v.x) >= 0.0001f || std::abs(v.y) >= 0.0001f || std::abs(v.z) >= 0.0001f)
		rot = Rotator::FromVector(-v);
	else
		rot = Rotator(16384, 0, 0);
	SetRotation(rot);
}
