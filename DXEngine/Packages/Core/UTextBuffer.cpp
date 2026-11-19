
#include "Precomp.h"
#include "UTextBuffer.h"

void UTextBuffer::Load(ObjectStream* stream)
{
	UObject::Load(stream);

	Pos = stream->ReadUInt32();
	Top = stream->ReadUInt32();

	// Held to what the object has left before it is allocated, a guard of
	// ours (bugs.md PKG.61): a sound file holds every byte it counts
	int size = stream->ReadIndex();
	if (size > 0 && (uint32_t)size > stream->Remaining())
		Exception::Throw("Text buffer " + Name.ToString() + " of " + std::to_string(size) + " bytes");
	if (size > 0)
	{
		Text.resize(size);
		stream->ReadBytes(&Text[0], size);
		if (!Text.empty() && Text.back() == 0) Text.pop_back();
	}
}

void UTextBuffer::Save(PackageStreamWriter* stream)
{
	UObject::Save(stream);
	stream->WriteUInt32(Pos);
	stream->WriteUInt32(Top);
	stream->WriteIndex((int)Text.size() + 1);
	stream->WriteBytes(Text.c_str(), (uint32_t)Text.size() + 1);
}
