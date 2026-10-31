
#include "Precomp.h"
#include "USound.h"
#include "Engine.h"
#include "Audio/AudioSource.h"
#include "Audio/AudioDevice.h"
#include "Packages/Engine/Subsystems/UDXEngineAudioDevice.h"

#include <algorithm>
#include <cstring>
#include <type_traits>

void USound::Load(ObjectStream* stream)
{
	UObject::Load(stream);

	Format = stream->ReadName();
	if (stream->GetVersion() >= 63)
		stream->ReadUInt32(); // lazy array skip offset
	// The count from the file, held before the allocation: a negative one was
	// up to 4 GB
	int size = stream->ReadIndex();
	if (size < 0 || size > 256 * 1024 * 1024)
		Exception::Throw(Name.ToString() + ": " + std::to_string(size) + " bytes of data");
	Data.resize(size);
	stream->ReadBytes(Data.data(), size);
}

void USound::Save(PackageStreamWriter* stream)
{
	UObject::Save(stream);

	stream->WriteName(Format);
	if (stream->GetVersion() >= 63)
		stream->BeginSkipOffset();
	stream->WriteIndex((int)Data.size());
	stream->WriteBytes(Data.data(), (uint32_t)Data.size());
	if (stream->GetVersion() >= 63)
		stream->EndSkipOffset();
}

// Low Sound Quality (the audio subsystem's LowSoundQuality), as the original
// applies it to a WAV as the sound loads (Engine.dll FSoundData::Load
// 0x1036f040): 16-bit samples made 8-bit, a rate of 22,050 Hz or more halved,
// or both, each sample rounded with the last one's error carried on, and the
// header, the loop points and the chunks after the data mended
// (0x10370010). The data is taken as one channel, as the original takes it.
static void LowerSoundQuality(Array<uint8_t>& wav)
{
	auto u16 = [&](size_t at) { return (uint32_t)(wav[at] | (wav[at + 1] << 8)); };
	auto u32 = [&](size_t at) { return u16(at) | (u16(at + 2) << 16); };
	auto put16 = [&](size_t at, uint32_t v) { wav[at] = (uint8_t)v; wav[at + 1] = (uint8_t)(v >> 8); };
	auto put32 = [&](size_t at, uint32_t v) { put16(at, v & 0xffff); put16(at + 2, v >> 16); };
	auto findChunk = [&](const char* id) -> size_t
	{
		size_t at = 12;
		while (at + 8 < wav.size() && memcmp(&wav[at], id, 4) != 0)
			at += ((u32(at + 4) + 1) & ~1u) + 8;
		return (at + 8 < wav.size() && memcmp(&wav[at], id, 4) == 0) ? at : 0;
	};

	if (wav.size() < 12 || memcmp(&wav[8], "WAVE", 4) != 0)
		return;
	size_t fmt = findChunk("fmt ");
	size_t data = findChunk("data");
	if (!fmt || !data || fmt + 24 > wav.size())
		return;
	size_t smpl = findChunk("smpl");

	uint32_t rate = u32(fmt + 12);
	uint32_t bits = u16(fmt + 22);
	uint32_t size = std::min(u32(data + 4), (uint32_t)(wav.size() - data - 8));
	if (size <= 4)
		return;

	bool to8 = bits == 16;
	bool halve = rate >= 22050;
	if (!to8 && !halve)
		return;

	uint8_t* bytes = &wav[data + 8];
	int16_t* words = reinterpret_cast<int16_t*>(bytes);
	uint32_t newSize = size;
	if (to8 && halve)
	{
		uint32_t count = size / 4;
		int prev = words[0], error = 0;
		for (uint32_t i = 0; i < count; i++)
		{
			int cur = words[2 * i], next = words[2 * i + 1];
			int sum = next + cur + prev + cur + 0x20000 + error;
			prev = next;
			int out = (sum + 512) & (int)0xFFFFFC00;
			if (out > 261120)
				out = 261120;
			bytes[i] = (uint8_t)(out >> 10);
			error = sum - out;
		}
		newSize = count;
		put16(fmt + 22, 8);
		put32(fmt + 12, rate >> 1);
	}
	else if (to8)
	{
		uint32_t count = size / 2;
		int error = 0;
		for (uint32_t i = 0; i < count; i++)
		{
			int sum = error + words[i] + 0x8000;
			int out = (sum + 127) & ~0xFF;
			if (out > 65280)
				out = 65280;
			bytes[i] = (uint8_t)(out >> 8);
			error = sum - out;
		}
		newSize = count;
		put16(fmt + 22, 8);
	}
	else if (bits == 16)
	{
		uint32_t count = size / 4;
		int prev = words[0], error = 0;
		for (uint32_t i = 0; i < count; i++)
		{
			int cur = words[2 * i], next = words[2 * i + 1];
			int sum = next + cur + prev + cur + 0x20000 + error;
			prev = next;
			int out = (sum + 2) & 0x3FFFC;
			words[i] = (int16_t)(uint16_t)((out >> 2) + 0x8000);
			error = sum - out;
		}
		newSize = 2 * count;
		put32(fmt + 12, rate >> 1);
	}
	else if (bits == 8)
	{
		uint32_t count = size / 2;
		int prev = bytes[0], error = 0;
		for (uint32_t i = 0; i < count; i++)
		{
			int cur = bytes[2 * i], next = bytes[2 * i + 1];
			int sum = next + cur + cur + prev + error;
			prev = next;
			int out = (uint16_t)(sum + 2) & 0x3FC;
			bytes[i] = (uint8_t)(out >> 2);
			error = sum - out;
		}
		newSize = count;
		put32(fmt + 12, rate >> 1);
	}
	else
	{
		return;
	}

	// The header: the data's size, the file's, the block and the bytes a
	// second; the loop points scaled; what follows the data moved down.
	uint32_t removed = ((size + 1) & ~1u) - ((newSize + 1) & ~1u);
	uint32_t newBits = u16(fmt + 22);
	uint32_t channels = u16(fmt + 10);
	put32(data + 4, newSize);
	put32(4, u32(4) - removed);
	put16(fmt + 20, (newBits >> 3) * channels);
	put32(fmt + 16, u32(fmt + 12) * u16(fmt + 20));
	if (smpl && smpl + 44 <= wav.size())
	{
		uint32_t loops = u32(smpl + 8 + 28);
		uint32_t scale = newBits * size / newSize;
		for (uint32_t i = 0; i < loops && smpl + 8 + 36 + i * 24 + 16 <= wav.size(); i++)
		{
			size_t loop = smpl + 8 + 36 + i * 24;
			put32(loop + 8, bits * u32(loop + 8) / scale);
			put32(loop + 12, bits * u32(loop + 12) / scale);
		}
	}
	size_t dataEnd = data + 8 + size;
	if (removed && dataEnd <= wav.size())
		wav.erase(wav.begin() + (dataEnd - removed), wav.begin() + dataEnd);
}

// The sound's samples, decoded from its data, and what the decoder says of
// them.
Array<float> USound::Decode()
{
	std::unique_ptr<AudioSource> source;

	if (Format == "wav")
	{
		if (engine && engine->audiodev && engine->audiodev->LowSoundQuality)
		{
			Array<uint8_t> lowered = Data;
			LowerSoundQuality(lowered);
			source = AudioSource::CreateWav(lowered);
		}
		else
		{
			source = AudioSource::CreateWav(Data);
		}
	}
	else if (Format == "mp3" || Format == "mp2")
	{
		source = AudioSource::CreateMp3(Data);
	}
	else
	{
		Exception::Throw("Unsupported sound format: " + Format.ToString());
	}

	// GetSamples counts frames, ReadSamples samples: a stereo sound's are
	// twice its frames
	Array<float> decoded;
	#define ALIGN(x, a) ((x & ~(a-1)) + a)
	channels = source->GetChannels();
	// A count past 256 M floats (a gigabyte) is no sound of the game's
	if ((size_t)source->GetSamples() * channels > ((size_t)1 << 28))
		Exception::Throw("Sound too large");
	decoded.resize(ALIGN((size_t)source->GetSamples() * channels, 4));
	decoded.resize(ALIGN(source->ReadSamples(decoded.data(), decoded.size()), 4));

	frequency = source->GetFrequency();
	duration = decoded.size() / (float)channels / (float)frequency;

	loopInfo.Looped = source->bIsLooped;
	loopInfo.LoopStart = source->loopStart;
	loopInfo.LoopEnd = source->loopEnd;
	eightBit = source->eightBit;
	frames = (uint64_t)std::max(source->GetSamples(), 0);
	return decoded;
}

// Decoded once, for the audio device, whose mixer keeps a 16-bit copy of its
// own (GalaxySample): the samples are not kept here.
void USound::GetSound()
{
	if (decoded)
		return;
	decoded = true;

	samples = Decode();
	if (engine && engine->audiodev && engine->audiodev->GetDevice())
		engine->audiodev->GetDevice()->AddSound(this);
	samples = Array<float>();
}

float USound::GetDuration()
{
	GetSound();

	return duration;
}

float USound::GetPeriod()
{
	if (Data.size() < 12 || memcmp(Data.data(), "RIFF", 4) != 0 || memcmp(Data.data() + 8, "WAVE", 4) != 0)
		return 0.0f;
	auto u16 = [&](size_t at) { return (uint32_t)(Data[at] | (Data[at + 1] << 8)); };
	auto u32 = [&](size_t at) { return u16(at) | (u16(at + 2) << 16); };
	uint32_t channels = 0, rate = 0, bits = 0, bytes = 0;
	bool format = false, data = false;
	for (size_t at = 12; at + 8 <= Data.size(); at += ((u32(at + 4) + 1) & ~(size_t)1) + 8)
	{
		if (!format && memcmp(&Data[at], "fmt ", 4) == 0 && at + 24 <= Data.size())
		{
			channels = u16(at + 10);
			rate = u32(at + 12);
			bits = u16(at + 22);
			format = true;
		}
		else if (!data && memcmp(&Data[at], "data", 4) == 0)
		{
			bytes = u32(at + 4);
			data = true;
		}
	}
	uint32_t perSecond = bits * channels * rate;
	if (!format || !data || perSecond == 0)
		return 0.0f;
	return (float)((double)bytes * 8.0 / (double)(int32_t)perSecond);
}

int USound::GetChannels()
{
	GetSound();

	return channels;
}

float USound::GetHeaderDuration()
{
	// A RIFF's canonical header: the data chunk's size at byte 40 over the
	// format chunk's bytes a second at byte 28, read where they lie, as the
	// original reads them
	if (Data.size() >= 44 && memcmp(Data.data(), "RIFF", 4) == 0)
	{
		auto u32 = [&](size_t at) { return (uint32_t)(Data[at] | (Data[at + 1] << 8) | (Data[at + 2] << 16) | ((uint32_t)Data[at + 3] << 24)); };
		return (float)((double)u32(40) / (double)(int32_t)u32(28));
	}

	// An MPEG frame header, read as the original reads it: the sync, the
	// version and the layer picking the column of the bit rate table, the
	// bit rate index its row; a reserved layer or version column 0; a free
	// or bad bit rate index -1, a negative length
	if (Data.size() >= 4 && Data[0] == 0xFF && (Data[1] & 0xE0) == 0xE0)
	{
		static const int32_t kbps[16][5] =
		{
			{ -1, -1, -1, -1, -1 },
			{ 32, 32, 32, 32, 8 },
			{ 64, 48, 40, 48, 16 },
			{ 96, 56, 48, 56, 24 },
			{ 128, 64, 56, 64, 32 },
			{ 160, 80, 64, 80, 40 },
			{ 192, 96, 80, 96, 48 },
			{ 224, 112, 96, 112, 56 },
			{ 256, 128, 112, 128, 64 },
			{ 288, 160, 128, 144, 80 },
			{ 320, 192, 160, 160, 96 },
			{ 352, 224, 192, 176, 112 },
			{ 384, 256, 224, 192, 128 },
			{ 416, 320, 256, 224, 144 },
			{ 448, 384, 320, 256, 160 },
			{ -1, -1, -1, -1, -1 },
		};
		int version = (Data[1] >> 3) & 3; // 3 MPEG 1, 2 MPEG 2, 0 MPEG 2.5, 1 reserved
		int layer = (Data[1] >> 1) & 3;   // 3 layer I, 2 layer II, 1 layer III, 0 reserved
		int column = 0;
		if (version == 3)
			column = layer == 2 ? 1 : layer == 1 ? 2 : 0;
		else if (version != 1)
			column = layer == 3 ? 3 : layer == 0 ? 0 : 4;
		int row = Data[2] >> 4;
		return (float)((double)(int32_t)(8 * (uint32_t)Data.size()) / (double)(1000 * kbps[row][column]));
	}

	LogMessage("Unknown sound format in " + Name.ToString() + ": no length");
	return 0.0f;
}

void USound::OnGCDestroy()
{
	if (engine && engine->audiodev && engine->audiodev->GetDevice())
		engine->audiodev->GetDevice()->RemoveSound(this);
	UObject::OnGCDestroy();
}
