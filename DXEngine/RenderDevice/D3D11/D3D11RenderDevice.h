#pragma once

#include "RenderDevice/RenderDevice.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"

#include "Math/vec.h"
#include "Math/mat.h"
#include "Utils/ComPtr.h"
#include <set>
#include <string>

#include "D3D11TextureManager.h"
#include "D3D11UploadManager.h"
#include "D3D11CachedTexture.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <dxgi1_5.h>

struct D3D11SceneVertex
{
	uint32_t Flags;
	vec3 Position;
	vec2 TexCoord;
	vec2 TexCoord2;
	vec2 TexCoord3;
	vec2 TexCoord4;
	vec4 Color;
};

struct D3D11ScenePushConstants
{
	mat4 ObjectToProjection;
	mat4 ObjectToView;
	vec4 NearClip;
};

struct D3D11PresentPushConstants
{
	float Contrast;
	float Saturation;
	float Padding; // GammaCorrection kept at 16 bytes
	float HdrScale;
	vec4 GammaCorrection;
};

struct D3D11BloomPushConstants
{
	float SampleWeights[8];
};

class D3D11RenderDevice : public RenderDevice
{
public:
	D3D11RenderDevice(Widget* viewport);
	~D3D11RenderDevice();

	// To do: port this
	bool Init(int NewX, int NewY);
	bool SetRes(int NewX, int NewY);
	void Exit();

	void Flush(bool AllowPrecache) override;
	void Lock(vec4 FlashScale, vec4 FlashFog, vec4 ScreenClear) override;
	void SetLODBias(float bias) override;
	bool LODBiasChanged = false;
	void Unlock(bool Blit) override;
	void DrawComplexSurface(SceneNode* Frame, SurfaceInfo& Surface, SurfaceFacet& Facet) override;
	void DrawGouraudPolygon(SceneNode* Frame, TextureInfo& Info, const GouraudVertex* Pts, int NumPts, uint32_t PolyFlags) override;
	void DrawTile(SceneNode* Frame, TextureInfo& Info, float X, float Y, float XL, float YL, float U, float V, float UL, float VL, float Z, vec4 Color, vec4 Fog, uint32_t PolyFlags) override;
	void Draw3DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 OrigP, vec3 OrigQ) override;
	void Draw2DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2) override;
	void ClearZ() override;
	void ReadPixels(TextureColor* Pixels) override;
	// The size ReadPixels writes: the scene's at the last Lock, not the
	// window's now (a window resized since overflowed the buffer)
	void LastFrameSize(int& width, int& height) const override { width = CurrentSizeX; height = CurrentSizeY; }
	void EndFlash() override;
	void TakeBloom() override;
	void SetSceneNode(SceneNode* Frame) override;
	void PrecacheTexture(TextureInfo& Info, uint32_t PolyFlags) override;
	bool SupportsTextureFormat(TextureFormat Format) override;
	void UpdateTextureRect(TextureInfo& Info, int U, int V, int UL, int VL) override;


	void SetDebugName(ID3D11Device* obj, const char* name);
	void SetDebugName(ID3D11DeviceChild* obj, const char* name);

	HWND WindowHandle = 0;
	ComPtr<ID3D11Device> Device;
	D3D_FEATURE_LEVEL FeatureLevel = {};
	ComPtr<ID3D11DeviceContext> Context;
	ComPtr<IDXGISwapChain> SwapChain;
	ComPtr<IDXGISwapChain1> SwapChain1;
	ComPtr<ID3D11Debug> DebugLayer;
	ComPtr<ID3D11InfoQueue> InfoQueue;
	ComPtr<ID3D11Texture2D> BackBuffer;
	ComPtr<ID3D11RenderTargetView> BackBufferView;
	bool DxgiSwapChainAllowTearing = false;
	int BufferCount = 2;
	std::set<std::string> SeenDebugMessages;
	int TotalSeenDebugMessages = 0;

	struct PPBlurLevel
	{
		ComPtr<ID3D11Texture2D> VTexture;
		ComPtr<ID3D11RenderTargetView> VTextureRTV;
		ComPtr<ID3D11ShaderResourceView> VTextureSRV;
		ComPtr<ID3D11Texture2D> HTexture;
		ComPtr<ID3D11RenderTargetView> HTextureRTV;
		ComPtr<ID3D11ShaderResourceView> HTextureSRV;
		int Width = 0;
		int Height = 0;
	};

	struct
	{
		ComPtr<ID3D11Texture2D> ColorBuffer;
		ComPtr<ID3D11Texture2D> DepthBuffer;
		ComPtr<ID3D11Texture2D> PPImage[2];
		ComPtr<ID3D11RenderTargetView> ColorBufferView;
		ComPtr<ID3D11DepthStencilView> DepthBufferView;
		ComPtr<ID3D11RenderTargetView> PPImageView[2];
		ComPtr<ID3D11ShaderResourceView> PPImageShaderView[2];
		enum { NumBloomLevels = 4 };
		PPBlurLevel BlurLevels[NumBloomLevels];
		int Width = 0;
		int Height = 0;
		int Multisample = 0;
	} SceneBuffers;

	struct ScenePipelineState
	{
		D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		ID3D11PixelShader* PixelShader = nullptr;
		ComPtr<ID3D11BlendState> BlendState;
		ComPtr<ID3D11DepthStencilState> DepthStencilState;
		float MinDepth = 0.1f;
		float MaxDepth = 1.0f;
	};

	struct
	{
		ComPtr<ID3D11VertexShader> VertexShader;
		ComPtr<ID3D11InputLayout> InputLayout;
		ComPtr<ID3D11Buffer> VertexBuffer;
		ComPtr<ID3D11Buffer> IndexBuffer;
		ComPtr<ID3D11Buffer> ConstantBuffer;
		ComPtr<ID3D11RasterizerState> RasterizerState[2];
		ComPtr<ID3D11PixelShader> PixelShader;
		ComPtr<ID3D11PixelShader> PixelShaderAlphaTest;
		ComPtr<ID3D11SamplerState> Samplers[16];
		ScenePipelineState Pipelines[33];
		ScenePipelineState LinePipeline[2];
	} ScenePass;

	static const int D3D11SceneVertexBufferSize = 16 * 1024;
	static const int SceneIndexBufferSize = 32 * 1024;

	struct DrawBatchEntry
	{
		size_t SceneIndexStart = 0;
		size_t SceneIndexEnd = 0;
		ScenePipelineState* Pipeline = nullptr;
		D3D11CachedTexture* Tex = nullptr;
		D3D11CachedTexture* Lightmap = nullptr;
		D3D11CachedTexture* Detailtex = nullptr;
		D3D11CachedTexture* Macrotex = nullptr;
		uint32_t TexSamplerMode = 0;
		uint32_t DetailtexSamplerMode = 0;
		uint32_t MacrotexSamplerMode = 0;
		float BlendConstants[4] = {};
	} Batch;
	std::vector<DrawBatchEntry> QueuedBatches;
	D3D11CachedTexture* nulltex = nullptr;

	D3D11SceneVertex* SceneVertices = nullptr;
	size_t D3D11SceneVertexPos = 0;

	uint32_t* SceneIndexes = nullptr;
	size_t SceneIndexPos = 0;

	struct
	{
		ComPtr<ID3D11VertexShader> PPStep;
		ComPtr<ID3D11InputLayout> PPStepLayout;
		ComPtr<ID3D11Buffer> PPStepVertexBuffer;
		ComPtr<ID3D11PixelShader> Present[8];
		ComPtr<ID3D11Buffer> PresentConstantBuffer;
		ComPtr<ID3D11Texture2D> DitherTexture;
		ComPtr<ID3D11ShaderResourceView> DitherTextureView;
		// The scene linear and held at its edges, the dither texel for texel and
		// repeated (s0, s1), as the Vulkan and GL presents sample them
		ComPtr<ID3D11SamplerState> LinearClamp;
		ComPtr<ID3D11SamplerState> NearestRepeat;
		ComPtr<ID3D11BlendState> BlendState;
		ComPtr<ID3D11DepthStencilState> DepthStencilState;
		ComPtr<ID3D11RasterizerState> RasterizerState;
	} PresentPass;

	struct
	{
		ComPtr<ID3D11PixelShader> Extract;
		ComPtr<ID3D11PixelShader> Combine;
		ComPtr<ID3D11PixelShader> BlurVertical;
		ComPtr<ID3D11PixelShader> BlurHorizontal;
		ComPtr<ID3D11Buffer> ConstantBuffer;
		ComPtr<ID3D11BlendState> AdditiveBlendState;
		ComPtr<ID3D11SamplerState> LinearClamp;
	} BloomPass;

	std::unique_ptr<D3D11TextureManager> Textures;
	std::unique_ptr<D3D11UploadManager> Uploads;


	struct
	{
		int ComplexSurfaces = 0;
		int GouraudPolygons = 0;
		int Tiles = 0;
		int DrawCalls = 0;
		int Uploads = 0;
		int RectUploads = 0;
		int BuffersUsed = 0;
	} Stats;

private:
	struct ComplexSurfaceInfo
	{
		SurfaceFacet* facet;
		D3D11CachedTexture* tex;
		D3D11CachedTexture* lightmap;
		D3D11CachedTexture* macrotex;
		D3D11CachedTexture* detailtex;
		D3D11CachedTexture* fogmap;
		vec4* editorcolor;
		// The base's and the macro texture's pan and scale (pan U, pan V,
		// UMult, VMult) from the surface's own texture infos, as the Vulkan and
		// GL devices take them: one texture can be both, panned apart, where its
		// cache entry keeps the last asked for.
		vec4 texPanMult;
		vec4 macroPanMult;
	};
	void DrawComplexSurfaceFaces(const ComplexSurfaceInfo& info);

	void ReleaseSwapChainResources();
	bool UpdateSwapChain();
	void SetColorSpace();
	void ResizeSceneBuffers(int width, int height, int multisample);
	void ClearTextureCache();

	void CreatePresentPass();
	void CreateBloomPass();
	void CreateScenePass();
	void ReleaseScenePass();
	void ReleaseBloomPass();
	void ReleasePresentPass();

	void CreateSceneSamplers();
	void ReleaseSceneSamplers();

	void ReleaseSceneBuffers();

	void RunBloomPass();
	void AddBloom();
	// The bloom levels hold this frame's, taken by TakeBloom
	bool BloomTaken = false;
	void BlurStep(ID3D11ShaderResourceView* input, ID3D11RenderTargetView* output, bool vertical);
	float ComputeBlurGaussian(float n, float theta);
	void ComputeBlurSamples(int sampleCount, float blurAmount, float* sampleWeights);

	void SetPipeline(ScenePipelineState* pipeline);
	void SetPipeline(uint32_t polyflags);
	void SetDescriptorSet(uint32_t polyflags, D3D11CachedTexture* tex = nullptr, bool clamp = false);
	void SetDescriptorSet(uint32_t polyflags, const ComplexSurfaceInfo& info);

	void AddDrawBatch();
	void DrawBatches(bool nextBuffer = false);
	void DrawEntry(const DrawBatchEntry& entry);

	void PrintDebugLayerMessages();

	HWND GetWindowHandle();

	struct VertexReserveInfo
	{
		D3D11SceneVertex* vptr;
		uint32_t* iptr;
		uint32_t vpos;
	};

	VertexReserveInfo ReserveVertices(size_t vcount, size_t icount)
	{
		if (!SceneVertices || !SceneIndexes)
			return { nullptr, nullptr, 0 };

		// If buffers are full, flush and wait for room.
		if (D3D11SceneVertexPos + vcount > (size_t)D3D11SceneVertexBufferSize || SceneIndexPos + icount > (size_t)SceneIndexBufferSize)
		{
			// If the request is larger than our buffers we can't draw this.
			if (vcount > (size_t)D3D11SceneVertexBufferSize || icount > (size_t)SceneIndexBufferSize)
				return { nullptr, nullptr, 0 };

			DrawBatches(true);
			// The flush maps the buffers again, each on its own: one may fail
			if (!SceneVertices || !SceneIndexes)
				return { nullptr, nullptr, 0 };
		}

		return { SceneVertices + D3D11SceneVertexPos, SceneIndexes + SceneIndexPos, (uint32_t)D3D11SceneVertexPos };
	}

	void UseVertices(size_t vcount, size_t icount)
	{
		D3D11SceneVertexPos += vcount;
		SceneIndexPos += icount;
	}

	int GetSettingsMultisample();

	ScenePipelineState* GetPipeline(uint32_t PolyFlags);

	void CreateVertexShader(ComPtr<ID3D11VertexShader>& outShader, const std::string& shaderName, ComPtr<ID3D11InputLayout>& outInputLayout, const std::string& inputLayoutName, const std::vector<D3D11_INPUT_ELEMENT_DESC>& elements, const std::string& filename, const std::vector<std::string> defines = {});
	void CreatePixelShader(ComPtr<ID3D11PixelShader>& outShader, const std::string& shaderName, const std::string& filename, const std::vector<std::string> defines = {});
	std::vector<uint8_t> CompileHlsl(const std::string& filename, const std::string& shadertype, const std::vector<std::string> defines = {});

	D3D11PresentPushConstants GetD3D11PresentPushConstants();

	bool VerticesMapped() const { return SceneVertices && SceneIndexes; }
	void MapVertices(bool nextBuffer);
	void UnmapVertices();

	bool UsePrecache = true;
	vec4 FlashScale = vec4(0.0f);
	vec4 FlashFog = vec4(0.0f);
	SceneNode* CurrentFrame = nullptr;
	float Aspect = 0.0f;
	float RProjZ = 0.0f;
	float RFX2 = 0.0f;
	float RFY2 = 0.0f;
	D3D11ScenePushConstants SceneConstants = {};
	D3D11_VIEWPORT SceneViewport = {};

	bool IsLocked = false;
	bool ActiveHdr = false;

	struct
	{
		int Width = 0;
		int Height = 0;
	} DesktopResolution;

	int CurrentSizeX = 0;
	int CurrentSizeY = 0;
};

inline void ThrowIfFailed(HRESULT result, const char* msg) { if (FAILED(result)) throw std::runtime_error(msg); }

inline void D3D11RenderDevice::SetPipeline(ScenePipelineState* pipeline)
{
	if (pipeline != Batch.Pipeline)
	{
		AddDrawBatch();
		Batch.Pipeline = pipeline;
	}
}

inline void D3D11RenderDevice::SetPipeline(uint32_t PolyFlags)
{
	auto pipeline = GetPipeline(PolyFlags);
	if (pipeline != Batch.Pipeline)
	{
		AddDrawBatch();
		Batch.Pipeline = pipeline;
	}
}

inline void D3D11RenderDevice::SetDescriptorSet(uint32_t PolyFlags, D3D11CachedTexture* tex, bool clamp)
{
	if (!tex) tex = nulltex;

	uint32_t samplermode = 0;
	if (PolyFlags & PF_NoSmooth) samplermode |= 1;
	if (clamp) samplermode |= 2;
	samplermode |= (tex->DummyMipmapCount << 2);

	if (Batch.Tex != tex || Batch.TexSamplerMode != samplermode || Batch.Lightmap != nulltex || Batch.Detailtex != nulltex || Batch.DetailtexSamplerMode != 0 || Batch.Macrotex != nulltex || Batch.MacrotexSamplerMode != 0)
	{
		AddDrawBatch();
		Batch.Tex = tex;
		Batch.Lightmap = nulltex;
		Batch.Detailtex = nulltex;
		Batch.Macrotex = nulltex;
		Batch.TexSamplerMode = samplermode;
		Batch.DetailtexSamplerMode = 0;
		Batch.MacrotexSamplerMode = 0;
	}
}

inline void D3D11RenderDevice::SetDescriptorSet(uint32_t PolyFlags, const ComplexSurfaceInfo& info)
{
	uint32_t samplermode = 0;
	if (PolyFlags & PF_NoSmooth) samplermode |= 1;
	samplermode |= (info.tex->DummyMipmapCount << 2);

	int detailsamplermode = info.detailtex->DummyMipmapCount << 2;
	int macrosamplermode = info.macrotex->DummyMipmapCount << 2;

	if (Batch.Tex != info.tex || Batch.TexSamplerMode != samplermode || Batch.Lightmap != info.lightmap || Batch.Detailtex != info.detailtex || Batch.DetailtexSamplerMode != detailsamplermode || Batch.Macrotex != info.macrotex || Batch.MacrotexSamplerMode != macrosamplermode)
	{
		AddDrawBatch();
		Batch.Tex = info.tex;
		Batch.Lightmap = info.lightmap;
		Batch.Detailtex = info.detailtex;
		Batch.Macrotex = info.macrotex;
		Batch.TexSamplerMode = samplermode;
		Batch.DetailtexSamplerMode = detailsamplermode;
		Batch.MacrotexSamplerMode = macrosamplermode;
	}
}
