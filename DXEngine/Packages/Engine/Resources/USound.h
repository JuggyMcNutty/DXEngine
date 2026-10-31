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
	// The original's USound::GetDuration (Engine.dll 0x1034ce50), which
	// Actor.GetSoundDuration gives: its FSoundData::GetPeriod (0x1036f7a0),
	// a WAV's data bytes times 8 over its bits a sample, channels and rate,
	// read from its first format and data chunks; 0 for anything else, an
	// MP3 among them.
	float GetPeriod();
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
	// A WAV of 8-bit samples, and its frames: Galaxy's lip sync reads such
	// a sound from its start (UDXEngineAudioDevice::UpdateLipSync)
	bool eightBit = false;
	uint64_t frames = 0;

private:
	Array<float> Decode();
};
