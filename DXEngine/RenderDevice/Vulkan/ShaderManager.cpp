
#include "Precomp.h"
#include "ShaderManager.h"
#include "Utils/Logger.h"
#include "FileResource.h"
#include "VulkanRenderDevice.h"
#include <dxgpu/vulkanbuilders.h>

ShaderManager::ShaderManager(VulkanRenderDevice* renderer)
{
	GLSLCompiler::Init();

	// Without descriptor indexing the scene shaders sample four bound samplers
	// instead of indexing one unbounded array.
	std::string sceneDefines = renderer->SupportsBindless ?
		"#extension GL_EXT_nonuniform_qualifier : enable\r\n#define BINDLESS_TEXTURES\r\n" :
		std::string();
	if (!RenderDevice::DarkClamp)
		sceneDefines += "#define NO_DARKCLAMP\r\n";
	if (RenderDevice::MacroTwoPass)
		sceneDefines += "#define MACRO_TWO_PASS\r\n";
	if (RenderDevice::DetailBands)
		sceneDefines += "#define DETAIL_BANDS\r\n";
	auto compileScene = [&](SceneShaders& shaders, const std::string& defines) {
		shaders.VertexShader = GLSLCompiler()
			.Type(ShaderType::Vertex)
			.AddSource("shaders/Scene.vert", LoadShaderCode("shaders/Scene.vert", defines))
			.Compile(renderer->Device.get());

		shaders.FragmentShader = GLSLCompiler()
			.Type(ShaderType::Fragment)
			.AddSource("shaders/Scene.frag", LoadShaderCode("shaders/Scene.frag", defines))
			.Compile(renderer->Device.get());

		shaders.FragmentShaderAlphaTest = GLSLCompiler()
			.Type(ShaderType::Fragment)
			.AddSource("shaders/Scene.frag", LoadShaderCode("shaders/Scene.frag", defines + "#define ALPHATEST\r\n"))
			.Compile(renderer->Device.get());
	};

	// A mirror's frame is clipped at the mirror's plane by a clip distance.
	// A device without shaderClipDistance (found but not required) clips it
	// by a discard, as the GL device's twins do on ES without
	// EXT_clip_cull_distance: shaders of their own for the mirror's frames
	// alone, so no other draw has a discard, which costs a tile-based GPU its
	// hidden-surface removal.
	if (renderer->Device.get()->EnabledFeatures.Features.shaderClipDistance)
	{
		compileScene(Scene, sceneDefines);
	}
	else
	{
		compileScene(Scene, sceneDefines + "#define NO_CLIP\r\n");
		compileScene(SceneClip, sceneDefines + "#define CLIP_DISCARD\r\n");
		LogMessage("Vulkan: no shaderClipDistance; a mirror's plane is clipped by a discard, in its frame alone");
	}

	Postprocess.VertexShader = GLSLCompiler()
		.Type(ShaderType::Vertex)
		.AddSource("shaders/PPStep.vert", LoadShaderCode("shaders/PPStep.vert"))
		.Compile(renderer->Device.get());

	static const char* transferFunctions[2] = { nullptr, "HDR_MODE" };
	static const char* gammaModes[2] = { "GAMMA_MODE_D3D9", "GAMMA_MODE_XOPENGL" };
	static const char* colorModes[2] = { nullptr, "COLOR_CORRECT" };
	for (int i = 0; i < 8; i++)
	{
		std::string defines;
		if (transferFunctions[i & 1]) defines += std::string("#define ") + transferFunctions[i & 1] + "\r\n";
		if (gammaModes[(i >> 1) & 1]) defines += std::string("#define ") + gammaModes[(i >> 1) & 1] + "\r\n";
		if (colorModes[(i >> 2) & 1]) defines += std::string("#define ") + colorModes[(i >> 2) & 1] + "\r\n";

		Postprocess.FragmentPresentShader[i] = GLSLCompiler()
			.Type(ShaderType::Fragment)
			.AddSource("shaders/Present.frag", LoadShaderCode("shaders/Present.frag", defines))
			.Compile(renderer->Device.get());
	}

	Bloom.Extract = GLSLCompiler()
		.Type(ShaderType::Fragment)
		.AddSource("shaders/BloomExtract.frag", LoadShaderCode("shaders/BloomExtract.frag"))
		.Compile(renderer->Device.get());

	Bloom.Combine = GLSLCompiler()
		.Type(ShaderType::Fragment)
		.AddSource("shaders/BloomCombine.frag", LoadShaderCode("shaders/BloomCombine.frag"))
		.Compile(renderer->Device.get());

	Bloom.BlurVertical = GLSLCompiler()
		.Type(ShaderType::Fragment)
		.AddSource("shaders/BlurVertical.frag", LoadShaderCode("shaders/Blur.frag", "#define BLUR_VERTICAL"))
		.Compile(renderer->Device.get());

	Bloom.BlurHorizontal = GLSLCompiler()
		.Type(ShaderType::Fragment)
		.AddSource("shaders/BlurHorizontal.frag", LoadShaderCode("shaders/Blur.frag", "#define BLUR_HORIZONTAL"))
		.Compile(renderer->Device.get());
}

ShaderManager::~ShaderManager()
{
	GLSLCompiler::Deinit();
}

std::string ShaderManager::LoadShaderCode(const std::string& filename, const std::string& defines)
{
	const char* shaderversion = R"(
		#version 450
		#extension GL_ARB_separate_shader_objects : enable
	)";
	return shaderversion + defines + "\r\n#line 1\r\n" + FileResource::readAllText(filename);
}
