
#include "Precomp.h"
#include "SoftwarePipelines.h"

namespace
{
	namespace K = SoftwarePipelineKey;

	// The canvas's tiles: the UI and its text, in each style
	constexpr uint32_t Tile = K::Rectangle | K::ColorWrite | K::DepthTest;

	template<uint32_t Key>
	constexpr SoftwarePipelines::Entry Pipeline() { return { Key, &SoftwareDrawPrimitive<Key, false> }; }

	const SoftwarePipelines::Entry Tiles[] =
	{
		Pipeline<Tile | (uint32_t)SoftwareBlend::Solid | K::DepthWrite | K::Clamp>(),
		Pipeline<Tile | (uint32_t)SoftwareBlend::Solid | K::DepthWrite | K::AlphaTest | K::Clamp>(),
		Pipeline<Tile | (uint32_t)SoftwareBlend::Translucent | K::Clamp>(),
		Pipeline<Tile | (uint32_t)SoftwareBlend::Translucent | K::NoSmooth | K::Clamp>(),
		Pipeline<Tile | (uint32_t)SoftwareBlend::Modulated | K::AlphaTest | K::Clamp>(),
	};
}

const SoftwarePipelines::Entry* SoftwarePipelines::TilePipelines(int& count)
{
	count = (int)(sizeof(Tiles) / sizeof(Tiles[0]));
	return Tiles;
}
