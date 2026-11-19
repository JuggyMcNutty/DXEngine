
#include "Precomp.h"
#include "SoftwarePipelines.h"
#include <utility>

namespace
{
	namespace K = SoftwarePipelineKey;

	constexpr uint32_t Tile = K::Rectangle | K::ColorWrite;

	// The canvas's tiles, the UI and its text, a sprite: solid, masked,
	// translucent, modulated or highlighted (a palette with alpha, solid or
	// masked), as each style draws them, filtered or not, held at the edges
	// or wrapped; the menus' TrueType text (SubpixelFont), untested for depth.
	// The keys the draws make (SoftwareRenderDevice::SetBlend): a tile
	// neither translucent nor modulated writes depth (PF_Occlude, from
	// ApplyPrecedenceRules), a modulated one is never NoSmooth (RenderDevice::
	// TileFlags) and the TrueType text always (PF_SubpixelFont has every bit)
	constexpr SoftwarePipelines::KeyList MakeTileKeys()
	{
		SoftwarePipelines::KeyList list;
		const uint32_t blends[] =
		{
			Tile | K::DepthTest | (uint32_t)SoftwareBlend::Solid | K::DepthWrite,
			Tile | K::DepthTest | (uint32_t)SoftwareBlend::Solid | K::DepthWrite | K::AlphaTest,
			Tile | K::DepthTest | (uint32_t)SoftwareBlend::Translucent,
			Tile | K::DepthTest | (uint32_t)SoftwareBlend::Modulated | K::AlphaTest,
			Tile | K::DepthTest | (uint32_t)SoftwareBlend::Highlighted | K::DepthWrite,
			Tile | K::DepthTest | (uint32_t)SoftwareBlend::Highlighted | K::DepthWrite | K::AlphaTest,
			Tile | (uint32_t)SoftwareBlend::SubpixelFont | K::AlphaTest
		};
		for (uint32_t blend : blends)
		{
			uint32_t kind = blend & K::BlendMask;
			if (kind != (uint32_t)SoftwareBlend::SubpixelFont)
			{
				list.Add(blend);
				list.Add(blend | K::Clamp);
			}
			if (kind != (uint32_t)SoftwareBlend::Modulated)
			{
				list.Add(blend | K::NoSmooth);
				list.Add(blend | K::NoSmooth | K::Clamp);
			}
		}
		return list;
	}

	constexpr SoftwarePipelines::KeyList TileKeys = MakeTileKeys();

	template<size_t... I>
	constexpr std::array<SoftwarePipelines::Entry, sizeof...(I)> Instantiate(std::index_sequence<I...>)
	{
		return { SoftwarePipelines::Entry{ TileKeys.Keys[I], &SoftwareDrawPrimitive<TileKeys.Keys[I], false> }... };
	}

	const auto Tiles = Instantiate(std::make_index_sequence<TileKeys.Count>());
}

const SoftwarePipelines::Entry* SoftwarePipelines::TilePipelines(int& count)
{
	count = (int)Tiles.size();
	return Tiles.data();
}
