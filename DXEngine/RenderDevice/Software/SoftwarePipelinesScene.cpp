
#include "Precomp.h"
#include "SoftwarePipelines.h"
#include <unordered_map>

namespace
{
	namespace K = SoftwarePipelineKey;

	// The draws a world's frame is made of: the level's surfaces, their light
	// maps, detail textures and fog maps; meshes, their vertex fog; decals
	constexpr uint32_t Opaque = (uint32_t)SoftwareBlend::Solid | K::ColorWrite | K::DepthWrite | K::DepthTest;
	constexpr uint32_t Translucent = (uint32_t)SoftwareBlend::Translucent | K::ColorWrite | K::DepthTest;
	constexpr uint32_t Modulated = (uint32_t)SoftwareBlend::Modulated | K::ColorWrite | K::DepthTest;

	template<uint32_t Key>
	constexpr SoftwarePipelines::Entry Pipeline() { return { Key, &SoftwareDrawPrimitive<Key, false> }; }

	const SoftwarePipelines::Entry Scene[] =
	{
		Pipeline<Opaque | K::Shader(1)>(),
		Pipeline<Opaque | K::Shader(1 | 4)>(),
		Pipeline<Opaque | K::Shader(1 | 8)>(),
		Pipeline<Opaque | K::AlphaTest | K::Shader(1)>(),
		Pipeline<Opaque | K::Shader(0)>(),
		Pipeline<Opaque | K::Shader(16)>(),
		Pipeline<Opaque | K::AlphaTest | K::Shader(0)>(),
		Pipeline<Translucent | K::Shader(0)>(),
		Pipeline<Modulated | K::Shader(0)>(),
	};
}

const SoftwarePipelines::Entry* SoftwarePipelines::ScenePipelines(int& count)
{
	count = (int)(sizeof(Scene) / sizeof(Scene[0]));
	return Scene;
}

SoftwarePipeline SoftwarePipelines::General()
{
	return &SoftwareDrawPrimitive<0, true>;
}

SoftwarePipeline SoftwarePipelines::Select(const SoftwareDraw& draw, bool rectangle, bool generalOnly)
{
	static const std::unordered_map<uint32_t, SoftwarePipeline> table = []()
	{
		std::unordered_map<uint32_t, SoftwarePipeline> t;
		int count;
		const Entry* entries = ScenePipelines(count);
		for (int i = 0; i < count; i++)
			t[entries[i].Key] = entries[i].Pipeline;
		entries = TilePipelines(count);
		for (int i = 0; i < count; i++)
			t[entries[i].Key] = entries[i].Pipeline;
		return t;
	}();

	if (generalOnly)
		return General();
	auto it = table.find(SoftwarePipelineKey::Of(draw, rectangle));
	return it != table.end() ? it->second : General();
}
