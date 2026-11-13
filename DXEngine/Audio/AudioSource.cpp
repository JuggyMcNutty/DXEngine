
#include "Precomp.h"
#include "AudioSource.h"
#include "GalaxyMusic.h"
#include "Math/vec.h"
#include "Utils/Exception.h"
#include "Utils/File.h"
#include "Utils/StrTools.h"
#include <functional>

#ifdef _MSC_VER
#pragma warning(disable: 4267)
#pragma warning(disable: 4456)
#pragma warning(disable: 4706)
#pragma warning(disable: 4457)
#pragma warning(disable: 4245)
#pragma warning(disable: 4244)
#endif

// Downloaded from https://github.com/lieff/minimp3
#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "stb_vorbis.h"

#include "libopenmpt/libopenmpt.hpp"

class Mp3AudioSource : public AudioSource
{
public:
	Mp3AudioSource(Array<uint8_t> filedata) : input(std::move(filedata))
	{
		int error = mp3dec_ex_open_buf(&decoder, input.data(), input.size(), MP3D_SEEK_TO_SAMPLE);
		if (error)
			Exception::Throw("mp3dec_ex_open_buf failed");
		// Data with no MP3 frame opens with its info zeroed: refused here, not
		// divided by its zero channels at the first decode
		if (decoder.info.channels <= 0 || decoder.info.hz <= 0)
			Exception::Throw("No MP3 frames in the data");

		memset(&frame_info, 0, sizeof(frame_info));
	}

	~Mp3AudioSource()
	{
		mp3dec_ex_close(&decoder);
	}

	int GetFrequency() override
	{
		return decoder.info.hz;
	}

	int GetChannels() override
	{
		return decoder.info.channels;
	}

	int GetSamples() override
	{
		return decoder.samples / decoder.info.channels;
	}

	void SeekToSample(uint64_t position) override
	{
		int error = mp3dec_ex_seek(&decoder, position);
		if (error)
			Exception::Throw("mp3dec_ex_seek failed");
	}

	size_t ReadSamples(float* output, size_t samples) override
	{
		size_t samplesrequested = samples;
		while (samples > 0)
		{
			if (available == 0)
			{
				if (!endofdata)
					ReadFrame(samples);
				else
					break;
			}

			size_t count = std::min(samples, available);

			for (size_t i = 0; i < count; i++)
			{
				output[i] = buffer[i] * (float)(1.0 / 32768.0);
			}

			output += count;
			buffer += count;
			available -= count;
			samples -= count;
		}
		return samplesrequested - samples;
	}

	void ReadFrame(size_t samples)
	{
		buffer = nullptr;
		available = mp3dec_ex_read_frame(&decoder, &buffer, &frame_info, samples);
		endofdata = (available == 0);
	}

	Array<uint8_t> input;
	mp3d_sample_t* buffer = nullptr;
	size_t available = 0;
	bool endofdata = false;
	mp3dec_ex_t decoder;
	mp3dec_frame_info_t frame_info;
};

class WavAudioSource : public AudioSource
{
public:
	WavAudioSource(Array<uint8_t> filedata) : filedata(std::move(filedata))
	{
		drwav_bool32 result = drwav_init_ex(&decoder, &WavAudioSource::StaticInputRead, &WavAudioSource::StaticInputSeek, nullptr, this, nullptr, 0, nullptr);
		if (!result)
			Exception::Throw("Could not open wav file");

		// The first loop, as Galaxy plays it, whatever the count (dr_wav keeps
		// the file's count and stores the first), and only from a smpl chunk
		// after the data chunk: Galaxy's loader sets the sample up afresh at
		// the data chunk, an earlier loop lost (Galaxy.dll 0x10619d20)
		if (decoder.smpl.numSampleLoops >= 1 && SmplFollowsData())
		{
			bIsLooped = true;
			loopStart = decoder.smpl.loops[0].start;
			loopEnd = decoder.smpl.loops[0].end;
		}
		eightBit = decoder.bitsPerSample == 8;
	}

	bool SmplFollowsData() const
	{
		bool data = false;
		for (uint64_t at = 12; at + 8 <= filedata.size();)
		{
			const uint8_t* chunk = filedata.data() + at;
			uint32_t size = chunk[4] | (chunk[5] << 8) | (chunk[6] << 16) | ((uint32_t)chunk[7] << 24);
			if (memcmp(chunk, "data", 4) == 0)
				data = true;
			else if (memcmp(chunk, "smpl", 4) == 0)
				return data;
			at += 8 + (uint64_t)size + (size & 1);
		}
		return false;
	}

	~WavAudioSource()
	{
		drwav_uninit(&decoder);
	}

	int GetFrequency() override
	{
		return decoder.sampleRate;
	}

	int GetChannels() override
	{
		return decoder.channels;
	}

	int GetSamples() override
	{
		// The header's count held to what the data chunk holds (a claim of
		// 0xFFFFFFF0 bytes committed 8 GB) and to int
		uint64_t frames = decoder.totalPCMFrameCount;
		uint64_t frameBytes = (uint64_t)decoder.channels * (decoder.bitsPerSample / 8);
		if (frameBytes > 0)
			frames = std::min<uint64_t>(frames, decoder.dataChunkDataSize / frameBytes);
		return (int)std::min<uint64_t>(frames, INT_MAX);
	}

	void SeekToSample(uint64_t position) override
	{
		if (drwav_seek_to_pcm_frame(&decoder, position))
			eofdata = false;
	}

	size_t ReadSamples(float* output, size_t samples) override
	{
		if (!eofdata)
		{
			// 8-bit data as Galaxy holds it, each byte less 128 times 256 (its
			// loader's += 0x80, Galaxy.dll 0x10619d20), which dr_wav's 16-bit
			// read gives and its float read does not
			size_t samplesread;
			if (eightBit)
			{
				std::vector<int16_t> data(samples);
				samplesread = drwav_read_pcm_frames_s16(&decoder, samples / decoder.channels, data.data()) * decoder.channels;
				for (size_t i = 0; i < samplesread; i++)
					output[i] = data[i] * (1.0f / 32768.0f);
			}
			else
			{
				samplesread = drwav_read_pcm_frames_f32(&decoder, samples / decoder.channels, output) * decoder.channels;
			}
			eofdata = (samplesread != samples);
			return samplesread;
		}
		else
		{
			return 0;
		}
	}

	size_t InputRead(void* pBufferOut, size_t bytesToRead)
	{
		size_t available = filedata.size() - inputpos;
		size_t count = std::min(bytesToRead, available);
		memcpy(pBufferOut, filedata.data() + inputpos, count);
		inputpos += count;
		return count;
	}

	drwav_bool32 InputSeek(int offset, drwav_seek_origin origin)
	{
		if (origin == drwav_seek_origin_start)
		{
			if (offset < 0 || offset >(int)filedata.size())
				return DRWAV_FALSE;
			inputpos = offset;
			return DRWAV_TRUE;
		}
		else if (origin == drwav_seek_origin_current)
		{
			if ((offset < 0 && (size_t)-offset > inputpos) || inputpos + offset > filedata.size())
				return DRWAV_FALSE;
			inputpos = (size_t)((int64_t)inputpos + offset);
			return DRWAV_TRUE;
		}
		else
		{
			return DRWAV_FALSE;
		}
	}

	static size_t StaticInputRead(void* pUserData, void* pBufferOut, size_t bytesToRead)
	{
		return static_cast<WavAudioSource*>(pUserData)->InputRead(pBufferOut, bytesToRead);
	}

	static drwav_bool32 StaticInputSeek(void* pUserData, int offset, drwav_seek_origin origin)
	{
		return static_cast<WavAudioSource*>(pUserData)->InputSeek(offset, origin);
	}

	drwav decoder = {};
	Array<uint8_t> filedata;
	size_t inputpos = 0;
	bool eofdata = false;
};

class OggAudioSource : public AudioSource
{
public:
	OggAudioSource(Array<uint8_t> input, bool loop) : filedata(std::move(input))
	{
		bIsLooped = loop;
		int error = 0;
		handle = stb_vorbis_open_pushdata(filedata.data(), (int)filedata.size(), &stream_byte_offset, &error, nullptr);
		if (!handle)
			Exception::Throw("Unable to read ogg file");

		stream_info = stb_vorbis_get_info(handle);
	}

	~OggAudioSource()
	{
		if (handle)
			stb_vorbis_close(handle);
	}

	int GetFrequency() override
	{
		return stream_info.sample_rate;
	}

	int GetChannels() override
	{
		return stream_info.channels;
	}

	int GetSamples() override
	{
		return 0; // To do: how to get this
	}

	void SeekToSample(uint64_t pos) override
	{
		// Currently only support seeking to beginning of stream.
		if (pos != 0) return;
		Restart();
	}

	// The stream from its start, a decoder of its own: nothing the last one
	// decoded is kept, as its packet goes with it (bugs.md AUD.19). On the
	// music thread, which must not touch the script's call stack as
	// Exception::Throw does: a plain error, which the music thread catches.
	void Restart()
	{
		if (handle)
			stb_vorbis_close(handle);
		handle = nullptr;
		stream_byte_offset = 0;
		stream_eof = false;
		pcm = nullptr;
		pcm_position = 0;
		pcm_samples = 0;

		int error = 0;
		handle = stb_vorbis_open_pushdata(filedata.data(), (int)filedata.size(), &stream_byte_offset, &error, nullptr);
		if (handle == nullptr)
			throw std::runtime_error("Unable to read ogg file");

		stream_info = stb_vorbis_get_info(handle);
	}

	// The next packet's samples; at the data's end what the last call
	// decoded is played out first
	void DecodePacket()
	{
		pcm = nullptr;
		pcm_position = 0;
		pcm_samples = 0;
		int bytes_used = stb_vorbis_decode_frame_pushdata(handle, filedata.data() + stream_byte_offset, (int)filedata.size() - stream_byte_offset, nullptr, &pcm, &pcm_samples);
		stream_byte_offset += bytes_used;
		if (bytes_used == 0 || (size_t)stream_byte_offset >= filedata.size())
			stream_eof = true;
		if (!pcm)
			pcm_samples = 0;
	}

	// A looping song plays to its last sample and on from its first in the
	// same read; one that decodes nothing from its start ends there
	size_t ReadSamples(float* output, size_t requestedsamples) override
	{
		int c = stream_info.channels;
		int data_requested = (int)(requestedsamples / c);
		int data_left = data_requested;
		bool restarted = false;     // started again, nothing decoded since
		while (data_left > 0)
		{
			if (pcm_position == pcm_samples)
			{
				if (!stream_eof)
					DecodePacket();
				else if (bIsLooped && !restarted)
				{
					Restart();
					restarted = true;
				}
				else
					break;
				continue;
			}
			restarted = false;

			int samples = pcm_samples - pcm_position;
			if (samples > data_left) samples = data_left;

			int buffer_pos = (data_requested - data_left) * c;
			for (int j = 0; j < c; j++)
			{
				const float* src = &pcm[j][pcm_position];
				float* dst = &output[buffer_pos + j];
				for (int i = 0; i < samples; i++)
				{
					*dst = src[i];
					dst += c;
				}
			}

			pcm_position += samples;
			data_left -= samples;
		}

		return (data_requested - data_left) * c;
	}

	Array<uint8_t> filedata;
	bool stream_eof = false;

	stb_vorbis* handle = nullptr;
	stb_vorbis_info stream_info = {};
	int stream_byte_offset = 0;

	float** pcm = nullptr;
	int pcm_position = 0;
	int pcm_samples = 0;
};

class OpenMPTAudioSource : public AudioSource
{
public:
	OpenMPTAudioSource(Array<uint8_t> initfiledata, bool loop, int subsong)
	{
		// An IT song as Galaxy's player plays it (GalaxyMusic.h; bugs.md
		// AUD.10)
		Array<uint8_t> galaxySong = GalaxyMusic::ConvertIT(initfiledata);
		galaxy = !galaxySong.empty();
		if (galaxy)
			initfiledata = std::move(galaxySong);

		uint8_t* data = initfiledata.begin();
		size_t size = initfiledata.size();
		int probe_result = openmpt::probe_file_header(openmpt::probe_file_header_flags_default2, data, size);

		// TODO: should we handle probe_file_header_result_wantmoredata?
		if (probe_result != openmpt::probe_file_header_result_success)
			Exception::Throw("Module file unsupported by OpenMPT");

		// TODO: log param defaults to std::clog, do we want to keep it that way?
		module = new openmpt::module(data, size);

		// Galaxy's mixer, whatever the module: a voice read between a sample
		// and the next linearly (its SSE routine, Galaxy.dll 0x10622730), and
		// its gains set anew at each tick of the song, never ramped
		// (0x10614920)
		module->set_render_param(openmpt::module::RENDER_INTERPOLATIONFILTER_LENGTH, 2);
		module->set_render_param(openmpt::module::RENDER_VOLUMERAMPING_STRENGTH, 0);
		if (galaxy)
		{
			module->ctl_set_boolean(GalaxyMusic::PanningCtl, true);
			module->ctl_set_boolean(GalaxyMusic::PlayerCtl, true);
		}

		subsong = clamp(subsong, 0, module->get_num_subsongs() - 1);
		if (subsong != 0)
			module->select_subsong(subsong);

		// Galaxy starts at the order SongSection (galaxy-dll.md, Music), a
		// jump there passing over an empty order (SetOrder; Galaxy.dll
		// 0x10615710), at once, no row playing

		module->set_repeat_count(-1);
	}

	~OpenMPTAudioSource()
	{
		if (module)
			delete module;
	}

	// Rendered at the mix rate, as Galaxy mixes a song's voices at
	// OutputRate (galaxy-dll.md, the mixer)
	int GetFrequency() override
	{
		return rate;
	}

	void SetFrequency(int value) override
	{
		if (value > 0)
			rate = value;
	}

	int GetChannels() override
	{
		return 2;
	}

	int GetSamples() override
	{
		return -1;
	}

	void SeekToSample(uint64_t position) override
	{
	}

	size_t ReadSamples(float* output, size_t samples) override
	{
		if (!module)
			return 0;

		// OpenMPT answers the frames it rendered; a read answers samples.
		// Mono, OpenMPT's mono mix in both channels, spread from the last
		// frame back so none is overwritten before it is read
		if (mono)
		{
			size_t frames = module->read(GetFrequency(), samples / GetChannels(), output);
			for (size_t i = frames; i-- > 0;)
				output[2 * i] = output[2 * i + 1] = output[i];
			return frames * GetChannels();
		}
		return module->read_interleaved_stereo(GetFrequency(), samples / GetChannels(), output) * GetChannels();
	}

	int GetOrder() override
	{
		return module ? (int)module->get_current_order() : -1;
	}

	// A song Galaxy plays jumps as Galaxy's section change: its voices
	// silenced and the order started as the playing row ends, the speed,
	// tempo and pans as they stand but for order 0's own, GetOrder the order
	// before it until then, 255 before order 0 (play.galaxy; bugs.md AUD.16,
	// AUD.17)
	void SetOrder(int order, bool atOnce = false) override
	{
		if (!module)
			return;
		if (galaxy && atOnce)
			module->ctl_set_boolean(GalaxyMusic::JumpAtOnceCtl, true);
		module->set_position_order_row(order, 0);
	}

	// Galaxy's IT loader keeps the orders before the first end mark, at most
	// 255, and its glxControlMusic refuses a section past the last of them
	// (Galaxy.dll 0x10628990, 0x1060d0e0); an empty song's last order counts
	// down past 0 to 255, so it takes any
	int GetSections() override
	{
		if (!module || !galaxy)
			return -1;
		int orders = module->get_num_orders();
		return orders > 0 ? std::min(orders, 255) : 256;
	}

	double GetTempo() override
	{
		return module ? module->get_current_tempo2() : 0.0;
	}

	// Galaxy's mono mix, each voice at the mean of its two sides' gains, a
	// surround voice's both kept (Galaxy.dll 0x10623210; bugs.md AUD.14):
	// OpenMPT's mono render, a Galaxy song's voices keeping their pans
	void SetMono(bool value) override
	{
		mono = value;
	}

	openmpt::module* module;
	int rate = 44100;
	bool mono = false;
	bool galaxy = false;        // an IT song, played as Galaxy plays it
};

std::unique_ptr<AudioSource> AudioSource::CreateMp3(Array<uint8_t> filedata)
{
	return std::make_unique<Mp3AudioSource>(std::move(filedata));
}

std::unique_ptr<AudioSource> AudioSource::CreateWav(Array<uint8_t> filedata)
{
	return std::make_unique<WavAudioSource>(std::move(filedata));
}

std::unique_ptr<AudioSource> AudioSource::CreateOgg(Array<uint8_t> filedata, bool loop)
{
	return std::make_unique<OggAudioSource>(std::move(filedata), loop);
}

std::unique_ptr<AudioSource> AudioSource::CreateMod(Array<uint8_t> filedata, bool loop, int subsong)
{
	return std::make_unique<OpenMPTAudioSource>(filedata, loop, subsong);
}
