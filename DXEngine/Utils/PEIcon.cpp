#include "Precomp.h"
#include "PEIcon.h"
#include "File.h"
#include <cstring>

namespace
{
	// The resource types of an icon's image and of a group of them.
	const int ResourceIcon = 3;
	const int ResourceGroupIcon = 14;

	struct PEFile
	{
		const uint8_t* data = nullptr;
		size_t size = 0;
		size_t sections = 0, numSections = 0;	// the section table's offset and length
		size_t rsrc = 0;						// the resource directory's offset
	};

	uint16_t U16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
	uint32_t U32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
	bool Has(const PEFile& f, size_t off, size_t n) { return off <= f.size && n <= f.size - off; }

	// A virtual address's place in the file, through the section holding it.
	bool RvaOffset(const PEFile& f, uint32_t rva, size_t& off)
	{
		for (size_t i = 0; i < f.numSections; i++)
		{
			const uint8_t* s = f.data + f.sections + i * 40;
			uint32_t vsize = U32(s + 8), va = U32(s + 12), rawsize = U32(s + 16), rawptr = U32(s + 20);
			uint32_t span = vsize > rawsize ? vsize : rawsize;
			if (rva >= va && rva - va < span)
			{
				off = (size_t)rawptr + (rva - va);
				return true;
			}
		}
		return false;
	}

	// The entry of a resource directory with the numeric id, skipping the named
	// ones, or for id -1 the first, whatever it is (a language); the target is
	// an offset from the resource directory, its top bit set for a subdirectory.
	bool DirEntry(const PEFile& f, uint32_t dir, int id, uint32_t& target)
	{
		size_t at = f.rsrc + (dir & 0x7fffffff);
		if (!Has(f, at, 16))
			return false;
		unsigned named = U16(f.data + at + 12), ids = U16(f.data + at + 14);
		for (unsigned i = 0; i < named + ids; i++)
		{
			size_t e = at + 16 + (size_t)i * 8;
			if (!Has(f, e, 8))
				return false;
			uint32_t name = U32(f.data + e);
			if ((id >= 0 && !(name & 0x80000000) && name == (uint32_t)id) || (id < 0 && i == 0))
			{
				target = U32(f.data + e + 4);
				return true;
			}
		}
		return false;
	}

	// A resource's data: its type, its id, its first language.
	bool Resource(const PEFile& f, int type, int id, const uint8_t*& data, size_t& len)
	{
		uint32_t t, n, lang;
		if (!DirEntry(f, 0, type, t) || !(t & 0x80000000))
			return false;
		if (!DirEntry(f, t, id, n) || !(n & 0x80000000))
			return false;
		if (!DirEntry(f, n, -1, lang) || (lang & 0x80000000))
			return false;
		size_t leaf = f.rsrc + lang, off;
		if (!Has(f, leaf, 16) || !RvaOffset(f, U32(f.data + leaf), off))
			return false;
		uint32_t size = U32(f.data + leaf + 4);
		if (!Has(f, off, size))
			return false;
		data = f.data + off;
		len = size;
		return true;
	}

	bool OpenPE(PEFile& f)
	{
		if (!Has(f, 0, 0x40) || f.data[0] != 'M' || f.data[1] != 'Z')
			return false;
		size_t pe = U32(f.data + 0x3c);
		if (!Has(f, pe, 24) || memcmp(f.data + pe, "PE\0\0", 4) != 0)
			return false;
		size_t nsec = U16(f.data + pe + 6), optsz = U16(f.data + pe + 20), opt = pe + 24;
		if (!Has(f, opt, optsz) || optsz < 2)
			return false;
		size_t dirs = opt + (U16(f.data + opt) == 0x20b ? 112 : 96);	// PE32+ or PE32
		if (dirs + 3 * 8 > opt + optsz)
			return false;
		uint32_t rsrcRva = U32(f.data + dirs + 2 * 8);
		f.sections = opt + optsz;
		f.numSections = nsec;
		if (!Has(f, f.sections, nsec * 40))
			return false;
		return rsrcRva && RvaOffset(f, rsrcRva, f.rsrc) && Has(f, f.rsrc, 16);
	}

	// An icon's DIB, top row first.
	std::shared_ptr<Image> Decode(const uint8_t* d, size_t len, int w, int h)
	{
		if (len >= 8 && memcmp(d, "\x89PNG", 4) == 0)
			return nullptr;
		if (len < 40 || U32(d) < 40)
			return nullptr;
		uint32_t hdr = U32(d);
		int bpp = U16(d + 14);
		uint32_t compression = U32(d + 16), used = U32(d + 32);
		if (compression != 0 || !(bpp == 1 || bpp == 4 || bpp == 8 || bpp == 24 || bpp == 32))
			return nullptr;
		size_t colours = bpp <= 8 ? (used ? used : (size_t)1 << bpp) : 0;
		size_t table = hdr, pixels = table + colours * 4;
		size_t xorStride = ((size_t)w * bpp + 31) / 32 * 4, andStride = ((size_t)w + 31) / 32 * 4;
		size_t mask = pixels + xorStride * h;
		if (mask + andStride * h > len)
			return nullptr;

		std::vector<uint8_t> bgra((size_t)w * h * 4);
		bool anyAlpha = false;
		for (int y = 0; y < h; y++)
		{
			const uint8_t* row = d + pixels + xorStride * (h - 1 - y);
			const uint8_t* maskRow = d + mask + andStride * (h - 1 - y);
			for (int x = 0; x < w; x++)
			{
				const uint8_t* c;
				uint8_t a = 255;
				if (bpp <= 8)
				{
					size_t bit = (size_t)x * bpp;
					unsigned index = (row[bit / 8] >> (8 - bpp - bit % 8)) & ((1u << bpp) - 1);
					c = d + table + (index < colours ? index : 0) * 4;
				}
				else
				{
					c = row + (size_t)x * (bpp / 8);
					if (bpp == 32)
					{
						a = c[3];
						anyAlpha = anyAlpha || a != 0;
					}
				}
				if (bpp != 32 && (maskRow[x / 8] >> (7 - x % 8)) & 1)
					a = 0;
				uint8_t* p = &bgra[((size_t)y * w + x) * 4];
				p[0] = c[0];
				p[1] = c[1];
				p[2] = c[2];
				p[3] = a;
			}
		}
		// A 32-bit icon without alpha leaves its pixels out by the mask.
		if (bpp == 32 && !anyAlpha)
		{
			for (int y = 0; y < h; y++)
			{
				const uint8_t* maskRow = d + mask + andStride * (h - 1 - y);
				for (int x = 0; x < w; x++)
					bgra[((size_t)y * w + x) * 4 + 3] = (maskRow[x / 8] >> (7 - x % 8)) & 1 ? 0 : 255;
			}
		}
		return Image::Create(w, h, ImageFormat::B8G8R8A8, bgra.data());
	}
}

std::vector<std::shared_ptr<Image>> PEIcon::Load(const std::string& path, int group)
{
	Array<uint8_t> file;
	try
	{
		file = File::read_all_bytes(path);
	}
	catch (...)
	{
		return {};
	}

	PEFile f;
	f.data = file.data();
	f.size = file.size();
	const uint8_t* grp;
	size_t grpLen;
	if (!OpenPE(f) || !Resource(f, ResourceGroupIcon, group, grp, grpLen) || grpLen < 6)
		return {};

	// GRPICONDIR: 6 bytes, then 14 an entry -- width, height (0 is 256),
	// colours, reserved, planes, bits, bytes, the RT_ICON's id.
	unsigned count = U16(grp + 4);
	std::vector<std::shared_ptr<Image>> images;
	int taken = -1;
	for (int size : { 32, 16 })
	{
		int best = -1, bestScore = -1;
		for (unsigned i = 0; i < count && 6 + (i + 1) * 14 <= grpLen; i++)
		{
			const uint8_t* e = grp + 6 + i * 14;
			int w = e[0] ? e[0] : 256, h = e[1] ? e[1] : 256, bits = U16(e + 6);
			int score = (w == size && h == size ? 1 << 20 : 0) + w * 64 + bits;
			if (w == h && score > bestScore)
			{
				best = (int)i;
				bestScore = score;
			}
		}
		if (best < 0)
			return {};
		if (best == taken)
			continue;
		taken = best;

		const uint8_t* e = grp + 6 + best * 14;
		const uint8_t* img;
		size_t imgLen;
		if (!Resource(f, ResourceIcon, U16(e + 12), img, imgLen))
			return {};
		auto image = Decode(img, imgLen, e[0] ? e[0] : 256, e[1] ? e[1] : 256);
		if (!image)
			return {};
		images.push_back(std::move(image));
	}
	return images;
}

std::vector<std::shared_ptr<Image>> PEIcon::OfGame(const std::string& gameRootFolder, const std::string& gameExecutableName)
{
	return Load((fs::path(gameRootFolder) / "System" / (gameExecutableName + ".exe")).string(), 128);
}
