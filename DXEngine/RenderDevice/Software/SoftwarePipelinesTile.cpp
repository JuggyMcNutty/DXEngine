
#include "Precomp.h"
#include "SoftwarePipelines.h"
#include <utility>

namespace
{
	namespace K = SoftwarePipelineKey;

	constexpr uint32_t Tile = K::Rectangle | K::ColorWrite;

	// The canvas's tiles, the UI and its text, a sprite: solid, masked,
	// translucent, modulated or highlighted, as each style draws them,
	// filtered or not, held at the edges or wrapped; the menus' TrueType
	// text (SubpixelFont), untested for depth
	constexpr SoftwarePipelines::KeyList MakeTileKeys()
	{
		SoftwarePipelines::KeyList list;
		const uint32_t blends[] =
		{
			Tile | K::DepthTest | (uint32_t)SoftwareBlend::Solid | K::DepthWrite,
			Tile | K::DepthTest | (uint32_t)SoftwareBlend::Solid | K::DepthWrite | K::AlphaTest,
			Tile | K::DepthTest | (uint32_t)SoftwareBlend::Translucent,
			Tile | K::DepthTest | (uint32_t)SoftwareBlend::Modulated | K::AlphaTest,
			Tile | K::DepthTest | (uint32_t)SoftwareBlend::Highlighted,
			Tile | (uint32_t)SoftwareBlend::SubpixelFont | K::AlphaTest
		};
		for (uint32_t blend : blends)
		{
			list.Add(blend);
			list.Add(blend | K::NoSmooth);
			list.Add(blend | K::Clamp);
			list.Add(blend | K::NoSmooth | K::Clamp);
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
