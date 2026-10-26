
#include "Precomp.h"
#include "UActor.h"
#include "Packages/Engine/Actors/Pawn/UPawn.h"
#include "VM/ScriptCall.h"
#include "Engine.h"

void UActor::CheckPendingTouch()
{
	if (PendingTouch())
	{
		CallEvent(PendingTouch(), EventName::PostTouch, { ExpressionValue::ObjectValue(this) });
		if (PendingTouch())
		{
			UActor* cur = PendingTouch();
			UActor* next = cur->PendingTouch();
			PendingTouch() = next;
			cur->PendingTouch() = nullptr;
		}
	}
}

// The original's AActor::BeginTouch (Engine.dll 0x10369570): each side
// makes its own link (0x10369630), this one first, then the other unless this
// one's Touch event undid the link. A side already touching the other does
// nothing more. Else it takes its last free slot of the four, and with none
// frees one: every toucher with no physics is untouched (the last of them
// gives the slot); else, for a pawn coming, the first toucher that is no
// pawn; else, for a player coming, the first that is no player; with no slot
// freed the side stays unlinked, the other side linked all the same. The
// other goes in the slot and this side's Touch is sent.
bool UActor::TouchOneSide(UActor* other)
{
	auto slots = Touching();
	int slot = -1;
	for (int i = 0; i < TouchingArraySize; i++)
	{
		if (slots[i] == other)
			return true;
		if (!slots[i])
			slot = i;
	}
	if (slot < 0)
	{
		for (int i = 0; i < TouchingArraySize; i++)
		{
			if (slots[i] && slots[i]->Physics() == PHYS_None)
			{
				UnTouch(slots[i], true);
				slot = i;
			}
		}
		UPawn* otherPawn = UObject::TryCast<UPawn>(other);
		if (slot < 0 && otherPawn)
		{
			for (int i = 0; i < TouchingArraySize; i++)
			{
				if (slots[i] && !UObject::TryCast<UPawn>(slots[i]))
				{
					UnTouch(slots[i], true);
					slot = i;
					break;
				}
			}
		}
		if (slot < 0 && otherPawn && otherPawn->bIsPlayer())
		{
			for (int i = 0; i < TouchingArraySize; i++)
			{
				UPawn* pawn = UObject::TryCast<UPawn>(slots[i]);
				if (slots[i] && !(pawn && pawn->bIsPlayer()))
				{
					UnTouch(slots[i], true);
					slot = i;
					break;
				}
			}
		}
		if (slot < 0)
			return true;
	}

	slots[slot] = other;
	CallEvent(this, EventName::Touch, { ExpressionValue::ObjectValue(other) });
	return slots[slot] == other;
}

void UActor::Touch(UActor* actor)
{
	// Don't setup touch if any object has been destroyed
	if (bDeleteMe() || actor->bDeleteMe() || actor == this)
		return;

	if (TouchOneSide(actor))
		actor->TouchOneSide(this);
}

// The original's AActor::EndTouch(Other, bNoNotifySelf) (Engine.dll
// 0x10369970): this actor's slot holding the other is cleared and, unless
// told not to (a destroyed actor's untouch), its UnTouch sent; then the
// other's slot holding this one, and the other's UnTouch sent.
void UActor::UnTouch(UActor* actor, bool notifySelf)
{
	auto slots = Touching();
	auto otherSlots = actor->Touching();

	for (int i = 0; i < TouchingArraySize; i++)
	{
		if (slots[i] == actor)
		{
			slots[i] = nullptr;
			if (notifySelf)
				CallEvent(this, EventName::UnTouch, { ExpressionValue::ObjectValue(actor) });
			break;
		}
	}

	for (int i = 0; i < TouchingArraySize; i++)
	{
		if (otherSlots[i] == this)
		{
			otherSlots[i] = nullptr;
			CallEvent(actor, EventName::UnTouch, { ExpressionValue::ObjectValue(this) });
			break;
		}
	}
}
