
#include "Precomp.h"
#include "UMusic.h"

void UMusic::Load(ObjectStream* stream)
{
	UObject::Load(stream);

	Format = stream->ReadName();
	if (stream->GetVersion() > 61)
		stream->ReadUInt32(); // lazy array skip offset
	// The count from the file, held before the allocation: a negative one was
	// up to 4 GB
	int size = stream->ReadIndex();
	if (size < 0 || size > 256 * 1024 * 1024)
		Exception::Throw(Name.ToString() + ": " + std::to_string(size) + " bytes of data");
	Data.resize(size);
	stream->ReadBytes(Data.data(), size);
}

void UMusic::Save(PackageStreamWriter* stream)
{
	UObject::Save(stream);

	stream->WriteName(Format);
	if (stream->GetVersion() > 61)
		stream->BeginSkipOffset();
	stream->WriteIndex((int)Data.size());
	stream->WriteBytes(Data.data(), (uint32_t)Data.size());
	if (stream->GetVersion() > 61)
		stream->EndSkipOffset();
}
