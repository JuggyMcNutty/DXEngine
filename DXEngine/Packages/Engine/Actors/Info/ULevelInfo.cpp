
#include "Precomp.h"
#include "ULevelInfo.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"

void ULevelInfo::InitActorZone()
{
	Region() = OwnRegion();
}

void ULevelInfo::UpdateActorZone()
{
	// No zone events are sent by LevelInfo actors
	Region() = OwnRegion();
}

PointRegion ULevelInfo::GetLocZone(const vec3& pos)
{
	return XLevel()->Model->FindRegion(pos, Level());
}
