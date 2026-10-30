
#include "Precomp.h"
#include "UActor.h"
#include "VM/ScriptCall.h"
#include "Packages/Engine/Actors/Decoration/UDecoration.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Engine.h"

// The original's AMover::PostEditChange (Engine.dll 0x103afbd0), the
// editor's but run in play too: its key held to 7, its base made again from
// where it is and where its last move started, it put at that base and its
// key's place, no move made; then PostEditMove (0x103af920): from key 1 on,
// that key's place and turn made again from where it stands, and it put at
// its base and that place. So a mover at rest at a key past its first, its
// move started from the first, goes on by that key's place.
void UMover::PostEditChange()
{
	if (KeyNum() > 7)
		KeyNum() = 7;
	BasePos() = Location() - OldPos();
	BaseRot() = Rotation() - OldRot();
	OldPos() = KeyPos()[KeyNum()];
	OldRot() = KeyRot()[KeyNum()];
	Location() = OldPos() + BasePos();
	Rotation() = OldRot() + BaseRot();

	int key = KeyNum();
	if (key != 0)
	{
		KeyPos()[key] = Location() - (KeyPos()[0] + BasePos());
		KeyRot()[key] = Rotation() - (BaseRot() + KeyRot()[0]);
		OldPos() = KeyPos()[key];
		OldRot() = KeyRot()[key];
	}
	else
	{
		BasePos() = Location() - OldPos();
		BaseRot() = Rotation() - OldRot();
	}
	Location() = BasePos() + KeyPos()[key];
}

void UActor::TickMovingBrush(float elapsed)
{
	OldLocation() = Location();

	UMover* mover = UObject::TryCast<UMover>(this);
	if (mover)
	{
		float timeLeft = elapsed;
		while (timeLeft > 0.0f)
		{
			if (!bInterpolating())
				break;

			if (PhysRate() <= 0.0f)
				break;

			float physAlpha = PhysAlpha();
			float physRate = PhysRate();

			physAlpha += physRate * timeLeft;
			if (physAlpha > 1.0f)
			{
				timeLeft = (physAlpha - 1.0f) / physRate;
				physAlpha = 1.0f;
			}
			else
			{
				timeLeft = 0.0f;
			}

			float t = physAlpha;
			if (mover->MoverGlideType() == 1/*MV_GlideByTime*/)
				t = smoothstep(0.0f, 1.0f, t);

			int keyIndex = clamp((int)mover->KeyNum(), 0, 7);
			vec3 oldpos = mover->OldPos();
			vec3 basepos = mover->BasePos();
			vec3 keypos = mover->KeyPos()[keyIndex];
			Rotator oldrot = mover->OldRot();
			Rotator baserot = mover->BaseRot();
			Rotator keyrot = mover->KeyRot()[keyIndex];

			vec3 deltapos = basepos + keypos - oldpos;
			vec3 targetPos = oldpos + deltapos * t;

			Rotator targetRotation = oldrot + (baserot + keyrot - oldrot) * t;

			// LogMessage("Moving brush: " + std::to_string(t) + " key=" + std::to_string(keyIndex) +" keypos=(" + std::to_string(keypos.x) + "," + std::to_string(keypos.y) + "," + std::to_string(keypos.z) + ")");

			if (MoveBrush(targetPos - Location(), targetRotation))
			{
				PhysAlpha() = physAlpha;

				if (physAlpha == 1.0f)
				{
					bInterpolating() = false;
					CallEvent(this, EventName::InterpolateEnd, { ExpressionValue::ObjectValue(nullptr) });
				}
			}
		}
	}

	if (elapsed > 0.0f)
		Velocity() = (Location() - OldLocation()) / elapsed;
}
