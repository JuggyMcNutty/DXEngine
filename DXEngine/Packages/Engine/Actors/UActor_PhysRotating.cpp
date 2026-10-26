
#include "Precomp.h"
#include "UActor.h"
#include "VM/ScriptCall.h"

// AActor::fixedTurn (Engine.dll 0x103c9dc0): a yaw, pitch or roll turned by
// delta toward desired. With bFixedRotationDir and bRotateToDesired the turn
// goes delta's way and stops at the desired angle; with bFixedRotationDir
// alone it goes on; with bRotateToDesired alone it goes the shortest way, by
// delta's size; with neither, nothing. Angles are kept to 16 bits.
static int FixedTurn(int current, int desired, int delta, bool fixedDir, bool toDesired)
{
	current &= 0xffff;
	desired &= 0xffff;
	if (delta == 0)
		return current;
	if (fixedDir)
	{
		if (toDesired)
		{
			if (delta <= 0)
			{
				int from = current < desired ? current + 0x10000 : current;
				int room = desired - from;
				return ((delta >= room ? delta : room) + current) & 0xffff;
			}
			int to = current > desired ? desired + 0x10000 : desired;
			int room = to - current;
			return ((delta <= room ? delta : room) + current) & 0xffff;
		}
		return (delta + current) & 0xffff;
	}
	if (!toDesired)
		return current;
	int size = std::abs(delta);
	if (current <= desired)
	{
		int ahead = desired - current;
		if (ahead < 0x8000)
			return (current + std::min(ahead, size)) & 0xffff;
		int back = current - desired + 0x10000;
		return (current - std::min(back, size)) & 0xffff;
	}
	int back = current - desired;
	if (back < 0x8000)
		return (current - std::min(back, size)) & 0xffff;
	int ahead = desired - current + 0x10000;
	return (current + std::min(ahead, size)) & 0xffff;
}

// AActor::physicsRotation (Engine.dll 0x103ca1e0), after an actor's physics
// each tick when its RotationRate has a component (performPhysics,
// 0x103c9770; a pawn's is its own): with bRotateToDesired or
// bFixedRotationDir, and not at DesiredRotation when turning to it, yaw,
// pitch and roll each turn by their rate x the tick, fixedTurn's way. A
// rotation that changed goes through a move of nothing (a brush turns as a
// move does, what stands on the actor turns with it); at the desired
// rotation, EndedRotation where probed.
void UActor::TickRotating(float elapsed)
{
	if (((RotationRate().Pitch | RotationRate().Yaw | RotationRate().Roll) & 0xffff) == 0)
		return;
	bool toDesired = bRotateToDesired();
	bool fixedDir = bFixedRotationDir();
	if (!toDesired && !fixedDir)
		return;
	if (toDesired && Rotation() == DesiredRotation())
		return;

	int deltaYaw = (int)((double)RotationRate().Yaw * elapsed);
	int deltaPitch = (int)((double)RotationRate().Pitch * elapsed);
	int deltaRoll = (int)((double)RotationRate().Roll * elapsed);
	Rotator rot = Rotation();
	if (deltaYaw != 0 && (!toDesired || DesiredRotation().Yaw != rot.Yaw))
		rot.Yaw = FixedTurn(rot.Yaw, DesiredRotation().Yaw, deltaYaw, fixedDir, toDesired);
	if (deltaPitch != 0 && (!toDesired || DesiredRotation().Pitch != rot.Pitch))
		rot.Pitch = FixedTurn(rot.Pitch, DesiredRotation().Pitch, deltaPitch, fixedDir, toDesired);
	if (deltaRoll != 0 && (!toDesired || DesiredRotation().Roll != rot.Roll))
		rot.Roll = FixedTurn(rot.Roll, DesiredRotation().Roll, deltaRoll, fixedDir, toDesired);
	if (rot != Rotation())
		SetRotation(rot);

	if (toDesired && Rotation() == DesiredRotation())
		CallEvent(this, EventName::EndedRotation);
}
