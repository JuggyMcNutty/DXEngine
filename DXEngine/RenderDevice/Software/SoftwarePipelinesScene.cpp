
#include "Precomp.h"
#include "SoftwarePipelines.h"
#include <unordered_map>
#include <utility>

namespace
{
	namespace K = SoftwarePipelineKey;

	constexpr uint32_t Opaque = (uint32_t)SoftwareBlend::Solid | K::ColorWrite | K::DepthWrite | K::DepthTest;
	constexpr uint32_t Translucent = (uint32_t)SoftwareBlend::Translucent | K::ColorWrite | K::DepthTest;
	constexpr uint32_t Modulated = (uint32_t)SoftwareBlend::Modulated | K::ColorWrite | K::DepthTest;

	// The draws a world's frame is made of, each key once: the level's
	// surfaces, solid, masked, translucent or modulated, with their light
	// maps, detail textures, fog maps and macro textures, the light maps of
	// the 1x light mode too; meshes, their vertex fog, the brighter light
	// mode's; what draws depth alone (an invisible surface)
	constexpr SoftwarePipelines::KeyList MakeSceneKeys()
	{
		SoftwarePipelines::KeyList list;
		const uint32_t blends[] = { Opaque, Opaque | K::AlphaTest, Translucent, Modulated, Modulated | K::AlphaTest };
		const uint32_t surfaces[] = { 0, 1, 1 | 4, 1 | 8, 4, 8 };
		for (uint32_t blend : blends)
		{
			for (uint32_t shader : surfaces)
				list.Add(blend | K::Shader(shader));
		}
		const uint32_t lit[] = { Opaque, Opaque | K::AlphaTest, Translucent };
		for (uint32_t blend : lit)
		{
			list.Add(blend | K::Shader(1 | 64));
			list.Add(blend | K::Shader(1 | 4 | 64));
			list.Add(blend | K::Shader(1 | 8 | 64));
		}
		list.Add(Opaque | K::Shader(1 | 2));
		list.Add(Opaque | K::Shader(1 | 2 | 4));
		list.Add(Opaque | K::Shader(1 | 2 | 8));
		const uint32_t meshes[] = { 16, 32, 16 | 32 };
		for (uint32_t shader : meshes)
		{
			list.Add(Opaque | K::Shader(shader));
			list.Add(Opaque | K::AlphaTest | K::Shader(shader));
		}
		list.Add((uint32_t)SoftwareBlend::Solid | K::DepthWrite | K::DepthTest);
		list.Add((uint32_t)SoftwareBlend::Translucent | K::DepthWrite | K::DepthTest);
		return list;
	}

	constexpr SoftwarePipelines::KeyList SceneKeys = MakeSceneKeys();

	template<size_t... I>
	constexpr std::array<SoftwarePipelines::Entry, sizeof...(I)> Instantiate(std::index_sequence<I...>)
	{
		return { SoftwarePipelines::Entry{ SceneKeys.Keys[I], &SoftwareDrawPrimitive<SceneKeys.Keys[I], false> }... };
	}

	const auto Scene = Instantiate(std::make_index_sequence<SceneKeys.Count>());
}

const SoftwarePipelines::Entry* SoftwarePipelines::ScenePipelines(int& count)
{
	count = (int)Scene.size();
	return Scene.data();
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
