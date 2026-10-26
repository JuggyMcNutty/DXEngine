
#include "Precomp.h"
#include "UActor.h"
#include "VM/ScriptCall.h"
#include "Packages/Core/UClass.h"
#include "Packages/Engine/Actors/Decoration/UDecoration.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/Keypoint/UInterpolationPoint.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Engine.h"

// AActor::physPathing (Engine.dll 0x103dd8a0), once a tick with the whole
// tick (performPhysics): while the rate is not 0, the actor interpolates
// and time is left -- from its Target, an InterpolationPoint, to that
// point's Next: the rate's modifier, the level's TimeDilation and a
// player's flash, fog and field of view blended between the two by the
// alpha; the alpha advanced by the rate x its modifier x the time, held to
// 0 to 1; the place and rotation between the points, a spline through the
// four round them where the points before and after exist (the weights
// x^4/16 - x^2/2 + 1 at the alpha + 1, the alpha, - 1 and - 2, each
// rotation term truncated), else straight; one move with the turn, a pawn's
// view turned with it. Past 1 (or under 0 at a negative rate) the alpha
// starts over, the time past the end is kept for the next point, the point
// and the actor get InterpolateEnd, and the Target steps on past the points
// with bSkipNextPath; running backwards the actor gets InterpolateEnd(None)
// as well. The tick's velocity is the move over the tick.
static float Splerp(float t)
{
	float s = t * t;
	return s * s * (1.0f / 16.0f) - s * 0.5f + 1.0f;
}

static Rotator Scaled(const Rotator& r, float w)
{
	return Rotator((int)((double)r.Pitch * w), (int)((double)r.Yaw * w), (int)((double)r.Roll * w));
}

void UActor::TickInterpolating(float elapsed)
{
	OldLocation() = Location();
	UPlayerPawn* player = UObject::TryCast<UPlayerPawn>(this);

	float timeLeft = elapsed;
	while (PhysRate() != 0.0f && bInterpolating() && timeLeft > 0.0f)
	{
		UInterpolationPoint* target = UObject::TryCast<UInterpolationPoint>(Target());
		UInterpolationPoint* next = target ? target->Next() : nullptr;
		float alpha = PhysAlpha();
		float rateModifier = 1.0f;
		if (target && next)
		{
			rateModifier = (1.0f - alpha) * target->RateModifier() + next->RateModifier() * alpha;
			Level()->TimeDilation() = (1.0f - alpha) * target->GameSpeedModifier() + next->GameSpeedModifier() * alpha;
			if (player)
			{
				float scale = (1.0f - alpha) * target->ScreenFlashScale() + next->ScreenFlashScale() * alpha;
				player->DesiredFlashScale() = scale;
				player->FlashScale() = vec3(scale);
				vec3 fog = target->ScreenFlashFog() * (1.0f - alpha) + next->ScreenFlashFog() * alpha;
				player->FlashFog() = fog;
				player->DesiredFlashFog() = fog;
				player->FovAngle() = ((1.0f - alpha) * target->FovModifier() + next->FovModifier() * alpha) * Class->GetDefaultObject<UPlayerPawn>()->FovAngle();
			}
		}

		float newAlpha = alpha + rateModifier * PhysRate() * timeLeft;
		float a = clamp(newAlpha, 0.0f, 1.0f);
		PhysAlpha() = a;

		if (target && next)
		{
			vec3 location;
			Rotator rotation;
			UInterpolationPoint* prev = target->Prev();
			UInterpolationPoint* nextnext = next->Next();
			if (prev && nextnext)
			{
				float w0 = Splerp(a + 1.0f), w1 = Splerp(a), w2 = Splerp(a - 1.0f), w3 = Splerp(a - 2.0f);
				float rw = 1.0f / (w0 + w1 + w2 + w3);
				location = (prev->Location() * w0 + target->Location() * w1 + next->Location() * w2 + nextnext->Location() * w3) * rw;
				Rotator sum = Scaled(prev->Rotation(), w0) + Scaled(target->Rotation(), w1) + Scaled(next->Rotation(), w2) + Scaled(nextnext->Rotation(), w3);
				rotation = Scaled(sum, rw);
			}
			else
			{
				location = next->Location() * a + target->Location() * (1.0f - a);
				rotation = Scaled(next->Rotation(), a) + Scaled(target->Rotation(), 1.0f - a);
			}
			TryMoveHeldOff(location - Location());
			if (bDeleteMe())
				return;
			SetRotation(rotation);
			if (UPawn* pawn = UObject::TryCast<UPawn>(this))
				pawn->ViewRotation() = Rotation();
		}

		if (PhysRate() > 0.0f && newAlpha > 1.0f)
		{
			PhysAlpha() = 0.0f;
			timeLeft = (newAlpha - 1.0f) / (newAlpha - alpha) * timeLeft;
			if (UActor* reached = Target())
			{
				CallEvent(reached, EventName::InterpolateEnd, { ExpressionValue::ObjectValue(this) });
				CallEvent(this, EventName::InterpolateEnd, { ExpressionValue::ObjectValue(Target()) });
				if (target)
				{
					do
						target = target->Next();
					while (target && target->bSkipNextPath());
					Target() = target;
				}
			}
		}
		else if (PhysRate() < 0.0f && newAlpha < 0.0f)
		{
			PhysAlpha() = 1.0f;
			timeLeft = -newAlpha / (alpha - newAlpha) * timeLeft;
			if (UActor* reached = Target())
			{
				CallEvent(reached, EventName::InterpolateEnd, { ExpressionValue::ObjectValue(this) });
				CallEvent(this, EventName::InterpolateEnd, { ExpressionValue::ObjectValue(Target()) });
				if (target)
				{
					do
						target = target->Prev();
					while (target && target->bSkipNextPath());
					Target() = target;
				}
			}
			CallEvent(this, EventName::InterpolateEnd, { ExpressionValue::ObjectValue(nullptr) });
		}
		else
			timeLeft = 0.0f;
		if (bDeleteMe())
			return;
	}

	if (elapsed > 0.0f)
		Velocity() = (Location() - OldLocation()) / elapsed;
}
