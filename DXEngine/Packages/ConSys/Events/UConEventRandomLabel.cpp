
#include "Precomp.h"
#include "UConEventRandomLabel.h"

void UConEventRandomLabel::Load(ObjectStream* stream)
{
	UConEvent::Load(stream);
	int count = stream->ReadIndex();
	for (int i = 0; i < count; i++)
	{
		std::string label = stream->ReadString();
		labels.push_back(std::move(label));
	}
}

std::string UConEventRandomLabel::GetLabel(int labelIndex)
{
	// One it has not is none, not a read past its labels
	if (labelIndex < 0 || labelIndex >= (int)labels.size())
		return {};
	return labels[labelIndex];
}

int UConEventRandomLabel::GetLabelCount()
{
	return (int)labels.size();
}

std::string UConEventRandomLabel::GetRandomLabel()
{
	// The original's (ConSys.dll 0x1000a270): bLabelsCycled set when
	// cycleIndex has reached the count; a random label when not cycling, or
	// with bCycleRandom once the labels have cycled; else with bCycleOnce the
	// last label once cycleIndex is on it, never counted past it, so the
	// labels never cycle and bCycleRandom never picks; else the label at
	// cycleIndex modulo the count, cycleIndex counted on.
	int count = (int)labels.size();
	if (count <= 0)
		return {};

	int index = cycleIndex();
	if (index == count)
		bLabelsCycled() = true;

	int pick;
	if (!bCycleEvents() || (bCycleRandom() && bLabelsCycled()))
	{
		pick = std::rand() % count;
	}
	else if (bCycleOnce() && index == count - 1)
	{
		pick = count - 1;
	}
	else
	{
		pick = index % count;
		cycleIndex() = index + 1;
	}
	// A cycleIndex a script set below 0 has no label, where the original
	// reads before its array
	return pick >= 0 ? labels[pick] : std::string();
}
