
#include "Precomp.h"
#include "GalaxyMusic.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <vector>

namespace
{
	// IT's commands, A to Z
	enum : uint8_t
	{
		CmdA = 1, CmdB, CmdC, CmdD, CmdE, CmdF, CmdG, CmdH, CmdI, CmdJ, CmdK, CmdL, CmdM,
		CmdN, CmdO, CmdP, CmdQ, CmdR, CmdS, CmdT, CmdU, CmdV, CmdW, CmdX, CmdY, CmdZ
	};

	uint16_t Read16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
	uint32_t Read32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

	void Write32(uint8_t* p, uint32_t v)
	{
		p[0] = (uint8_t)v;
		p[1] = (uint8_t)(v >> 8);
		p[2] = (uint8_t)(v >> 16);
		p[3] = (uint8_t)(v >> 24);
	}

	// What Galaxy's loader keeps of a channel from one cell to the next, and
	// from one pattern to the next, in the order it converts them: the last
	// mask, note, instrument, volume and command, and one memory for every
	// command's parameter.
	struct ChannelState
	{
		uint8_t mask = 0, note = 0, instrument = 0, volume = 0, command = 0, memory = 0;
	};

	struct Effect
	{
		uint8_t command = 0, param = 0;
	};

	// Galaxy's command for an IT command, its parameter resolved, in IT's
	// terms again; command 0 where Galaxy plays nothing (its loader's switch,
	// 0x10629e0d; its player's commands, 0x10616eb0).
	Effect ConvertEffect(uint8_t command, uint8_t p)
	{
		uint8_t x = p >> 4, y = p & 15;
		switch (command)
		{
		case CmdA:
			// A speed over 32 is 32 (0x10629e54). With nothing remembered,
			// Galaxy's player stops the song, which is not carried.
			return p ? Effect{ CmdA, std::min<uint8_t>(p, 0x20) } : Effect{};
		case CmdB:
			return { CmdB, p };
		case CmdC:
			// The row read as decimal digits, the MOD way
			return { CmdC, (uint8_t)(x * 10 + y) };
		case CmdD:
			if (p == 0)
				return {};
			if (y == 15 && x)
				return { CmdD, p };                           // fine up by x
			if (p >= 0xF1)
				return { CmdD, (uint8_t)(0xF0 | y) };         // fine down by y
			return y ? Effect{ CmdD, y } : Effect{ CmdD, (uint8_t)(x << 4) };  // down by y, else up by x
		case CmdE:
		case CmdF:
			if (p == 0)
				return {};
			if (p >= 0xE0 && p < 0xF0)
				return { command, (uint8_t)(0xF0 | ((y + 2) >> 2)) };  // extra fine made fine
			return { command, p };
		case CmdG:
		case CmdH:
		case CmdO:
		case CmdR:
			return { command, p };
		case CmdJ:
			return p ? Effect{ CmdJ, p } : Effect{};
		case CmdK:
			// Galaxy's slide in K and L: up by x, else down by y, no fine slide
			return p ? Effect{ CmdK, (uint8_t)(p > 15 ? p & 0xF0 : p) } : Effect{ CmdH, 0 };
		case CmdL:
			return p ? Effect{ CmdL, (uint8_t)(p > 15 ? p & 0xF0 : p) } : Effect{ CmdG, 0 };
		case CmdQ:
			// The retrigger's volume change dropped; Q00 retriggers nothing
			return y ? Effect{ CmdQ, y } : Effect{};
		case CmdS:
			switch (x)
			{
			case 0x3: case 0x4: case 0xB: case 0xD: case 0xE:
				return { CmdS, p };
			case 0x8:
				return { CmdX, (uint8_t)(8 * y + 4) };        // Galaxy's pan, (y << 11) + 1024 of 32767: its 8xx's 8y + 4
			case 0x9:
				return { CmdX, 0xA4 };                        // any S9x its 8xx's surround, 0xA4
			case 0xC:
				// The note's volume goes to 0 a tick later than IT's cut
				return y < 15 ? Effect{ CmdS, (uint8_t)(0xC0 | (y + 1)) } : Effect{};
			default:
				return {};                                    // S0, S1, S2, S5, S6, S7, SA, SF
			}
		case CmdT:
			// Galaxy's Fxx: a tempo over 32, and 32 a speed; a tempo slide none
			return p > 0x20 ? Effect{ CmdT, p } : p == 0x20 ? Effect{ CmdA, 0x20 } : Effect{};
		case CmdX:
			// Galaxy's 8xx, its parameter as it is, which libopenmpt's Galaxy
			// player takes as Galaxy's (play.galaxy; Snd_fx.cpp's Panning): 0 to
			// 0x80 left to right, over it a surround, at a pan from 0x84 to 0xC4
			return { CmdX, p };
		default:
			return {};                                        // I, M, N, P, U, V, W, Y, Z
		}
	}

	// A pattern as Galaxy's loader converts it, packed as IT packs one: every
	// field of a cell given, so nothing rests on another cell.
	std::vector<uint8_t> ConvertPattern(const uint8_t* data, size_t size, size_t at, ChannelState* channels)
	{
		uint16_t length = Read16(data + at);
		uint16_t rows = Read16(data + at + 2);
		size_t pos = at + 8, end = std::min(size, pos + length);

		std::vector<uint8_t> body;
		for (uint16_t row = 0; row < rows; row++)
		{
			while (pos < end)
			{
				uint8_t b = data[pos++];
				if (b == 0)
					break;
				int ch = (b - 1) & 63;
				ChannelState& state = channels[ch];
				if ((b & 0x80) && pos < end)
					state.mask = data[pos++];
				uint8_t mask = state.mask;
				size_t need = ((mask & 1) ? 1 : 0) + ((mask & 2) ? 1 : 0) + ((mask & 4) ? 1 : 0) + ((mask & 8) ? 2 : 0);
				if (pos + need > end)
				{
					pos = end;
					break;
				}

				// Galaxy's note: 1 to 120 a note, 0x80 a note off, a cut the
				// volume 0 with no note (0x10629c0b-0x10629c21)
				uint8_t note = 0, instrument = 0, volume = 0xFF, command = 0, param = 0;
				if (mask & 1)
				{
					note = data[pos++];
					if (note < 120)
						note++;
					if (note == 254)
					{
						volume = 0;
						note = 0;
					}
					if (note == 255)
						note = 0x80;
					state.note = note;
				}
				if (mask & 2)
				{
					instrument = data[pos++];
					state.instrument = instrument;
				}
				if (mask & 4)
				{
					// The volume column's volumes alone, doubled to 127 at most;
					// its other commands dropped (0x10629ca7)
					uint8_t v = data[pos++];
					if (v <= 64)
					{
						v = (uint8_t)std::min(2 * v, 127);
						volume &= v;
					}
					state.volume = v;
				}
				if (mask & 8)
				{
					command = data[pos++];
					param = data[pos++];
					state.command = command;
					if (command != CmdA && command != CmdD && command != CmdE && command != CmdF && command != CmdK && command != CmdL)
						state.memory = param;                     // 0x10629d63
				}
				if (mask & 0x10)
					note = state.note;
				if (mask & 0x20)
					instrument = state.instrument;
				if ((mask & 0x40) && state.volume <= 0x7F)
					volume &= state.volume;
				if (mask & 0x80)
				{
					command = state.command;
					param = state.memory;                         // 0x10629df6
				}
				// A, D, E, F, K and L take a parameter of 0 from the memory and
				// keep theirs in it, whatever command left it there
				if (command == CmdA || command == CmdD || command == CmdE || command == CmdF || command == CmdK || command == CmdL)
				{
					if (param == 0)
						param = state.memory;
					state.memory = param;
				}
				Effect effect = command ? ConvertEffect(command, param) : Effect{};

				// Only the first 32 channels (0x1062a26e)
				if (ch >= 32 || (note == 0 && instrument == 0 && volume == 0xFF && effect.command == 0))
					continue;

				int itNote = -1;
				if (note >= 0x80)
					itNote = 255;                                 // off, the IT fades among them
				else if (note >= 1 && note <= 120)
					itNote = note - 1;
				else if (note > 120)
					itNote = 254;                                 // a note Galaxy plays at no pitch
				uint8_t cellMask = (itNote >= 0 ? 1 : 0) | (instrument ? 2 : 0) | (volume != 0xFF ? 4 : 0) | (effect.command ? 8 : 0);
				body.push_back((uint8_t)((ch + 1) | 0x80));
				body.push_back(cellMask);
				if (itNote >= 0)
					body.push_back((uint8_t)itNote);
				if (instrument)
					body.push_back(instrument);
				if (volume != 0xFF)
					body.push_back((uint8_t)((volume + 1) >> 1));
				if (effect.command)
				{
					body.push_back(effect.command);
					body.push_back(effect.param);
				}
			}
			body.push_back(0);
		}

		// A pattern too long to pack as IT packs one is none
		if (body.size() > 0xFFFF)
			return {};
		std::vector<uint8_t> pattern(8);
		pattern[0] = (uint8_t)body.size();
		pattern[1] = (uint8_t)(body.size() >> 8);
		pattern[2] = (uint8_t)rows;
		pattern[3] = (uint8_t)(rows >> 8);
		pattern.insert(pattern.end(), body.begin(), body.end());
		return pattern;
	}
}

Array<uint8_t> GalaxyMusic::ConvertIT(const Array<uint8_t>& song)
{
	const uint8_t* data = song.data();
	size_t size = song.size();
	if (size < 0xC0 || memcmp(data, "IMPM", 4) != 0)
		return {};

	uint16_t orders = Read16(data + 0x20), instruments = Read16(data + 0x22), samples = Read16(data + 0x24), patterns = Read16(data + 0x26);
	uint16_t flags = Read16(data + 0x2C);
	size_t tables = 0xC0 + (size_t)orders, patternTable = tables + 4 * ((size_t)instruments + samples);
	if (patternTable + 4 * (size_t)patterns > size)
		return {};

	Array<uint8_t> out = song;
	uint8_t* d = out.data();

	// Galaxy plays every IT song one way, whatever tracker wrote it, and reads
	// an instrument by IT 2.x's layout: the song is signed as Impulse
	// Tracker 2.14's, which OpenMPT plays as IT does, its mix levels, its
	// instruments and its samples taken as IT's (a sample with no data flag
	// but a length plays, as Galaxy's loader plays it)
	d[0x28] = 0x14;
	d[0x29] = 0x02;
	d[0x2A] = 0x14;
	d[0x2B] = 0x02;
	memset(d + 0x3C, 0, 4);

	// Galaxy reads the global volume, which the Music slider replaces as the
	// song starts (Update's SetVolumes after glxLoadMusic, 0x1060a810), and
	// never the mix volume nor the channels' volumes. A channel's pan over 64
	// is its surround, played, where IT's 128 and over mute the channel, a
	// quarter left in libopenmpt's Galaxy player (play.galaxy) as in Galaxy's
	// (bugs.md AUD.17).
	d[0x30] = 128;
	d[0x31] = 128;
	for (int c = 0; c < 64; c++)
	{
		if (d[0x40 + c] > 64)
			d[0x40 + c] = 100;
		d[0x80 + c] = 64;
	}

	// The orders end at the first end mark; an order whose pattern holds no
	// data is passed over, as Galaxy's player passes over a pattern it never
	// converted (0x10615710), where IT plays 64 empty rows
	bool ended = false;
	for (size_t i = 0; i < orders; i++)
	{
		uint8_t& order = d[0xC0 + i];
		if (ended || order == 255)
		{
			ended = true;
			order = 255;
		}
		else if (order < 254 && (order >= patterns || Read32(data + patternTable + 4 * order) == 0))
		{
			order = 254;
		}
	}

	// A sample's global volume, sustain loop, stereo data and vibrato are
	// none of Galaxy's (0x10628990's sample header: its volume, pan, flags,
	// loop and rate alone)
	for (size_t i = 0; i < samples; i++)
	{
		uint32_t at = Read32(data + tables + 4 * ((size_t)instruments + i));
		if (at == 0 || (size_t)at + 0x50 > size || memcmp(data + at, "IMPS", 4) != 0)
			continue;
		d[at + 0x11] = 64;
		d[at + 0x12] &= (uint8_t)~(0x04 | 0x20 | 0x80);
		d[at + 0x4D] = 0;
	}

	// Nor an instrument's global volume, default pan, random variations,
	// new note action, pitch-pan separation or filter: a new note cuts the
	// last, and the pitch envelope is one whatever its filter flag
	if (flags & 4)
	{
		for (size_t i = 0; i < instruments; i++)
		{
			uint32_t at = Read32(data + tables + 4 * i);
			if (at == 0 || (size_t)at + 0x226 > size || memcmp(data + at, "IMPI", 4) != 0)
				continue;
			d[at + 0x11] = 0;
			d[at + 0x12] = 0;
			d[at + 0x13] = 0;
			d[at + 0x16] = 0;
			d[at + 0x18] = 128;
			d[at + 0x19] |= 0x80;
			d[at + 0x1A] = 0;
			d[at + 0x1B] = 0;
			d[at + 0x3A] &= 0x7F;
			d[at + 0x3B] &= 0x7F;
			d[at + 0x1D4] &= 0x7F;
		}
	}

	// The patterns, each the first time the order list reaches it, Galaxy's
	// channel state carried from one to the next; appended, the ones Galaxy
	// never converts left empty
	ChannelState channels[64];
	std::map<uint16_t, std::vector<uint8_t>> converted;
	for (size_t i = 0; i < orders; i++)
	{
		uint8_t order = data[0xC0 + i];
		if (order == 255)
			break;
		if (order >= 254 || order >= patterns || converted.count(order))
			continue;
		uint32_t at = Read32(data + patternTable + 4 * order);
		if (at == 0 || (size_t)at + 8 > size)
			continue;
		std::vector<uint8_t> pattern = ConvertPattern(data, size, at, channels);
		if (pattern.empty())
			return {};
		converted[order] = std::move(pattern);
	}
	for (uint16_t p = 0; p < patterns; p++)
	{
		auto it = converted.find(p);
		Write32(out.data() + patternTable + 4 * p, it != converted.end() ? (uint32_t)out.size() : 0);
		if (it != converted.end())
			out.insert(out.end(), it->second.begin(), it->second.end());
	}
	return out;
}
