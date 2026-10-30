#pragma once

#include "Packages/Core/UObject.h"

class AudioLoopInfo
{
public:
	bool Looped = false;
	uint64_t LoopStart = 0;
	uint64_t LoopEnd = 0;
};

class AudioFrequency
{
public:
	AudioFrequency(int frequency) : frequency(frequency) {}
	int frequency = 0;
};

class USound : public UObject
{
public:
	using UObject::UObject;

	void Load(ObjectStream* stream) override;
	void Save(PackageStreamWriter* stream) override;

	void GetSound();
	float GetDuration();
	// The length the original's DConversation::GetSoundLength reads from the
	// header, without decoding (ConSys.dll 0x10010460): a WAV's data bytes
	// over its bytes a second, an MP3's bits over its first frame's bit
	// rate; anything else logged and 0.
	float GetHeaderDuration();

	// Gone from the audio device's buffers and mixer.
	void OnGCDestroy() override;
	int GetChannels();

	NameString Format;
	Array<uint8_t> Data;

	// The decoded samples, only while GetSound hands them to the device.
	Array<float> samples;
	bool decoded = false;
	float duration = 0.0f;
	int frequency = 0;
	int channels = 0;
	void* handle = nullptr;
	AudioLoopInfo loopInfo;

private:
	Array<float> Decode();
};
