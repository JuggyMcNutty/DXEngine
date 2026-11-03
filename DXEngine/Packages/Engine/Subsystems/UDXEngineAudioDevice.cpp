
#include "Precomp.h"
#include "UDXEngineAudioDevice.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include "Package/IniProperty.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/Pawn/UPawn.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Info/UZoneInfo.h"
#include "Packages/Engine/Resources/UMusic.h"
#include "Packages/Engine/Resources/USound.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Collision/BottomLevel/TraceRayModel.h"
#include "Light/LightmapBuilder.h"
#include "kissfft/kiss_fftr.h"

static float square(float x) { return x * x; }

// The channel count's name: Galaxy's EffectsChannels for Deus Ex, which its
// Sound menu reads and writes (galaxy-dll.md, Settings); Channels for other
// games.
static const char* ChannelsKey() { return "EffectsChannels"; }

std::string UDXEngineAudioDevice::GetPropertyAsString(const NameString& propertyName) const
{
	if (propertyName == "Class")
		return "class'" + Class + "'";
	else if (propertyName == "UseFilter")
		return IniPropertyConverter<bool>::ToString(UseFilter);
	else if (propertyName == "UseSurround")
		return IniPropertyConverter<bool>::ToString(UseSurround);
	else if (propertyName == "UseStereo")
		return IniPropertyConverter<bool>::ToString(UseStereo);
	else if (propertyName == "UseCDMusic")
		return IniPropertyConverter<bool>::ToString(UseCDMusic);
	else if (propertyName == "UseDigitalMusic")
		return IniPropertyConverter<bool>::ToString(UseDigitalMusic);
	else if (propertyName == "UseSpatial")
		return IniPropertyConverter<bool>::ToString(UseSpatial);
	else if (propertyName == "UseReverb")
		return IniPropertyConverter<bool>::ToString(UseReverb);
	else if (propertyName == "Use3dHardware")
		return IniPropertyConverter<bool>::ToString(Use3dHardware);
	else if (propertyName == "LowSoundQuality")
		return IniPropertyConverter<bool>::ToString(LowSoundQuality);
	else if (propertyName == "ReverseStereo")
		return IniPropertyConverter<bool>::ToString(ReverseStereo);
	else if (propertyName == "Latency")
		return IniPropertyConverter<int>::ToString(Latency);
	else if (propertyName == "OutputRate")
		return IniPropertyConverter<AudioFrequency>::ToString(OutputRate);
	else if (propertyName == ChannelsKey())
		return IniPropertyConverter<int>::ToString(Channels);
	else if (propertyName == "MusicVolume")
		return IniPropertyConverter<uint8_t>::ToString(MusicVolumeSetting);
	else if (propertyName == "SoundVolume")
		return IniPropertyConverter<uint8_t>::ToString(SoundVolumeSetting);
	else if (propertyName == "SpeechVolume")
		return IniPropertyConverter<uint8_t>::ToString(SpeechVolumeSetting);
	else if (propertyName == "AmbientFactor")
		return IniPropertyConverter<float>::ToString(AmbientFactor);
	else if (propertyName == "DopplerSpeed")
		return IniPropertyConverter<float>::ToString(DopplerSpeed);

	LogMessage("Queried unknown property for DXEngineAudioDevice: " + propertyName.ToString());
	return {};
}

void UDXEngineAudioDevice::SetPropertyFromString(const NameString& propertyName, const std::string& value)
{
	if (propertyName == "UseFilter")
		UseFilter = IniPropertyConverter<bool>::FromString(value);
	else if (propertyName == "UseSurround")
		UseSurround = IniPropertyConverter<bool>::FromString(value);
	else if (propertyName == "UseStereo")
		UseStereo = IniPropertyConverter<bool>::FromString(value);
	else if (propertyName == "UseCDMusic")
		UseCDMusic = IniPropertyConverter<bool>::FromString(value);
	else if (propertyName == "UseDigitalMusic")
		UseDigitalMusic = IniPropertyConverter<bool>::FromString(value);
	else if (propertyName == "UseSpatial")
		UseSpatial = IniPropertyConverter<bool>::FromString(value);
	else if (propertyName == "UseReverb")
		UseReverb = IniPropertyConverter<bool>::FromString(value);
	else if (propertyName == "Use3dHardware")
		Use3dHardware = IniPropertyConverter<bool>::FromString(value);
	else if (propertyName == "LowSoundQuality")
		LowSoundQuality = IniPropertyConverter<bool>::FromString(value);
	else if (propertyName == "ReverseStereo")
		ReverseStereo = IniPropertyConverter<bool>::FromString(value);
	else if (propertyName == "Latency")
		Latency = std::clamp(IniPropertyConverter<int>::FromString(value), 10, 250);
	else if (propertyName == "OutputRate")
		OutputRate = IniPropertyConverter<AudioFrequency>::FromString(value);
	else if (propertyName == ChannelsKey())
	{
		Channels = IniPropertyConverter<int>::FromString(value);
		// Galaxy's channel loops read EffectsChannels as it stands: a SET takes
		// the channels in use at once (galaxy-dll.md, Settings).
		if (m_Viewport)
			SetChannelCount();
	}
	else if (propertyName == "MusicVolume")
		MusicVolume = MusicVolumeSetting = IniPropertyConverter<uint8_t>::FromString(value);
	else if (propertyName == "SoundVolume")
		SoundVolume = SoundVolumeSetting = IniPropertyConverter<uint8_t>::FromString(value);
	else if (propertyName == "SpeechVolume")
		SpeechVolume = SpeechVolumeSetting = IniPropertyConverter<uint8_t>::FromString(value);
	else if (propertyName == "AmbientFactor")
	{
		AmbientFactor = IniPropertyConverter<float>::FromString(value);
		AmbientFactorInUse = std::clamp(AmbientFactor, 0.0f, 10.0f);
	}
	else if (propertyName == "DopplerSpeed")
	{
		DopplerSpeed = IniPropertyConverter<float>::FromString(value);
		DopplerSpeedInUse = std::clamp(DopplerSpeed, 1.0f, 100000.0f);
	}
	else
		LogMessage("Setting unknown property for DXEngineAudioDevice: " + propertyName.ToString());

	engine->packages->SetIniValue("System", Class, propertyName, value);
}

void UDXEngineAudioDevice::LoadProperties(const NameString& from)
{
	NameString name_from = from;

	if (from == "")
		name_from = NameString(Class);

	UseFilter = IniPropertyConverter<bool>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, "UseFilter", UseFilter);
	UseSurround = IniPropertyConverter<bool>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, "UseSurround", UseSurround);
	UseStereo = IniPropertyConverter<bool>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, "UseStereo", UseStereo);
	UseCDMusic = IniPropertyConverter<bool>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, "UseCDMusic", UseCDMusic);
	UseDigitalMusic = IniPropertyConverter<bool>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, "UseDigitalMusic", UseDigitalMusic);
	UseSpatial = IniPropertyConverter<bool>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, "UseSpatial", UseSpatial);
	UseReverb = IniPropertyConverter<bool>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, "UseReverb", UseReverb);
	Use3dHardware = IniPropertyConverter<bool>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, "Use3dHardware", Use3dHardware);
	LowSoundQuality = IniPropertyConverter<bool>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, "LowSoundQuality", LowSoundQuality);
	ReverseStereo = IniPropertyConverter<bool>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, "ReverseStereo", ReverseStereo);
	Latency = IniPropertyConverter<int>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, "Latency", Latency);
	// Held to 10 to 250 ms, as Galaxy holds it (galaxy-dll.md)
	Latency = std::clamp(Latency, 10, 250);
	OutputRate = IniPropertyConverter<AudioFrequency>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, "OutputRate", OutputRate);
	// Held to 8,000 to 48,000 Hz, as Galaxy holds its rate: 0 divided the
	// mixer's step by zero, a negative rate capped every step, and one past
	// 48,188 wrapped a reverb delay
	OutputRate.frequency = std::clamp(OutputRate.frequency, 8000, 48000);
	Channels = IniPropertyConverter<int>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, ChannelsKey(), Channels);
	MusicVolume = IniPropertyConverter<uint8_t>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, "MusicVolume", MusicVolume);
	SoundVolume = IniPropertyConverter<uint8_t>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, "SoundVolume", SoundVolume);
	SpeechVolume = IniPropertyConverter<uint8_t>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, "SpeechVolume", SpeechVolume);
	MusicVolumeSetting = MusicVolume;
	SoundVolumeSetting = SoundVolume;
	SpeechVolumeSetting = SpeechVolume;
	AmbientFactor = IniPropertyConverter<float>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, "AmbientFactor", AmbientFactor);
	DopplerSpeed = IniPropertyConverter<float>::FromIniFile(*engine->packages->GetIniFile("System"), name_from, "DopplerSpeed", DopplerSpeed);
	AmbientFactorInUse = AmbientFactor;
	DopplerSpeedInUse = DopplerSpeed;
}

void UDXEngineAudioDevice::SaveConfig()
{
	engine->packages->SetIniValue("System", Class, "UseFilter", IniPropertyConverter<bool>::ToString(UseFilter));
	engine->packages->SetIniValue("System", Class, "UseSurround", IniPropertyConverter<bool>::ToString(UseSurround));
	engine->packages->SetIniValue("System", Class, "UseStereo", IniPropertyConverter<bool>::ToString(UseStereo));
	engine->packages->SetIniValue("System", Class, "UseCDMusic", IniPropertyConverter<bool>::ToString(UseCDMusic));
	engine->packages->SetIniValue("System", Class, "UseDigitalMusic", IniPropertyConverter<bool>::ToString(UseDigitalMusic));
	engine->packages->SetIniValue("System", Class, "UseSpatial", IniPropertyConverter<bool>::ToString(UseSpatial));
	engine->packages->SetIniValue("System", Class, "UseReverb", IniPropertyConverter<bool>::ToString(UseReverb));
	engine->packages->SetIniValue("System", Class, "Use3dHardware", IniPropertyConverter<bool>::ToString(Use3dHardware));
	engine->packages->SetIniValue("System", Class, "LowSoundQuality", IniPropertyConverter<bool>::ToString(LowSoundQuality));
	engine->packages->SetIniValue("System", Class, "ReverseStereo", IniPropertyConverter<bool>::ToString(ReverseStereo));
	engine->packages->SetIniValue("System", Class, "Latency", IniPropertyConverter<int>::ToString(Latency));
	engine->packages->SetIniValue("System", Class, "OutputRate", IniPropertyConverter<AudioFrequency>::ToString(OutputRate));
	engine->packages->SetIniValue("System", Class, ChannelsKey(), IniPropertyConverter<int>::ToString(Channels));
	engine->packages->SetIniValue("System", Class, "MusicVolume", IniPropertyConverter<uint8_t>::ToString(MusicVolumeSetting));
	engine->packages->SetIniValue("System", Class, "SoundVolume", IniPropertyConverter<uint8_t>::ToString(SoundVolumeSetting));
	engine->packages->SetIniValue("System", Class, "SpeechVolume", IniPropertyConverter<uint8_t>::ToString(SpeechVolumeSetting));
	engine->packages->SetIniValue("System", Class, "AmbientFactor", IniPropertyConverter<float>::ToString(AmbientFactor));
	engine->packages->SetIniValue("System", Class, "DopplerSpeed", IniPropertyConverter<float>::ToString(DopplerSpeed));
}

void UDXEngineAudioDevice::InitDevice()
{
	// The rate is OutputRate's, the mix-ahead Latency's and the channels
	// UseStereo's; the rest is fixed, as the original's Galaxy has no key for
	// a device, its voices or its music buffers.
	m_Device = AudioDevice::Create(OutputRate.frequency, 256, 16, 256, Latency, UseStereo);
	LogMessage("Audio device initialized");
}

void UDXEngineAudioDevice::ShutdownDevice()
{
	m_Device.reset();
	LogMessage("Audio device has been shut down");
}

void UDXEngineAudioDevice::SetViewport(UViewport* InViewport)
{
	// A joining client's viewport has no player until the server's arrives;
	// it is taken then, its music with it.
	if (InViewport && !InViewport->Actor())
		return;

	if (m_Viewport != InViewport)
	{
		StopSounds();

		m_Viewport = InViewport;

		if (m_Viewport)
		{
			if (m_Viewport->Actor()->Song() && m_Viewport->Actor()->Transition() == MTRAN_None)
				m_Viewport->Actor()->Transition() = MTRAN_Instant;

			SetChannelCount();
		}
	}
}

// The channels in use: the setting held to the device's channels, as Galaxy's
// PostEditChange holds EffectsChannels to its 32 channel records; the setting
// itself stays as set, as GET and the ini give it (galaxy-dll.md, Settings).
// The sounds on channels past the count stop.
void UDXEngineAudioDevice::SetChannelCount()
{
	size_t count = (size_t)std::clamp(Channels, 0, m_Device->GetTotalChannels());
	for (size_t i = count; i < PlayingSounds.size(); i++)
		StopSound(i);
	PlayingSounds.resize(count);
}

void UDXEngineAudioDevice::Update()
{
	// The update's own time step, 0 to 1 s, as Galaxy's (galaxy-dll.md, Each
	// frame); the obstruction fade runs on it.
	auto now = std::chrono::steady_clock::now();
	float timeStep = std::clamp(std::chrono::duration<float>(now - m_LastUpdateTime).count(), 0.0f, 1.0f);
	m_LastUpdateTime = now;

	StartAmbience();
	UpdateAmbience();
	UpdateSounds(timeStep);
	UpdateMusic(timeStep);
	UpdateReverb();

	float musicFade = 1.0f;
	if (m_MusicTransition && m_MusicFadeLength > 0.0f)
		musicFade = std::max(m_MusicFadeLeft, 0.0f) / m_MusicFadeLength;
	m_Device->SetMusicVolume(MusicVolume / 255.0f * musicFade);
	// Galaxy's sample volume is the louder of the two sliders, squared in its
	// mixer; each sound's own share of the sliders is its voice's
	// (GalaxyVoice, galaxy-dll.md, Volume).
	m_Device->SetSoundVolume(SoundVolume / 255.0f);
	m_Device->SetSpeechVolume(SpeechVolume / 255.0f);
	m_Device->Update();
}

void UDXEngineAudioDevice::StartAmbience()
{
	if (!m_Viewport || !m_Viewport->Actor())
		return;

	bool Realtime = m_Viewport->IsRealtime() && m_Viewport->Actor()->Level()->Pauser().empty();
	if (Realtime)
	{
		UActor* ViewActor = m_Viewport->Actor()->ViewTarget() ? m_Viewport->Actor()->ViewTarget() : m_Viewport->Actor();
		for (UActor* Actor : m_Viewport->Actor()->XLevel()->Actors)
		{
			if (Actor && Actor->AmbientSound() && dist_squared(ViewActor->Location(), Actor->Location()) <= square(Actor->WorldSoundRadius()))
			{
				int Id = SoundId(Actor, SLOT_Ambient);
				bool foundSound = false;
				for (size_t j = 0; j < PlayingSounds.size(); j++)
				{
					if (PlayingSounds[j].Id == Id)
					{
						foundSound = true;
						break;
					}
				}
				if (!foundSound)
					PlaySound(Actor, Id, Actor->AmbientSound(), Actor->Location(), AmbientFactorInUse * Actor->SoundVolume() / 255.0f, Actor->WorldSoundRadius(), Actor->SoundPitch() / 64.0f, false);
			}
		}
	}
}

void UDXEngineAudioDevice::UpdateAmbience()
{
	if (!m_Viewport || !m_Viewport->Actor())
		return;

	UActor* ViewActor = m_Viewport->Actor()->ViewTarget() ? m_Viewport->Actor()->ViewTarget() : m_Viewport->Actor();
	bool Realtime = m_Viewport->IsRealtime() && m_Viewport->Actor()->Level()->Pauser().empty();
	for (size_t i = 0; i < PlayingSounds.size(); i++)
	{
		PlayingSound& Playing = PlayingSounds[i];
		if ((Playing.Id & 14) == SLOT_Ambient * 2)
		{
			if (Playing.Actor->bDeleteMe() || dist_squared(ViewActor->Location(), Playing.Actor->Location()) > square(Playing.Actor->WorldSoundRadius()) || Playing.Actor->AmbientSound() != Playing.Sound || !Realtime)
			{
				// Ambient sound went out of range
				StopSound(i);
			}
			else
			{
				// Update basic sound properties
				Playing.Volume = 2.0f * (AmbientFactorInUse * Playing.Actor->SoundVolume() / 255.0f);
				Playing.Radius = Playing.Actor->WorldSoundRadius();
				Playing.Pitch = Playing.Actor->SoundPitch() / 64.0f;

				// An actor with a light has its sound follow the light: the volume
				// times LightBrightness / 255 and the light's momentary pulse,
				// blink, flicker or strobe -- the renderer's GlobalLighting of a
				// steady 1, the last three as the lights' last step left them
				// (LightSystem::Tick) --, then at most 1 (galaxy-dll.md, Each
				// frame).
				if (Playing.Actor->LightType() != LT_None)
					Playing.Volume = std::min(Playing.Volume * (Playing.Actor->LightBrightness() / 255.0f) * LightmapBuilder::GlobalLighting(Playing.Actor, 1.0f, nullptr), 1.0f);

				// Deus Ex's Doppler is the ambient sound's alone: the pitch times
				// 1 - the actor's speed away from the view target / DopplerSpeed,
				// kept to 0.5-2 (galaxy-dll.md, Each frame). AL's own Doppler is
				// off for Deus Ex, so nothing else shifts.
				if (DopplerSpeedInUse > 0.0f)
				{
					vec3 away = Playing.Actor->Location() - ViewActor->Location();
					float distance = length(away);
					if (distance > 0.0f)
					{
						float speedAway = dot(Playing.Actor->Velocity(), away / distance);
						Playing.Pitch *= std::clamp(1.0f - speedAway / DopplerSpeedInUse, 0.5f, 2.0f);
					}
				}
			}
		}
	}
}

// A Pawn class without the lip-sync properties leaves their offsets unset: then no lip
// sync, and nothing else changes.
static bool HasLipSync()
{
	return PropOffsets_Pawn.nextPhoneme.DataOffset != ~(size_t)0;
}

// Galaxy's input to its lip sync's FFT, one for every voice (Galaxy.dll
// 0x1067588c): each slot a sample over 32,768, times the slot over 1,024 --
// the rising half of a triangle (0x10609180). A read sets the slots it reads
// and leaves the rest as the last read left them.
static float LipSyncInput[1024];

static void LipSyncSet(int slot, int16_t sample)
{
	LipSyncInput[slot] = (float)(sample / 32768.0 * (slot / 1024.0));
}

// Galaxy's mouth shape from its FFT's input (galaxy-dll.md, Lip sync;
// Galaxy.dll 0x10609310, 0x10608d70): a 1,024-point FFT, of whose bins Galaxy
// takes the size of the real part alone. The largest of the first 512, DC's
// among them, the first of equals: X at 5 or less, else the letter of its
// frequency, T above 2,000 Hz, F above 1,500, A above 600, O above 400, U
// above 250, E at 250 or below. Galaxy's M, tested above 1,000 Hz inside the
// branch for 250 or below, never comes.
static char LipSyncShape(int rate)
{
	constexpr int Size = 1024;
	static std::unique_ptr<kiss_fftr_state, void(*)(void*)> fft(kiss_fftr_alloc(Size, 0, nullptr, nullptr), free);
	if (!fft)
		return 'X';

	kiss_fft_cpx bins[Size / 2 + 1];
	kiss_fftr(fft.get(), LipSyncInput, bins);

	float largest = 0.0f;
	int strongest = -1;
	for (int i = 0; i < Size / 2; i++)
	{
		float size = std::fabs(bins[i].r);
		if (strongest < 0 || largest < size)
		{
			largest = size;
			strongest = i;
		}
	}
	if (largest <= 5.0f)
		return 'X';

	float hz = strongest * (float)rate * (1.0f / Size);
	if (hz > 2000.0f)
		return 'T';
	if (hz > 1500.0f)
		return 'F';
	if (hz > 600.0f)
		return 'A';
	if (hz > 400.0f)
		return 'O';
	if (hz > 250.0f)
		return 'U';
	return 'E';
}

// Galaxy's lip sync (galaxy-dll.md, Lip sync; its Update, 0x1060a09a): for
// speech from a pawn whose script has set bIsSpeaking, a voice that plays,
// once its time played reaches the next tenth of a second -- the first at
// once, at the update after the voice starts --, the shape written to the
// pawn's nextPhoneme and the next a tenth on, so one held back runs a shape
// an update until it catches up. An MP3 plays from a ring of 64 KB Galaxy
// decodes it into, 32,768 frames, read from the voice's place in it -- its
// 4,096 frames of silence first among them -- up to the ring's last frame;
// any other sound 0.1 s past its time played, but an 8-bit one: its bytes
// from its start, wherever the voice plays, two to a slot, the second kept,
// so its first 512 slots alone, each 0 where the place read from and the
// byte pass the data's end (0x10609180).
void UDXEngineAudioDevice::UpdateLipSync(PlayingSound& Playing, int channel)
{
	if (!HasLipSync() || !Playing.Actor || !Playing.IsActive || !m_Device->IsPlaying(channel))
		return;
	if ((Playing.Id & 14) != SLOT_Talk * 2)
		return;

	UPawn* pawn = UObject::TryCast<UPawn>(Playing.Actor);
	if (!pawn || Playing.PlayTime < Playing.NextLipSync)
		return;

	// bIsSpeaking is the script's alone: ConPlay sets it around a line and
	// LipSynch's bWasSpeaking branch closes the mouth when it drops, so the
	// original's Galaxy only reads it. Writing it here moved mouths on barks
	// the script never opened.
	if (!pawn->bIsSpeaking())
		return;

	constexpr int Size = 1024, Ring = 0x8000;
	int16_t frames[Size] = {};
	int rate = Playing.Sound->frequency;
	if (Playing.Sound->Format == "mp3" || Playing.Sound->Format == "mp2")
	{
		uint64_t frame = 0;
		if (!m_Device->GetMixedSoundPosition(channel, frame))
			return;
		m_Device->ReadMixedSound(channel, frame, frames, std::min(Size, (int)(Ring - 1 - frame % Ring)));
		for (int i = 0; i < Size; i++)
			LipSyncSet(i, frames[i]);
	}
	else
	{
		double played = (double)(int64_t)(rate * (double)Playing.PlayTime);
		uint64_t from = (uint64_t)(int64_t)(played + rate * 0.1);
		if (Playing.Sound->eightBit)
		{
			m_Device->ReadMixedSound(channel, 0, frames, Size);
			for (int i = 0; i < Size / 2; i++)
				LipSyncSet(i, from + 2 * i + 1 >= Playing.Sound->frames ? 0 : frames[2 * i + 1]);
		}
		else
		{
			m_Device->ReadMixedSound(channel, from, frames, Size);
			for (int i = 0; i < Size; i++)
				LipSyncSet(i, frames[i]);
		}
	}

	pawn->nextPhoneme() = ScriptString(1, (char16_t)(uint8_t)LipSyncShape(rate));
	Playing.NextLipSync += 0.1f;
}

void UDXEngineAudioDevice::UpdateSounds(float timeStep)
{
	if (!m_Viewport || !m_Viewport->Actor())
		return;

	UActor* ViewActor = m_Viewport->Actor()->ViewTarget() ? m_Viewport->Actor()->ViewTarget() : m_Viewport->Actor();
	for (size_t i = 0; i < PlayingSounds.size(); i++)
	{
		PlayingSound& Playing = PlayingSounds[i];

		if (Playing.Id != 0)
		{
			Playing.PlayTime += timeStep;

			// Update positioning from actor, if available
			if (Playing.Actor)
				Playing.Location = Playing.Actor->Location();

			// Update the priority
			Playing.Priority = SoundPriority(m_Viewport, Playing.Location, Playing.Volume, Playing.Radius);

			UpdateLipSync(Playing, (int)i);

			// Deus Ex: a wall between the player's eyes and the sound fades it
			// toward a third of its volume over half a second, and back as it
			// clears (galaxy-dll.md, Sounds behind walls); then the voice as
			// Galaxy works it out, through Galaxy's mixer.
			UpdateObstruction(Playing, timeStep);
			int volume = 0, panning = 0;
			GalaxyVoice(Playing, volume, panning);
			if (Playing.IsActive)
			{
				if (m_Device->IsPlaying((int)i))
					m_Device->UpdateMixedSound((int)i, volume, panning, Playing.Pitch);
				else
					PlayingSounds[i] = {};
			}
			else
			{
				m_Device->PlayMixedSound((int)i, Playing.Sound, volume, panning, Playing.Pitch);
				Playing.IsActive = true;
			}
		}
	}
}

// A Deus Ex channel's voice, as Galaxy's Update works it out (galaxy-dll.md,
// Each frame): heard from the eyes the view is drawn from, the sound falls off
// as 1 - distance / radius; its angle off straight ahead, front and back
// alike, becomes a pan of at most seven-eighths to a side, nearer the middle
// within a tenth of its radius; and its volume is the script's times the
// fall-off, the obstruction and its share of the sliders -- speech the Speech
// slider over the Sound one when the Sound slider is the louder, the other
// sounds the other way round --, full at 32767 and never under 128, 1/256 of
// it. Galaxy's equal-sliders quirk (each sound scaled by the slider once
// more) is not carried, by choice (docs/NATIVES.md, sound).
void UDXEngineAudioDevice::GalaxyVoice(const PlayingSound& Playing, int& volume, int& panning)
{
	vec3 forward, right, up;
	Coords::Rotation(engine->CameraRotation).GetAxes(forward, right, up);
	vec3 offset = Playing.Location - engine->CameraLocation;
	double x = dot(offset, right);
	double z = dot(offset, forward);
	double distance = length(offset);

	double angle = std::atan2(x, std::abs(z));
	double nearby = Playing.Radius * 0.1;
	if (nearby * nearby > distance * distance)
		angle = distance / nearby * angle;
	panning = (int)std::clamp((int64_t)(angle * 28671.125 * 0.3183098861837907 + 16383.0), (int64_t)0, (int64_t)0x7fff);
	if (ReverseStereo)
		panning = 0x7fff - panning;
	if (z < 0.0 && UseSurround)
		panning = 49152;

	double falloff = std::clamp(1.0 - distance / Playing.Radius, 0.0, 1.0);
	double obstruction = std::max(1.0 - 2.0 * Playing.ObstructionTime, 0.33);
	double share = 1.0;
	bool speech = (Playing.Id & 14) == SLOT_Talk * 2;
	if (!speech && SoundVolume < SpeechVolume)
		share = (double)SoundVolume / SpeechVolume;
	else if (speech && SoundVolume > SpeechVolume)
		share = (double)SpeechVolume / SoundVolume;
	volume = (int)std::clamp((int64_t)(obstruction * falloff * share * Playing.Volume * 32767.0), (int64_t)0, (int64_t)0x7fff);
	// Then Galaxy's floor, on every voice it starts or updates (Galaxy.dll
	// 0x10609ee3): a sound gone out of its radius as it plays (an ambient
	// one stops there), one at a volume of 0 and one whose own slider is at
	// 0 while the other is not play on at 128, never silent; only both
	// sliders at 0 silence a voice, through the mixer's sample volume
	// (GalaxyMixer::SetMasterVolume).
	volume = std::max(volume, 128);
}

void UDXEngineAudioDevice::UpdateObstruction(PlayingSound& Playing, float timeStep)
{
	// Speech and a sound whose actor has gone run no line: their time only
	// shrinks, as Galaxy's (galaxy-dll.md, sounds behind walls), so a sound
	// muffled when its actor went fades back over up to half a second
	if (!Playing.Actor || (Playing.Id & 14) == SLOT_Talk * 2)
	{
		Playing.ObstructionTime = std::max(Playing.ObstructionTime - timeStep, 0.0f);
		return;
	}

	// The line runs from the player's own eyes -- the player's place and
	// EyeHeight, even when viewing through another actor -- to the sound's
	// actor, and only the level's BSP blocks it, as the original's
	// UModel::FastLineCheck has it: no actor does, but the movers where the
	// BSP filed them, so a closed door muffles (CollisionSystem, filed
	// movers).
	UActor* player = m_Viewport->Actor();
	vec3 eyes = player->Location();
	if (UPawn* pawn = UObject::TryCast<UPawn>(player))
		eyes.z += pawn->EyeHeight();

	bool blocked = false;
	dvec3 origin = to_dvec3(eyes);
	dvec3 direction = to_dvec3(Playing.Location) - origin;
	double tmax = length(direction);
	if (tmax > 0.01)
	{
		// The original's mixer asks FastLineCheck, the solid-space walk
		// (FastWalk, the hearing test's), down the BSP with the movers filed
		// into it: a solid brush muffles whatever faces the line crosses,
		// invisible or portal ones too, and a polygon with open space on
		// both sides does not.
		CollisionSystem& collision = player->XLevel()->Collision;
		blocked = !TraceRayModel::FastWalk(collision.LineNodes(), collision.LineRootOutside(), origin, to_dvec3(Playing.Location));
	}

	if (blocked)
		Playing.ObstructionTime = std::min(Playing.ObstructionTime + timeStep, 0.5f);
	else
		Playing.ObstructionTime = std::max(Playing.ObstructionTime - timeStep, 0.0f);
}

// Builds the source for a song; a tracker module starts at the given order.
static std::unique_ptr<AudioSource> CreateMusicSource(UMusic* song, int order)
{
	std::unique_ptr<AudioSource> source;
	if (song->Format == "mp3" || song->Format == "mp2")
		source = AudioSource::CreateMp3(song->Data);
	else if (song->Format == "ogg")
		source = AudioSource::CreateOgg(song->Data, true);
	else if (song->Format == "wav")
		source = AudioSource::CreateWav(song->Data);
	else
	{
		source = AudioSource::CreateMod(song->Data, true, 0);
		if (source && order > 0)
			source->SetOrder(order);
	}
	return source;
}

void UDXEngineAudioDevice::UpdateMusic(float timeStep)
{
	if (!m_Viewport || !m_Viewport->Actor())
		return;

	auto* player = m_Viewport->Actor();

	if (!m_MusicTransition && player->Transition() != MTRAN_None)
	{
		// The playing music fades out first: 1 s for MTRAN_Fade, 5 s for
		// MTRAN_SlowFade, 1/3 s for MTRAN_FastFade, at once for the others,
		// plus twice Latency (galaxy-dll.md, Music).
		float length = 0.0f;
		switch (player->Transition())
		{
		case MTRAN_Fade: length = 1.0f; break;
		case MTRAN_SlowFade: length = 5.0f; break;
		case MTRAN_FastFade: length = 1.0f / 3.0f; break;
		default: break;
		}
		if (length > 0.0f)
			length += 2.0f * Latency / 1000.0f;
		if (!CurrentSong)
			length = 0.0f;   // nothing plays, nothing to fade
		m_MusicTransition = true;
		m_MusicFadeLength = length;
		m_MusicFadeLeft = length;
	}

	if (m_MusicTransition)
	{
		m_MusicFadeLeft -= timeStep;
		if (m_MusicFadeLeft > 0.0f)
			return;

		// The fade is done: the player's Song starts at full volume at the
		// order SongSection -- a different song is loaded, the same one only
		// jumps -- and section 255 is silence (galaxy-dll.md, Music).
		UMusic* song = player->Song();
		int section = player->SongSection();
		if (!song || section == 255 || !UseDigitalMusic)
		{
			m_Device->PlayMusic({});
			CurrentSong = nullptr;
		}
		else if (song == CurrentSong)
		{
			m_Device->SetMusicOrder(section);
		}
		else
		{
			auto source = CreateMusicSource(song, section);
			m_Device->PlayMusic(std::move(source));
			CurrentSong = song;
		}
		CurrentSection = section;
		m_MusicTransition = false;
		m_MusicFadeLength = 0.0f;
		m_MusicFadeLeft = 0.0f;
		player->Transition() = MTRAN_None;
	}
	else if (CurrentSong)
	{
		// Where the music is: while no transition waits, each frame writes the
		// order playing back into the player's SongSection, so the ambient
		// track comes back where it was after a fight or a conversation
		// (galaxy-dll.md, Music).
		int order = m_Device->GetMusicOrder();
		if (order >= 0)
		{
			player->SongSection() = (uint8_t)order;
			CurrentSection = order;
		}
	}
}

void UDXEngineAudioDevice::UpdateReverb()
{
	if (!m_Viewport || !m_Viewport->Actor())
		return;

	// With UseReverb, the zone of the player's view target with bReverbZone
	// gives every sound its reverb -- MasterGain / 255 the volume, CutoffHz
	// the damping of highs, six echoes of Delay x 2 ms at Gain / 255 --
	// another zone gives none, and it is set again only when it changes
	// (galaxy-dll.md, Reverb).
	UActor* ViewActor = m_Viewport->Actor()->ViewTarget() ? m_Viewport->Actor()->ViewTarget() : m_Viewport->Actor();
	UZoneInfo* zone = ViewActor->Region().Zone;
	UZoneInfo* reverbZone = (UseReverb && zone && zone->bReverbZone()) ? zone : nullptr;
	if (reverbZone == m_ReverbZone)
		return;
	m_ReverbZone = reverbZone;

	if (!reverbZone)
	{
		m_Device->SetReverb(nullptr);
		return;
	}

	// Galaxy's values to the bit: its reverb takes an echo's delay in whole
	// samples.
	ReverbSettings settings;
	settings.masterGain = (float)(reverbZone->MasterGain() * 0.00392156862745098);
	settings.cutoffHz = (float)std::clamp(reverbZone->CutoffHz(), 0, 44100);
	for (int i = 0; i < 6; i++)
	{
		settings.delaySeconds[i] = (float)std::clamp(reverbZone->Delay()[i] * (double)0.002f, 0.001, 0.34);
		settings.gains[i] = (float)std::clamp(reverbZone->Gain()[i] * (double)(1.0f / 255.0f), 0.001, (double)0.999f);
	}
	m_Device->SetReverb(&settings);
}

bool UDXEngineAudioDevice::PlaySound(UActor* Actor, int Id, USound* Sound, vec3 Location, float Volume, float Radius, float Pitch, bool isTalk)
{
	// A radius of 0 or less is 800 in Deus Ex's Actor.PlaySound (Engine.dll
	// 0x103e1f60, engine-dll.md#sounds): a script that plays a sound with no
	// radius of its own is heard 800 units out, not the 1,500 DXEngine let
	// through -- and its priority, (1 - distance / radius) * volume, is worked
	// out on that.
	if (Radius <= 0.0)
		Radius = 800.0f;

	// Deus Ex plays the script's volume as Galaxy does: scaled only by fall-off,
	// obstruction and the sliders, at most full and at least 1/256 of it
	// (GalaxyVoice).

	if (!m_Viewport || !Sound)
		return false;

	// Allocate a new slot if requested
	if ((Id & 14) == 2 * SLOT_None)
		Id = 16 * --FreeSlot;

	float Priority = SoundPriority(m_Viewport, Location, Volume, Radius);

	// If already playing, stop it
	size_t Index = PlayingSounds.size();
	float BestPriority = Priority;
	for (size_t i = 0; i < PlayingSounds.size(); i++)
	{
		PlayingSound& Playing = PlayingSounds[i];
		if ((Playing.Id & ~1) == (Id & ~1))
		{
			// Skip if not interruptable.
			if (Id & 1)
				return 0;

			// Stop the sound.
			Index = i;
			break;
		}
		else if (Playing.Priority <= BestPriority)
		{
			Index = i;
			BestPriority = Playing.Priority;
		}
	}

	// If no sound, or its priority is overruled, stop it
	if (Index == PlayingSounds.size())
		return 0;

	Sound->GetSound();

	// Put the sound on the play-list
	StopSound(Index);
	PlayingSounds[Index] = PlayingSound(Actor, Id, Sound, Location, Volume, Radius, Pitch, Priority);

	return true;
}

void UDXEngineAudioDevice::ActorDestroyed(UActor* Actor)
{
	SoundSerials.erase(Actor);
	for (size_t i = 0; i < PlayingSounds.size(); i++)
	{
		if (PlayingSounds[i].Actor == Actor)
		{
			if ((PlayingSounds[i].Id & 14) == SLOT_Ambient * 2)
			{
				// Stop ambient sound when actor dies
				StopSound(i);
			}
			else
			{
				// Unbind regular sounds from actors
				PlayingSounds[i].Actor = nullptr;
			}
		}
	}
}

// A sound's ID packs the object that plays it with its slot -- the object's
// number times 16, plus the slot times 2, plus 1 with bNoOverride -- as the
// original packs the actor's object index (engine-dll.md#sounds). The ID is
// load-bearing: a sound replaces the one playing with the same ID, and
// StopSound stops by the ID alone. DXEngine's objects have no index, so each
// gets a number of its own the first time it plays a sound, dropped when the
// actor is destroyed or the level's sounds are all stopped, never given out
// twice. The low 24 bits of the object's address stood in for it before, and
// two objects 16 MB apart shared them: one's sound could replace or stop the
// other's.
int UDXEngineAudioDevice::SoundId(UObject* Object, int Slot)
{
	auto it = SoundSerials.find(Object);
	if (it == SoundSerials.end())
		it = SoundSerials.emplace(Object, NextSoundSerial++).first;
	return it->second * 16 + Slot * 2;
}

void UDXEngineAudioDevice::StopSound(UActor* Actor, int Id)
{
	// The original stops by ID alone: Actor.StopSound hands the ID to the
	// audio subsystem, which stops the channel with that ID (Galaxy's
	// StopSoundId, engine-dll.md#sounds). The ID encodes the actor that played
	// it, but the caller need not be that actor -- ConPlay's StopSpeech calls
	// the *player's* StopSound with the *speaker's* ID to cut a line short,
	// which a match on the actor too would never find, the speech playing on.
	// Actor is kept for the interface and ignored.
	for (size_t i = 0; i < PlayingSounds.size(); i++)
	{
		if (PlayingSounds[i].Id == Id)
		{
			StopSound(i);
			break;
		}
	}
}

void UDXEngineAudioDevice::StopSound(size_t index)
{
	PlayingSound& Playing = PlayingSounds[index];

	if (Playing.IsActive)
	{
		m_Device->StopSound((int)index);
		Playing.IsActive = false;
	}

	// The mouth is left as it is: the script's LipSynch closes it when ConPlay
	// drops bIsSpeaking, as the original does.
	PlayingSounds[index] = {};
}

void UDXEngineAudioDevice::StopEffects()
{
	for (size_t i = 0; i < PlayingSounds.size(); i++)
		StopSound(i);
	SoundSerials.clear();
	// Set again from the next level's zones.
	m_ReverbZone = nullptr;
}

void UDXEngineAudioDevice::StopSounds()
{
	StopEffects();

	// The song goes with the music: a level of the same song starts it
	// again, where it would only jump the order of music stopped.
	m_Device->PlayMusic(nullptr);
	CurrentSong = nullptr;
	m_Viewport = nullptr;
}

void UDXEngineAudioDevice::AddStats(Array<std::string>& lines)
{
	const int bufsize = 1024;
	char buffer[bufsize];
	int index = 0;
	for (const PlayingSound& sound : PlayingSounds)
	{
		if (sound.IsActive)
		{
			std::snprintf(buffer, bufsize - 1, "Channel %2i: Vol: %05.2f %s", index, sound.Volume, sound.Sound->Name.ToString().c_str());
		}
		else
		{
			if (index >= 10)
				std::snprintf(buffer, bufsize - 1, "Channel %i:  None", index);
			else
				std::snprintf(buffer, bufsize - 1, "Channel %i: None", index);
		}
		buffer[bufsize - 1] = 0;
		lines.push_back(buffer);
		index++;
	}
}

float UDXEngineAudioDevice::SoundPriority(UViewport* Viewport, vec3 Location, float Volume, float Radius)
{
	UActor* target = Viewport->Actor();
	if (target && Viewport->Actor()->ViewTarget())
		target = Viewport->Actor()->ViewTarget();
	if (!target)
		return 0.0f;
	float priority = Volume * (1.0f - length(Location - target->Location()) / Radius);
	// Galaxy lets a sound beyond its radius go negative: it never beats an empty
	// channel's 0, so it is dropped even when one is free, and a playing sound
	// that left its radius is the first stolen (galaxy-dll.md, Playing a sound).
	return priority;
}

void UDXEngineAudioDevice::Mark(GCMarker& marker)
{
	UAudioSubsystem::Mark(marker);
	marker.SetField("PlayingSounds");
	for (PlayingSound& playing : PlayingSounds)
	{
		marker.MarkConst(playing.Sound);
		// An ambient sound reads its actor each update: kept, never None.
		if ((playing.Id & 14) == SLOT_Ambient * 2)
			marker.MarkConst(playing.Actor);
		else
			marker.Mark(playing.Actor);
	}
	marker.SetField("CurrentSong");
	marker.MarkConst(CurrentSong);
	marker.SetField("m_Viewport");
	marker.MarkConst(m_Viewport);
}

void UDXEngineAudioDevice::PurgeDying()
{
	for (auto it = SoundSerials.begin(); it != SoundSerials.end();)
	{
		if (GC::IsDying(it->first))
			it = SoundSerials.erase(it);
		else
			++it;
	}
	if (GC::IsDying(m_ReverbZone))
		m_ReverbZone = nullptr;
}
