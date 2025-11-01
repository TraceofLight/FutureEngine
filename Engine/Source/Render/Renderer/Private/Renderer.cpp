#include "pch.h"
#include <algorithm>
#include "Component/Mesh/Public/StaticMesh.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Component/Public/AmbientLightComponent.h"
#include "Component/Public/DecalComponent.h"
#include "Component/Public/DirectionalLightComponent.h"
#include "Component/Public/EditorIconComponent.h"
#include "Component/Public/HeightFogComponent.h"
#include "Component/Public/PointLightComponent.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Component/Public/SpotLightComponent.h"
#include "Component/Public/UUIDTextComponent.h"
#include "Editor/Public/Camera.h"
#include "Editor/Public/Editor.h"
#include "Global/Octree.h"
#include "Global/Octree.h"
#include "Level/Public/Level.h"
#include "Manager/UI/Public/UIManager.h"
#include "Manager/UI/Public/ViewportManager.h"
#include "Manager/UI/Public/ViewportManager.h"
#include "Optimization/Public/OcclusionCuller.h"
#include "Render/RenderPass/Public/BillboardPass.h"
#include "Render/RenderPass/Public/EditorIconPass.h"
#include "Render/RenderPass/Public/ClusteredRenderingGridPass.h"
#include "Render/RenderPass/Public/ClusteredRenderingGridPass.h"
#include "Render/RenderPass/Public/DecalPass.h"
#include "Render/RenderPass/Public/FXAAPass.h"
#include "Render/RenderPass/Public/FogPass.h"
#include "Render/RenderPass/Public/HitProxyPass.h"
#include "Render/RenderPass/Public/LightPass.h"
#include "Render/RenderPass/Public/PointLightPass.h"
#include "Render/RenderPass/Public/RenderPass.h"
#include "Render/RenderPass/Public/SceneDepthPass.h"
#include "Render/RenderPass/Public/ShadowMapFilterPass.h"
#include "Render/RenderPass/Public/ShadowMapPass.h"
#include "Render/RenderPass/Public/StaticMeshPass.h"
#include "Render/RenderPass/Public/TextPass.h"
#include "Render/HitProxy/Public/HitProxy.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/UI/Overlay/Public/D2DOverlayManager.h"
#include "Render/UI/Overlay/Public/StatOverlay.h"
#include "Render/UI/Viewport/Public/Viewport.h"
#include "Render/UI/Viewport/Public/ViewportClient.h"

IMPLEMENT_SINGLETON_CLASS(URenderer, UObject)

URenderer::URenderer() = default;

URenderer::~URenderer() = default;


void URenderer::Init(HWND InWindowHandle)
{
	DeviceResources = new UDeviceResources(InWindowHandle);
	Pipeline = new UPipeline(GetDeviceContext());
	ViewportClient = new FViewport();
	
	// 렌더링 상태 및 리소스 생성
	CreateDepthStencilState();
	CreateBlendState();
	CreateSamplerState();
	CreateDefaultShader();
	CreateTextureShader();
	CreateDecalShader();
	CreateFogShader();
	CreateConstantBuffers();
	CreateFXAAShader();
	CreateStaticMeshShader();
	CreateGizmoShader();
	CreateClusteredRenderingGrid();
	CreateDepthOnlyShader();
	CreatePointLightShadowShader();
	CreateHitProxyShader();

	//ViewportClient->InitializeLayout(DeviceResources->GetViewportInfo());

	ShadowMapPass = new FShadowMapPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		DepthOnlyVertexShader, DepthOnlyPixelShader, DepthOnlyInputLayout,
		PointLightShadowVS, PointLightShadowPS, PointLightShadowInputLayout);
	RenderPasses.Add(ShadowMapPass);

	ShadowMapFilterPass = new FShadowMapFilterPass(ShadowMapPass, Pipeline);
	// ShadowMapFilterPass = new FShadowMapFilterPass(ShadowMapPass, Pipeline, GaussianTextureFilterRowCS, GaussianTextureFilterColumnCS);
	// ShadowMapFilterPass = new FShadowMapFilterPass(ShadowMapPass, Pipeline, BoxTextureFilterRowCS, BoxTextureFilterColumnCS);
	RenderPasses.Add(ShadowMapFilterPass);

	LightPass = new FLightPass(Pipeline, ConstantBufferViewProj, GizmoInputLayout, GizmoVS, GizmoPS, DefaultDepthStencilState);
	RenderPasses.Add(LightPass);

	FStaticMeshPass* StaticMeshPass = new FStaticMeshPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		UberLitVertexShader, UberLitPixelShader, UberLitInputLayout, DefaultDepthStencilState);
	RenderPasses.Add(StaticMeshPass);

	FDecalPass* DecalPass = new FDecalPass(Pipeline, ConstantBufferViewProj,
		DecalVertexShader, DecalPixelShader, DecalInputLayout, DecalDepthStencilState, AlphaBlendState);
	RenderPasses.Add(DecalPass);
	
	FBillboardPass* BillboardPass = new FBillboardPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		TextureVertexShader, TexturePixelShader, TextureInputLayout, DefaultDepthStencilState, AlphaBlendState);
	RenderPasses.Add(BillboardPass);

	FEditorIconPass* EditorIconPass = new FEditorIconPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		TextureVertexShader, TexturePixelShader, TextureInputLayout, DefaultDepthStencilState, AlphaBlendState);
	RenderPasses.Add(EditorIconPass);

	FTextPass* TextPass = new FTextPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels);
	RenderPasses.Add(TextPass);

	FFogPass* FogPass = new FFogPass(Pipeline, ConstantBufferViewProj,
		FogVertexShader, FogPixelShader, FogInputLayout, DefaultDepthStencilState, AlphaBlendState);
	RenderPasses.Add(FogPass);

	ClusteredRenderingGridPass = new FClusteredRenderingGridPass(Pipeline, ConstantBufferViewProj,
		ClusteredRenderingGridVS, ClusteredRenderingGridPS, ClusteredRenderingGridInputLayout, DefaultDepthStencilState, AlphaBlendState);
	RenderPasses.Add(ClusteredRenderingGridPass);

	FSceneDepthPass* SceneDepthPass = new FSceneDepthPass(Pipeline, ConstantBufferViewProj, DisabledDepthStencilState);
	RenderPasses.Add(SceneDepthPass);

	// UPipeline* InPipeline, UDeviceResources* InDeviceResources, ID3D11VertexShader* InVS,
	// ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11SamplerState* InSampler
	FXAAPass = new FFXAAPass(Pipeline, DeviceResources, FXAAVertexShader, FXAAPixelShader, FXAAInputLayout, FXAASamplerState);
	//RenderPasses.Add(FXAAPass);

	HitProxyPass = new FHitProxyPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		HitProxyVS, HitProxyPS, HitProxyInputLayout, DefaultDepthStencilState);
}

void URenderer::Release()
{
	ReleaseConstantBuffers();
	ReleaseDefaultShader();
	ReleaseDepthStencilState();
	ReleaseBlendState();
	ReleaseSamplerState();
	FRenderResourceFactory::ReleaseRasterizerState();
	for (auto& RenderPass : RenderPasses)
	{
		RenderPass->Release();
		SafeDelete(RenderPass);
	}
	FXAAPass->Release();
	SafeDelete(FXAAPass);

	if (HitProxyPass)
	{
		HitProxyPass->Release();
		SafeDelete(HitProxyPass);
	}

	SafeDelete(ViewportClient);
	SafeDelete(Pipeline);
	SafeDelete(DeviceResources);
}

void URenderer::CreateDepthStencilState()
{
	// 3D Default Depth Stencil (Depth O, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DefaultDescription = {};
	DefaultDescription.DepthEnable = TRUE;
	DefaultDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DefaultDescription.DepthFunc = D3D11_COMPARISON_LESS;
	DefaultDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DefaultDescription, &DefaultDepthStencilState);

	// Decal Depth Stencil (Depth Read, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DecalDescription = {};
	DecalDescription.DepthEnable = TRUE;
	DecalDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DecalDescription.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	DecalDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DecalDescription, &DecalDepthStencilState);


	// Disabled Depth Stencil (Depth X, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DisabledDescription = {};
	DisabledDescription.DepthEnable = FALSE;
	DisabledDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DisabledDescription, &DisabledDepthStencilState);
}

void URenderer::CreateBlendState()
{
    // Alpha Blending
    D3D11_BLEND_DESC BlendDesc = {};
    BlendDesc.RenderTarget[0].BlendEnable = TRUE;
    BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    GetDevice()->CreateBlendState(&BlendDesc, &AlphaBlendState);

    // Additive Blending
    D3D11_BLEND_DESC AdditiveBlendDesc = {};
    AdditiveBlendDesc.RenderTarget[0].BlendEnable = TRUE;
    AdditiveBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    AdditiveBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    AdditiveBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    AdditiveBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    AdditiveBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    AdditiveBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    AdditiveBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    GetDevice()->CreateBlendState(&AdditiveBlendDesc, &AdditiveBlendState);
}

void URenderer::CreateSamplerState()
{
	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	GetDevice()->CreateSamplerState(&samplerDesc, &DefaultSampler);

	// Comparison sampler for shadow mapping (PCF)
	D3D11_SAMPLER_DESC shadowSamplerDesc = {};
	shadowSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;  // PCF filtering
	shadowSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	shadowSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	shadowSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	shadowSamplerDesc.BorderColor[0] = 1.0f;  // Outside shadow map = fully lit
	shadowSamplerDesc.BorderColor[1] = 1.0f;
	shadowSamplerDesc.BorderColor[2] = 1.0f;
	shadowSamplerDesc.BorderColor[3] = 1.0f;
	shadowSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;  // Shadow test
	shadowSamplerDesc.MinLOD = 0;
	shadowSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	GetDevice()->CreateSamplerState(&shadowSamplerDesc, &ShadowComparisonSampler);

	// Sampler for variance shadow mapping (VSM)
	D3D11_SAMPLER_DESC VarianceShadowSamplerDesc = {};
	VarianceShadowSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	VarianceShadowSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	VarianceShadowSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	VarianceShadowSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	VarianceShadowSamplerDesc.BorderColor[0] = 1.0f;
	VarianceShadowSamplerDesc.BorderColor[1] = 1.0f;
	VarianceShadowSamplerDesc.BorderColor[2] = 1.0f;
	VarianceShadowSamplerDesc.BorderColor[3] = 1.0f;
	VarianceShadowSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	VarianceShadowSamplerDesc.MinLOD = 0;
	VarianceShadowSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	VarianceShadowSamplerDesc.MipLODBias = 0;
	GetDevice()->CreateSamplerState(&VarianceShadowSamplerDesc, &VarianceShadowSampler);

	// Sampler for point light Atlas sampling
	D3D11_SAMPLER_DESC PointShadowSamplerDesc = {};
	PointShadowSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	PointShadowSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	PointShadowSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	PointShadowSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	PointShadowSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	PointShadowSamplerDesc.MinLOD = 0;
	PointShadowSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	GetDevice()->CreateSamplerState(&PointShadowSamplerDesc, &PointShadowSampler);
}

void URenderer::RegisterShaderReloadCache(const std::filesystem::path& ShaderPath, ShaderUsage Usage)
{
	std::error_code ErrorCode; // 파일 API는 대개 문제가 생겼을 때 exception 발생을 피하고 ErrorCode에 문제 내용 저장

	// weakly_canonical: 경로의 일부가 실제로 없어도 존재하는 구간까지는 실제 파일시스템을 참고해 절대경로로 정규화 + 나머지는 문자열로 정규화
	std::filesystem::path NormalizedPath = std::filesystem::weakly_canonical(ShaderPath, ErrorCode);
	if (ErrorCode)
	{
		// 1차 FALLBACK: 기준 경로 오류 대비. 현재 작업 디렉터리를 기준으로 재시도.
		ErrorCode.clear();
		NormalizedPath = std::filesystem::weakly_canonical(std::filesystem::current_path() / ShaderPath, ErrorCode);
	}
	if (ErrorCode)
	{
		// 2차 FALLBACK: 그냥 원본 경로 사용.
		ErrorCode.clear();
		NormalizedPath = ShaderPath;
	}

	const std::wstring Key = NormalizedPath.generic_wstring(); // 경로 구분자를 '/'로 통일한 유니코드 문자열
	ShaderFileUsageMap.FindOrAdd(Key).Add(Usage);

	// 정규화된 경로(NormalizedPath)의 마지막 수정 시간 캐싱
	const auto LastWriteTime = std::filesystem::last_write_time(NormalizedPath, ErrorCode);
	if (!ErrorCode)
	{
		ShaderFileLastWriteTimeMap[Key] = LastWriteTime;
		return;
	}
}

void URenderer::CreateDefaultShader()
{
	const std::wstring ShaderFilePathString = L"Asset/Shader/SampleShader.hlsl";
	const std::filesystem::path ShaderPath(ShaderFilePathString);

	TArray<D3D11_INPUT_ELEMENT_DESC> DefaultLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(ShaderFilePathString, DefaultLayout, &DefaultVertexShader, &DefaultInputLayout);
	FRenderResourceFactory::CreatePixelShader(ShaderFilePathString, &DefaultPixelShader);
	Stride = sizeof(FNormalVertex);

	RegisterShaderReloadCache(ShaderPath, ShaderUsage::DEFAULT);
}

void URenderer::CreateTextureShader()
{
	const std::wstring VSFilePathString = L"Asset/Shader/TextureVS.hlsl";
	const std::filesystem::path VSPath(VSFilePathString);
	const std::wstring PSFilePathString = L"Asset/Shader/TexturePS.hlsl";
	const std::filesystem::path PSPath(PSFilePathString);

	TArray<D3D11_INPUT_ELEMENT_DESC> TextureLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(VSFilePathString, TextureLayout, &TextureVertexShader, &TextureInputLayout);
	FRenderResourceFactory::CreatePixelShader(PSFilePathString, &TexturePixelShader);

	RegisterShaderReloadCache(VSPath, ShaderUsage::TEXTURE);
	RegisterShaderReloadCache(PSPath, ShaderUsage::TEXTURE);
}

void URenderer::CreateDecalShader()
{
	const std::wstring ShaderFilePathString = L"Asset/Shader/DecalShader.hlsl";
	const std::filesystem::path ShaderPath(ShaderFilePathString);

	TArray<D3D11_INPUT_ELEMENT_DESC> DecalLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(ShaderFilePathString, DecalLayout, &DecalVertexShader, &DecalInputLayout);
	FRenderResourceFactory::CreatePixelShader(ShaderFilePathString, &DecalPixelShader);

	RegisterShaderReloadCache(ShaderPath, ShaderUsage::DECAL);
}

void URenderer::CreateFogShader()
{
	const std::wstring ShaderFilePathString = L"Asset/Shader/HeightFogShader.hlsl";
	const std::filesystem::path ShaderPath(ShaderFilePathString);

	TArray<D3D11_INPUT_ELEMENT_DESC> FogLayout =
	{
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(ShaderFilePathString, FogLayout, &FogVertexShader, &FogInputLayout);
	FRenderResourceFactory::CreatePixelShader(ShaderFilePathString, &FogPixelShader);

	RegisterShaderReloadCache(ShaderPath, ShaderUsage::FOG);
}

void URenderer::CreateFXAAShader()
{
	const std::wstring ShaderFilePathString = L"Asset/Shader/FXAAShader.hlsl";
	const std::filesystem::path ShaderPath(ShaderFilePathString);

	TArray<D3D11_INPUT_ELEMENT_DESC> FXAALayout =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(ShaderFilePathString, FXAALayout, &FXAAVertexShader, &FXAAInputLayout);
	FRenderResourceFactory::CreatePixelShader(ShaderFilePathString, &FXAAPixelShader);
	
	FXAASamplerState = FRenderResourceFactory::CreateFXAASamplerState();

	RegisterShaderReloadCache(ShaderPath, ShaderUsage::FXAA);
}

void URenderer::CreateStaticMeshShader()
{
	const std::wstring ShaderFilePathString = L"Asset/Shader/UberLit.hlsl";
	const std::filesystem::path ShaderPath(ShaderFilePathString);

	TArray<D3D11_INPUT_ELEMENT_DESC> ShaderMeshLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Tangent),  D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	
	// Compile Lambert variant (default)
	TArray<D3D_SHADER_MACRO> LambertMacros = {
		{ "LIGHTING_MODEL_LAMBERT", "1" },
		{ nullptr, nullptr }
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(ShaderFilePathString, ShaderMeshLayout, &UberLitVertexShader, &UberLitInputLayout, "Uber_VS", LambertMacros.GetData());
	FRenderResourceFactory::CreatePixelShader(ShaderFilePathString, &UberLitPixelShader, "Uber_PS", LambertMacros.GetData());
	
	// Compile Gouraud variant
	TArray<D3D_SHADER_MACRO> GouraudMacros = {
		{ "LIGHTING_MODEL_GOURAUD", "1" },
		{ nullptr, nullptr }
	};
	ID3D11InputLayout* GouraudInputLayout = nullptr;
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(ShaderFilePathString, ShaderMeshLayout, &UberLitVertexShaderGouraud, &GouraudInputLayout, "Uber_VS", GouraudMacros.GetData());
	SafeRelease(GouraudInputLayout);
	FRenderResourceFactory::CreatePixelShader(ShaderFilePathString, &UberLitPixelShaderGouraud, "Uber_PS", GouraudMacros.GetData());
	
	// Compile Phong (Blinn-Phong) variant
	TArray<D3D_SHADER_MACRO> PhongMacros = {
		{ "LIGHTING_MODEL_BLINNPHONG", "1" },
		{ nullptr, nullptr }
	};
	FRenderResourceFactory::CreatePixelShader(ShaderFilePathString, &UberLitPixelShaderBlinnPhong, "Uber_PS", PhongMacros.GetData());

	TArray<D3D_SHADER_MACRO> WorldNormalViewMacros = {
		{ "LIGHTING_MODEL_NORMAL", "1" },
		{ nullptr, nullptr }
	};
	FRenderResourceFactory::CreatePixelShader(ShaderFilePathString, &UberLitPixelShaderWorldNormal, "Uber_PS", WorldNormalViewMacros.GetData());

	RegisterShaderReloadCache(ShaderPath, ShaderUsage::STATICMESH);
}

void URenderer::CreateGizmoShader()
{
	const std::wstring ShaderFilePathString = L"Asset/Shader/GizmoLine.hlsl";
	const std::filesystem::path ShaderPath(ShaderFilePathString);

	// Create shaders
	TArray<D3D11_INPUT_ELEMENT_DESC> LayoutDesc =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	FRenderResourceFactory::CreateVertexShaderAndInputLayout(ShaderFilePathString, LayoutDesc, &GizmoVS, &GizmoInputLayout);
	FRenderResourceFactory::CreatePixelShader(ShaderFilePathString, &GizmoPS);

	RegisterShaderReloadCache(ShaderPath, ShaderUsage::GIZMO);
}

void URenderer::CreateClusteredRenderingGrid()
{
	const std::wstring ShaderFilePathString = L"Asset/Shader/ClusteredRenderingGrid.hlsl";
	const std::filesystem::path ShaderPath(ShaderFilePathString);

	TArray<D3D11_INPUT_ELEMENT_DESC> InputLayout =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(ShaderFilePathString, InputLayout, &ClusteredRenderingGridVS, &ClusteredRenderingGridInputLayout);
	FRenderResourceFactory::CreatePixelShader(ShaderFilePathString, &ClusteredRenderingGridPS);

	RegisterShaderReloadCache(ShaderPath, ShaderUsage::CLUSTERED_RENDERING_GRID);
}

void URenderer::CreateDepthOnlyShader()
{
	const std::wstring ShaderFilePathString = L"Asset/Shader/DepthOnly.hlsl";
	const std::filesystem::path ShaderPath(ShaderFilePathString);

	// Depth-only VS uses FNormalVertex layout (same as UberLit)
	TArray<D3D11_INPUT_ELEMENT_DESC> InputLayout =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	FRenderResourceFactory::CreateVertexShaderAndInputLayout(ShaderFilePathString, InputLayout, &DepthOnlyVertexShader, &DepthOnlyInputLayout);
	FRenderResourceFactory::CreatePixelShader(ShaderFilePathString, &DepthOnlyPixelShader);
	// No pixel shader needed for depth-only rendering

	RegisterShaderReloadCache(ShaderPath, ShaderUsage::SHADOWMAP);
}

void URenderer::CreatePointLightShadowShader()
{
	const std::wstring ShaderFilePathString = L"Asset/Shader/LinearDepthOnly.hlsl";
	const std::filesystem::path ShaderPath(ShaderFilePathString);

	// Same layout as depth-only shader
	TArray<D3D11_INPUT_ELEMENT_DESC> InputLayout =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	// Create vertex shader and input layout
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(ShaderFilePathString, InputLayout, &PointLightShadowVS, &PointLightShadowInputLayout);

	// Create pixel shader (for linear distance output)
	FRenderResourceFactory::CreatePixelShader(ShaderFilePathString, &PointLightShadowPS);

	RegisterShaderReloadCache(ShaderPath, ShaderUsage::SHADOWMAP);
}

void URenderer::CreateHitProxyShader()
{
	std::filesystem::path ShaderPath = std::filesystem::current_path() / "Asset" / "Shader" / "HitProxyShader.hlsl";

	ID3DBlob* VSBlob = nullptr;
	ID3DBlob* PSBlob = nullptr;

	HRESULT hr = D3DCompileFromFile(ShaderPath.c_str(), nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &VSBlob, nullptr);
	if (FAILED(hr))
	{
		UE_LOG_ERROR("Renderer: HitProxyShader.hlsl VS 컴파일 실패");
		return;
	}

	hr = GetDevice()->CreateVertexShader(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), nullptr, &HitProxyVS);
	if (FAILED(hr))
	{
		UE_LOG_ERROR("Renderer: HitProxy VertexShader 생성 실패");
		SafeRelease(VSBlob);
		return;
	}

	hr = D3DCompileFromFile(ShaderPath.c_str(), nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &PSBlob, nullptr);
	if (FAILED(hr))
	{
		UE_LOG_ERROR("Renderer: HitProxyShader.hlsl PS 컴파일 실패");
		SafeRelease(VSBlob);
		return;
	}

	hr = GetDevice()->CreatePixelShader(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize(), nullptr, &HitProxyPS);
	if (FAILED(hr))
	{
		UE_LOG_ERROR("Renderer: HitProxy PixelShader 생성 실패");
		SafeRelease(VSBlob);
		SafeRelease(PSBlob);
		return;
	}

	// InputLayout (Position + Normal)
	D3D11_INPUT_ELEMENT_DESC InputElementDesc[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	hr = GetDevice()->CreateInputLayout(InputElementDesc, 2, VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), &HitProxyInputLayout);
	if (FAILED(hr))
	{
		UE_LOG_ERROR("Renderer: HitProxy InputLayout 생성 실패");
	}

	SafeRelease(VSBlob);
	SafeRelease(PSBlob);

	RegisterShaderReloadCache(ShaderPath, ShaderUsage::HITPROXY);
}

TSet<ShaderUsage> URenderer::GatherHotReloadTargets()
{
	TSet<ShaderUsage> HotReloadTargets = {};
	for (const auto& ShaderFileUsagePair : ShaderFileUsageMap)
	{
		const std::wstring& ShaderFilePathString = ShaderFileUsagePair.first;
		const std::filesystem::path ShaderPath(ShaderFilePathString);

		// 해당 파일을 마지막으로 불러왔을 때 캐시해둔 최종 수정 시간 조회
		const auto* CachedLastWriteTimePtr = ShaderFileLastWriteTimeMap.Find(ShaderFilePathString);
		if (!CachedLastWriteTimePtr)
		{
			continue;
		}

		const auto& CachedLastWriteTime = *CachedLastWriteTimePtr;

		// 파일의 현재 최종 수정 시간 조회
		std::error_code ErrorCode;
		const auto CurrentLastWriteTime = std::filesystem::last_write_time(ShaderPath, ErrorCode);
		if (ErrorCode)
		{
			continue;
		}
		
		// 두 시간 비교해서 다르면 핫 리로드 대상에 추가
		if (CurrentLastWriteTime != CachedLastWriteTime)
		{
			for (const ShaderUsage Usage : ShaderFileUsagePair.second)
			{
				HotReloadTargets.Add(Usage);
			}
		}
	}

	return HotReloadTargets;
}

void URenderer::HotReloadShaders()
{
	TSet<ShaderUsage> HotReloadTargets = GatherHotReloadTargets();

	for (const auto& Usage : HotReloadTargets)
	{
		switch (Usage)
		{
		case ShaderUsage::DEFAULT:
			SafeRelease(DefaultInputLayout);
			SafeRelease(DefaultVertexShader);
			SafeRelease(DefaultPixelShader);
			CreateDefaultShader();
			break;
		case ShaderUsage::TEXTURE:
			SafeRelease(TextureInputLayout);
			SafeRelease(TextureVertexShader);
			SafeRelease(TexturePixelShader);
			CreateTextureShader();
			for (FRenderPass* RenderPass : RenderPasses)
			{
				if (auto* BillboardPass = dynamic_cast<FBillboardPass*>(RenderPass))
				{
					BillboardPass->SetInputLayout(TextureInputLayout);
					BillboardPass->SetVertexShader(TextureVertexShader);
					BillboardPass->SetPixelShader(TexturePixelShader);
				}
				else if (auto* EditorIconPass = dynamic_cast<FEditorIconPass*>(RenderPass))
				{
					EditorIconPass->SetInputLayout(TextureInputLayout);
					EditorIconPass->SetVertexShader(TextureVertexShader);
					EditorIconPass->SetPixelShader(TexturePixelShader);
				}
			}
			break;
		case ShaderUsage::DECAL:
			SafeRelease(DecalInputLayout);
			SafeRelease(DecalVertexShader);
			SafeRelease(DecalPixelShader);
			CreateDecalShader();
			for (FRenderPass* RenderPass : RenderPasses)
			{
				if (auto* DecalPass = dynamic_cast<FDecalPass*>(RenderPass))
				{
					DecalPass->SetInputLayout(DecalInputLayout);
					DecalPass->SetVertexShader(DecalVertexShader);
					DecalPass->SetPixelShader(DecalPixelShader);
					break;
				}
			}
			break;
		case ShaderUsage::FOG:
			SafeRelease(FogInputLayout);
			SafeRelease(FogVertexShader);
			SafeRelease(FogPixelShader);
			CreateFogShader();
			for (FRenderPass* RenderPass : RenderPasses)
			{
				if (auto* FogPass = dynamic_cast<FFogPass*>(RenderPass))
				{
					FogPass->SetInputLayout(FogInputLayout);
					FogPass->SetVertexShader(FogVertexShader);
					FogPass->SetPixelShader(FogPixelShader);
					break;
				}
			}
			break;
		case ShaderUsage::FXAA:
			SafeRelease(FXAAInputLayout);
			SafeRelease(FXAAVertexShader);
			SafeRelease(FXAAPixelShader);
			SafeRelease(FXAASamplerState);
			CreateFXAAShader();
			if (FXAAPass)
			{
				FXAAPass->SetInputLayout(FXAAInputLayout);
				FXAAPass->SetVertexShader(FXAAVertexShader);
				FXAAPass->SetPixelShader(FXAAPixelShader);
				FXAAPass->SetSamplerState(FXAASamplerState);
			}
			break;
		case ShaderUsage::STATICMESH:
			SafeRelease(UberLitInputLayout);
			SafeRelease(UberLitVertexShader);
			SafeRelease(UberLitVertexShaderGouraud);
			SafeRelease(UberLitPixelShader);
			SafeRelease(UberLitPixelShaderGouraud);
			SafeRelease(UberLitPixelShaderBlinnPhong);
			SafeRelease(UberLitPixelShaderWorldNormal);
			CreateStaticMeshShader();
			for (FRenderPass* RenderPass : RenderPasses)
			{
				if (auto* StaticMeshPass = dynamic_cast<FStaticMeshPass*>(RenderPass))
				{
					StaticMeshPass->SetInputLayout(UberLitInputLayout);
					StaticMeshPass->SetVertexShader(UberLitVertexShader);
					StaticMeshPass->SetPixelShader(UberLitPixelShader);
					break;
				}
			}
			break;
		case ShaderUsage::GIZMO:
			SafeRelease(GizmoInputLayout);
			SafeRelease(GizmoVS);
			SafeRelease(GizmoPS);
			CreateGizmoShader();
			for (FRenderPass* RenderPass : RenderPasses)
			{
				if (auto* LightPass = dynamic_cast<FLightPass*>(RenderPass))
				{
					LightPass->SetInputLayout(GizmoInputLayout);
					LightPass->SetVertexShader(GizmoVS);
					LightPass->SetPixelShader(GizmoPS);
					break;
				}
			}
			break;
		case ShaderUsage::CLUSTERED_RENDERING_GRID:
			SafeRelease(ClusteredRenderingGridInputLayout);
			SafeRelease(ClusteredRenderingGridVS);
			SafeRelease(ClusteredRenderingGridPS);
			CreateClusteredRenderingGrid();
			if (ClusteredRenderingGridPass)
			{
				ClusteredRenderingGridPass->SetVertexShader(ClusteredRenderingGridVS);
				ClusteredRenderingGridPass->SetPixelShader(ClusteredRenderingGridPS);
				ClusteredRenderingGridPass->SetInputLayout(ClusteredRenderingGridInputLayout);
			}
			break;
		case ShaderUsage::HITPROXY:
			SafeRelease(HitProxyInputLayout);
			SafeRelease(HitProxyVS);
			SafeRelease(HitProxyPS);
			CreateHitProxyShader();
			if (HitProxyPass)
			{
				HitProxyPass->SetShaders(HitProxyVS, HitProxyPS, HitProxyInputLayout);
			}
			break;
		}
	}
}

void URenderer::ReleaseDefaultShader()
{
	SafeRelease(UberLitInputLayout);
	SafeRelease(UberLitPixelShader);
	SafeRelease(UberLitPixelShaderGouraud);
	SafeRelease(UberLitPixelShaderBlinnPhong);
	SafeRelease(UberLitPixelShaderWorldNormal);
	SafeRelease(UberLitVertexShader);
	SafeRelease(UberLitVertexShaderGouraud);
	
	SafeRelease(DefaultInputLayout);
	SafeRelease(DefaultPixelShader);
	SafeRelease(DefaultVertexShader);
	
	SafeRelease(TextureInputLayout);
	SafeRelease(TexturePixelShader);
	SafeRelease(TextureVertexShader);
	
	SafeRelease(DecalVertexShader);
	SafeRelease(DecalPixelShader);
	SafeRelease(DecalInputLayout);
	
	SafeRelease(FogVertexShader);
	SafeRelease(FogPixelShader);
	SafeRelease(FogInputLayout);
	
	SafeRelease(FXAAVertexShader);
	SafeRelease(FXAAPixelShader);
	SafeRelease(FXAAInputLayout);

	SafeRelease(GizmoVS);
	SafeRelease(GizmoPS);
	SafeRelease(GizmoInputLayout);

	SafeRelease(ClusteredRenderingGridInputLayout);
	SafeRelease(ClusteredRenderingGridVS);
	SafeRelease(ClusteredRenderingGridPS);

	SafeRelease(DepthOnlyVertexShader);
	SafeRelease(DepthOnlyPixelShader);
	SafeRelease(DepthOnlyInputLayout);

	SafeRelease(PointLightShadowVS);
	SafeRelease(PointLightShadowPS);
	SafeRelease(PointLightShadowInputLayout);

	SafeRelease(HitProxyVS);
	SafeRelease(HitProxyPS);
	SafeRelease(HitProxyInputLayout);
}

void URenderer::ReleaseDepthStencilState()
{
	SafeRelease(DefaultDepthStencilState);
	SafeRelease(DecalDepthStencilState);
	SafeRelease(DisabledDepthStencilState);
	if (GetDeviceContext())
	{
		GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
	}
}

void URenderer::ReleaseBlendState()
{
    SafeRelease(AlphaBlendState);
	SafeRelease(AdditiveBlendState);
}

void URenderer::ReleaseSamplerState()
{
	SafeRelease(FXAASamplerState);
	SafeRelease(DefaultSampler);
	SafeRelease(ShadowComparisonSampler);
	SafeRelease(VarianceShadowSampler);
	SafeRelease(PointShadowSampler);
}

void URenderer::Update()
{
	// 토글에 따라서 FXAA bool값 세팅
    if (const ULevel* CurrentLevel = GWorld->GetLevel())
    {
        bFXAAEnabled = (CurrentLevel->GetShowFlags() & EEngineShowFlags::SF_FXAA) != 0;
    }
    else
    {
        bFXAAEnabled = true;
    }

    RenderBegin();

    TArray<FViewport*>& Viewports = UViewportManager::GetInstance().GetViewports();
    UViewportManager& ViewportMgr = UViewportManager::GetInstance();
    EViewportLayout CurrentLayout = ViewportMgr.GetViewportLayout();

    // Single layout이면 ActiveIndex만, Quad layout이면 전체 렌더링
    int32 StartIndex = (CurrentLayout == EViewportLayout::Single) ? ViewportMgr.GetActiveIndex() : 0;
    int32 EndIndex = (CurrentLayout == EViewportLayout::Single) ? (StartIndex + 1) : Viewports.Num();

    for (int32 ViewportIndex = StartIndex; ViewportIndex < EndIndex; ++ViewportIndex)
    {
        FViewport* Viewport = Viewports[ViewportIndex];

    	// TODO(KHJ): 해당 해결책은 근본적인 해결법이 아니며 실제 Viewport 세팅의 문제를 확인할 필요가 있음
    	// 너무 작은 뷰포트는 건너뛰기 (애니메이션 중 artefact 방지)
		// 50픽셀 미만의 뷰포트는 렌더링하지 않음
		if (Viewport->GetRect().Width < 50 || Viewport->GetRect().Height < 50)
		{
			continue;
		}

    	FRect SingleWindowRect = Viewport->GetRect();
    	const int32 ViewportToolBarHeight = 32;
    	D3D11_VIEWPORT LocalViewport = { (float)SingleWindowRect.Left,(float)SingleWindowRect.Top + ViewportToolBarHeight, (float)SingleWindowRect.Width, (float)SingleWindowRect.Height - ViewportToolBarHeight, 0.0f, 1.0f };
    	GetDeviceContext()->RSSetViewports(1, &LocalViewport);
		Viewport->SetRenderRect(LocalViewport);
        UCamera* CurrentCamera = Viewport->GetViewportClient()->GetCamera();

        CurrentCamera->Update(LocalViewport);

        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferViewProj, CurrentCamera->GetFViewProjConstants());
        Pipeline->SetConstantBuffer(1, EShaderType::VS, ConstantBufferViewProj);
        {
            TIME_PROFILE(RenderLevel)
            RenderLevel(Viewport, ViewportIndex);
        }

		// PIE가 활성화된 뷰포트가 아닌 경우에만 에디터 도구 렌더링
		bool bIsPIEViewport = GEditor->IsPIESessionActive() &&
			ViewportIndex == UViewportManager::GetInstance().GetPIEActiveViewportIndex();
		if (!bIsPIEViewport)
		{
			// Grid, Gizmo 렌더링 (3D, FXAA 적용 대상)
			GEditor->GetEditorModule()->RenderEditorGeometry();
			GEditor->GetEditorModule()->RenderGizmo(CurrentCamera, LocalViewport);
		}
    }

    // FXAA는 SceneColor → 백버퍼로 복사
    if (bFXAAEnabled)
    {
        ID3D11RenderTargetView* nullRTV[] = { nullptr };
        GetDeviceContext()->OMSetRenderTargets(1, nullRTV, nullptr);

        FRenderingContext RenderingContext;
        FXAAPass->Execute(RenderingContext);
    }

    // D2D 오버레이는 FXAA 후 각 뷰포트마다 독립적으로 렌더링
    for (int32 ViewportIndex = StartIndex; ViewportIndex < EndIndex; ++ViewportIndex)
    {
        FViewport* Viewport = Viewports[ViewportIndex];
        if (!Viewport)
        {
            continue;
        }

        if (Viewport->GetRect().Width < 50 || Viewport->GetRect().Height < 50)
        {
            continue;
        }

        bool bIsPIEViewport = GEditor->IsPIESessionActive() &&
                               ViewportIndex == UViewportManager::GetInstance().GetPIEActiveViewportIndex();
        if (!bIsPIEViewport)
        {
            FRect SingleWindowRect = Viewport->GetRect();
            const int32 ViewportToolBarHeight = 32;
            D3D11_VIEWPORT LocalViewport = { static_cast<float>(SingleWindowRect.Left),static_cast<float>(SingleWindowRect.Top) + ViewportToolBarHeight, static_cast<float>(SingleWindowRect.Width), static_cast<float>(SingleWindowRect.Height) - ViewportToolBarHeight, 0.0f, 1.0f };
            UCamera* CurrentCamera = Viewport->GetViewportClient()->GetCamera();

            GEditor->GetEditorModule()->Collect2DRender(CurrentCamera, LocalViewport);
            TIME_PROFILE(FlushAndRender)
            FD2DOverlayManager::GetInstance().FlushAndRender();
        }
    }
    {
        TIME_PROFILE(UUIManager)
        UUIManager::GetInstance().Render();
    }

    RenderEnd();
}

void URenderer::RenderBegin() const
{
	auto* RenderTargetView = DeviceResources->GetRenderTargetView();
	GetDeviceContext()->ClearRenderTargetView(RenderTargetView, ClearColor);

	// @TODO: The clear color for the normal buffer should be a specific value (e.g., {0.5, 0.5, 1.0, 1.0})
	auto* NormalRenderTargetView = DeviceResources->GetNormalRenderTargetView();
	GetDeviceContext()->ClearRenderTargetView(NormalRenderTargetView, ClearColor);

	auto* DepthStencilView = DeviceResources->GetDepthStencilView();
	GetDeviceContext()->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	// FXAA bool값 변수에 따라서 RTV세팅
    if (bFXAAEnabled)
    {
        auto* SceneColorRenderTargetView = DeviceResources->GetSceneColorRenderTargetView();
        auto* NormalRenderTargetView = DeviceResources->GetNormalRenderTargetView();
        GetDeviceContext()->ClearRenderTargetView(SceneColorRenderTargetView, ClearColor);
        GetDeviceContext()->ClearRenderTargetView(NormalRenderTargetView, ClearColor);
        GetDeviceContext()->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
        ID3D11RenderTargetView* rtvs[] = { SceneColorRenderTargetView, NormalRenderTargetView };
        GetDeviceContext()->OMSetRenderTargets(2, rtvs, DepthStencilView);
    }
    else
    {
        auto* RenderTargetView = DeviceResources->GetRenderTargetView();
        auto* NormalRenderTargetView = DeviceResources->GetNormalRenderTargetView();
        GetDeviceContext()->ClearRenderTargetView(RenderTargetView, ClearColor);
        GetDeviceContext()->ClearRenderTargetView(NormalRenderTargetView, ClearColor);
        GetDeviceContext()->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
        ID3D11RenderTargetView* rtvs[] = { RenderTargetView, NormalRenderTargetView };
        GetDeviceContext()->OMSetRenderTargets(2, rtvs, DepthStencilView);
    }

    DeviceResources->UpdateViewport();
}

void URenderer::RenderLevel(FViewport* InViewport, int32 ViewportIndex)
{
	// 뷰포트별로 렌더링할 World 결정 (PIE active viewport면 PIE World, 아니면 Editor World)
	UWorld* WorldToRender = GEditor->GetWorldForViewport(ViewportIndex);
	if (!WorldToRender) { return; }

	const ULevel* CurrentLevel = WorldToRender->GetLevel();
	if (!CurrentLevel) { return; }

	const FCameraConstants& ViewProj = InViewport->GetViewportClient()->GetCamera()->GetFViewProjConstants();
	static bool bCullingEnabled = false; // 임시 토글(초기값: 컬링 비활성)
	TArray<UPrimitiveComponent*> FinalVisiblePrims;
	if (!bCullingEnabled)
	{
		// 1) 옥트리(정적 프리미티브) 전부 수집
		if (FOctree* StaticOctree = WorldToRender->GetLevel()->GetStaticOctree())
		{
			TArray<UPrimitiveComponent*> AllStatics;
			StaticOctree->GetAllPrimitives(AllStatics);
			for (UPrimitiveComponent* Primitive : AllStatics)
			{
				if (Primitive && Primitive->IsVisible())
				{
					FinalVisiblePrims.Add(Primitive);
				}
			}
		}
		// 2) 동적 프리미티브 전부 수집
		TArray<UPrimitiveComponent*>& DynamicPrimitives = WorldToRender->GetLevel()->GetDynamicPrimitives();
		for (UPrimitiveComponent* Primitive : DynamicPrimitives)
		{
			if (Primitive && Primitive->IsVisible())
			{
				FinalVisiblePrims.Add(Primitive);
			}
		}
	}
	else
	{
		FinalVisiblePrims = InViewport->GetViewportClient()->GetCamera()->GetViewVolumeCuller().GetRenderableObjects();
	}

	RenderingContext = FRenderingContext(

		&ViewProj,
		InViewport->GetViewportClient()->GetCamera(),
		InViewport->GetViewportClient()->GetViewMode(),
		CurrentLevel->GetShowFlags(),
		InViewport->GetRenderRect(),
		{DeviceResources->GetViewportInfo().Width, DeviceResources->GetViewportInfo().Height}
		);

	// 1. Sort visible primitive components
	RenderingContext.AllPrimitives = FinalVisiblePrims;
	for (auto& Prim : FinalVisiblePrims)
	{
		if (auto StaticMesh = Cast<UStaticMeshComponent>(Prim))
		{
			RenderingContext.StaticMeshes.Add(StaticMesh);
		}
		else if (auto BillBoard = Cast<UBillBoardComponent>(Prim))
		{
			RenderingContext.BillBoards.Add(BillBoard);
		}
		else if (auto EditorIcon = Cast<UEditorIconComponent>(Prim))
		{
			// Pilot Mode: 현재 조종 중인 Actor의 아이콘은 렌더링 스킵
			UEditor* Editor = GEditor ? GEditor->GetEditorModule() : nullptr;
			bool bShouldSkip = false;

			if (Editor && Editor->IsPilotMode() && Editor->GetPilotedActor())
			{
				AActor* OwnerActor = EditorIcon->GetTypedOuter<AActor>();
				bShouldSkip = (OwnerActor == Editor->GetPilotedActor());
			}

			if (!bShouldSkip)
			{
				RenderingContext.EditorIcons.Add(EditorIcon);
			}
		}
		else if (auto Text = Cast<UTextComponent>(Prim))
		{
			if (!Text->IsExactly(UUUIDTextComponent::StaticClass())) { RenderingContext.Texts.Add(Text); }
			else { RenderingContext.UUIDs.Add(Cast<UUUIDTextComponent>(Text)); }
		}
		else if (auto Decal = Cast<UDecalComponent>(Prim))
		{
			RenderingContext.Decals.Add(Decal);
		}
	}
	
	for (const auto& LightComponent : CurrentLevel->GetLightComponents())
	{
		if (auto PointLightComponent = Cast<UPointLightComponent>(LightComponent))
		{
			auto SpotLightComponent = Cast<USpotLightComponent>(LightComponent);
			
			if (SpotLightComponent &&
				SpotLightComponent->GetVisible() &&
				SpotLightComponent->GetLightEnabled())
			{
				RenderingContext.SpotLights.Add(SpotLightComponent);
			}
			else if (PointLightComponent &&
				PointLightComponent->GetVisible() &&
				PointLightComponent->GetLightEnabled())
			{
				RenderingContext.PointLights.Add(PointLightComponent);
			}
		}

		auto DirectionalLightComponent = Cast<UDirectionalLightComponent>(LightComponent);
		if (DirectionalLightComponent &&
			DirectionalLightComponent->GetVisible() &&
			DirectionalLightComponent->GetLightEnabled() &&
			RenderingContext.DirectionalLights.IsEmpty())
		{
			RenderingContext.DirectionalLights.Add(DirectionalLightComponent);
		}

		auto AmbientLightComponent = Cast<UAmbientLightComponent>(LightComponent);
		if (AmbientLightComponent &&
			AmbientLightComponent->GetVisible() &&
			AmbientLightComponent->GetLightEnabled() &&
			RenderingContext.AmbientLights.IsEmpty())
		{
			RenderingContext.AmbientLights.Add(AmbientLightComponent);
		}
	}

	// 2. Collect HeightFogComponents from all actors in the level
	for (const auto& Actor : CurrentLevel->GetLevelActors())
	{
		for (const auto& Component : Actor->GetOwnedComponents())
		{
			if (auto Fog = Cast<UHeightFogComponent>(Component))
			{
				RenderingContext.Fogs.Add(Fog);
			}
		}
	}

	for (auto RenderPass: RenderPasses)
	{
		RenderPass->Execute(RenderingContext);
	}
}

void URenderer::RenderEditorPrimitive(const FEditorPrimitive& InPrimitive, const FRenderState& InRenderState, uint32 InStride, uint32 InIndexBufferStride)
{
    // Use the global stride if InStride is 0
    const uint32 FinalStride = (InStride == 0) ? Stride : InStride;

    // Allow for custom shaders, fallback to default
    FPipelineInfo PipelineInfo = {
        InPrimitive.InputLayout ? InPrimitive.InputLayout : DefaultInputLayout,
        InPrimitive.VertexShader ? InPrimitive.VertexShader : DefaultVertexShader,
		FRenderResourceFactory::GetRasterizerState(InRenderState),
        InPrimitive.bShouldAlwaysVisible ? DisabledDepthStencilState : DefaultDepthStencilState,
        InPrimitive.PixelShader ? InPrimitive.PixelShader : DefaultPixelShader,
        nullptr,
        InPrimitive.Topology
    };
    Pipeline->UpdatePipeline(PipelineInfo);

    // Update constant buffers
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModels,
		FMatrix::GetModelMatrix(InPrimitive.Location, InPrimitive.Rotation, InPrimitive.Scale));
	Pipeline->SetConstantBuffer(0, EShaderType::VS, ConstantBufferModels);
	Pipeline->SetConstantBuffer(1, EShaderType::VS, ConstantBufferViewProj);
	
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferColor, InPrimitive.Color);
	Pipeline->SetConstantBuffer(2, EShaderType::PS, ConstantBufferColor);
	Pipeline->SetConstantBuffer(2, EShaderType::VS, ConstantBufferColor);
	
    Pipeline->SetVertexBuffer(InPrimitive.VertexBuffer, FinalStride);

    // The core logic: check for an index buffer
    if (InPrimitive.IndexBuffer && InPrimitive.NumIndices > 0)
    {
        Pipeline->SetIndexBuffer(InPrimitive.IndexBuffer, InIndexBufferStride);
        Pipeline->DrawIndexed(InPrimitive.NumIndices, 0, 0);
    }
    else
    {
        Pipeline->Draw(InPrimitive.NumVertices, 0);
    }
}

void URenderer::RenderEditorPrimitiveIndexed(const FEditorPrimitive& InPrimitive, const FRenderState& InRenderState, uint32 InStride, uint32 InIndexBufferStride, uint32 StartIndexLocation, uint32 IndexCount)
{
	// Use the global stride if InStride is 0
	const uint32 FinalStride = (InStride == 0) ? Stride : InStride;

	// Allow for custom shaders, fallback to default
	FPipelineInfo PipelineInfo = {
		InPrimitive.InputLayout ? InPrimitive.InputLayout : DefaultInputLayout,
		InPrimitive.VertexShader ? InPrimitive.VertexShader : DefaultVertexShader,
		FRenderResourceFactory::GetRasterizerState(InRenderState),
		InPrimitive.bShouldAlwaysVisible ? DisabledDepthStencilState : DefaultDepthStencilState,
		InPrimitive.PixelShader ? InPrimitive.PixelShader : DefaultPixelShader,
		nullptr,
		InPrimitive.Topology
	};
	Pipeline->UpdatePipeline(PipelineInfo);

	// Update constant buffers
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModels,
		FMatrix::GetModelMatrix(InPrimitive.Location, InPrimitive.Rotation, InPrimitive.Scale));
	Pipeline->SetConstantBuffer(0, EShaderType::VS, ConstantBufferModels);
	Pipeline->SetConstantBuffer(1, EShaderType::VS, ConstantBufferViewProj);

	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferColor, InPrimitive.Color);
	Pipeline->SetConstantBuffer(2, EShaderType::PS, ConstantBufferColor);
	Pipeline->SetConstantBuffer(2, EShaderType::VS, ConstantBufferColor);

	Pipeline->SetVertexBuffer(InPrimitive.VertexBuffer, FinalStride);

	// 인덱스 버퍼가 있으면 지정된 범위만 렌더링
	if (InPrimitive.IndexBuffer && IndexCount > 0)
	{
		Pipeline->SetIndexBuffer(InPrimitive.IndexBuffer, InIndexBufferStride);
		Pipeline->DrawIndexed(IndexCount, StartIndexLocation, 0);
	}
	else if (IndexCount > 0)
	{
		Pipeline->Draw(IndexCount, StartIndexLocation);
	}
}

void URenderer::RenderEnd() const
{
	TIME_PROFILE(DrawCall)
	GetSwapChain()->Present(0, 0);
	TIME_PROFILE_END(DrawCall)
}

void URenderer::OnResize(uint32 InWidth, uint32 InHeight) const
{
    if (!DeviceResources || !GetDeviceContext() || !GetSwapChain()) return;

    DeviceResources->ReleaseFactories();
    DeviceResources->ReleaseSceneColorTarget();
	DeviceResources->ReleaseFrameBuffer();
	DeviceResources->ReleaseDepthBuffer();
	DeviceResources->ReleaseNormalBuffer();
	GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);

    if (FAILED(GetSwapChain()->ResizeBuffers(2, InWidth, InHeight, DXGI_FORMAT_UNKNOWN, 0)))
    {
        UE_LOG("OnResize Failed");
        return;
    }

	DeviceResources->UpdateViewport();
    DeviceResources->CreateSceneColorTarget();
	DeviceResources->CreateFrameBuffer();
	DeviceResources->CreateDepthBuffer();
	DeviceResources->CreateNormalBuffer();
    DeviceResources->CreateFactories();

    ID3D11RenderTargetView* targetView = bFXAAEnabled
        ? DeviceResources->GetSceneColorRenderTargetView()
        : DeviceResources->GetRenderTargetView();
    ID3D11RenderTargetView* targetViews[] = { targetView };
    GetDeviceContext()->OMSetRenderTargets(1, targetViews, DeviceResources->GetDepthStencilView());
}

ID3D11VertexShader* URenderer::GetVertexShader(EViewModeIndex ViewModeIndex) const
{
	if (ViewModeIndex == EViewModeIndex::VMI_Gouraud)
	{
		return UberLitVertexShaderGouraud;
	}
	else if (ViewModeIndex == EViewModeIndex::VMI_Lambert
		|| ViewModeIndex == EViewModeIndex::VMI_BlinnPhong
		|| ViewModeIndex == EViewModeIndex::VMI_WorldNormal)
	{
		return UberLitVertexShader;
	}
	else if (ViewModeIndex == EViewModeIndex::VMI_Unlit || ViewModeIndex == EViewModeIndex::VMI_SceneDepth)
	{
		return TextureVertexShader;
	}

	return nullptr;
}

ID3D11PixelShader* URenderer::GetPixelShader(EViewModeIndex ViewModeIndex) const
{
	if (ViewModeIndex == EViewModeIndex::VMI_Gouraud)
	{
		return UberLitPixelShaderGouraud;
	}
	else if (ViewModeIndex == EViewModeIndex::VMI_Lambert)
	{
		return UberLitPixelShader;
	}
	else if (ViewModeIndex == EViewModeIndex::VMI_BlinnPhong)
	{
		return UberLitPixelShaderBlinnPhong;
	}
	else if (ViewModeIndex == EViewModeIndex::VMI_Unlit || ViewModeIndex == EViewModeIndex::VMI_SceneDepth)
	{
		return TexturePixelShader;
	}
	else if (ViewModeIndex == EViewModeIndex::VMI_WorldNormal)
	{
		return UberLitPixelShaderWorldNormal;
	}

	return nullptr;
}

void URenderer::CreateConstantBuffers()
{
	ConstantBufferModels = FRenderResourceFactory::CreateConstantBuffer<FMatrix>();
	ConstantBufferColor = FRenderResourceFactory::CreateConstantBuffer<FVector4>();
	ConstantBufferViewProj = FRenderResourceFactory::CreateConstantBuffer<FCameraConstants>();
}

void URenderer::ReleaseConstantBuffers()
{
	SafeRelease(ConstantBufferModels);
	SafeRelease(ConstantBufferColor);
	SafeRelease(ConstantBufferViewProj);
}

void URenderer::RenderHitProxyPass(UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
	if (!HitProxyPass || !InCamera)
	{
		if (!HitProxyPass)
		{
			UE_LOG_ERROR("Renderer: HitProxyPass가 null입니다 (셰이더 로드 실패?)");
		}
		return;
	}

	// 현재 활성 레벨의 컴포넌트 수집
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return;
	}

	ULevel* CurrentLevel = World->GetLevel();
	if (!CurrentLevel)
	{
		return;
	}

	// RenderingContext 구성
	FRenderingContext Context;
	Context.ViewProjConstants = nullptr;
	Context.CurrentCamera = InCamera;
	Context.ViewMode = EViewModeIndex::VMI_Unlit;
	Context.ShowFlags = static_cast<uint64>(EEngineShowFlags::SF_StaticMesh);
	Context.Viewport = InViewport;
	Context.RenderTargetSize = FVector2(InViewport.Width, InViewport.Height);

	// 모든 Primitive 컴포넌트 수집
	TArray<UPrimitiveComponent*> AllVisiblePrims;

	// Static Octree에서 정적 프리미티브 수집
	if (FOctree* StaticOctree = CurrentLevel->GetStaticOctree())
	{
		TArray<UPrimitiveComponent*> AllStatics;
		StaticOctree->GetAllPrimitives(AllStatics);
		for (UPrimitiveComponent* Primitive : AllStatics)
		{
			if (Primitive && Primitive->IsVisible())
			{
				AllVisiblePrims.Add(Primitive);
			}
		}
	}

	// 동적 프리미티브 수집
	TArray<UPrimitiveComponent*>& DynamicPrimitives = CurrentLevel->GetDynamicPrimitives();
	for (UPrimitiveComponent* Primitive : DynamicPrimitives)
	{
		if (Primitive && Primitive->IsVisible())
		{
			AllVisiblePrims.Add(Primitive);
		}
	}

	// Primitive 타입별로 분류
	for (auto& Prim : AllVisiblePrims)
	{
		if (auto StaticMesh = Cast<UStaticMeshComponent>(Prim))
		{
			Context.StaticMeshes.Add(StaticMesh);
		}
		else if (auto EditorIcon = Cast<UEditorIconComponent>(Prim))
		{
			// Pilot Mode: 현재 조종 중인 Actor의 아이콘은 렌더링 스킵
			// Outer 체인을 타고 올라가며 AActor 찾기
			UEditor* Editor = GEditor ? GEditor->GetEditorModule() : nullptr;
			bool bShouldSkip = false;
			if (Editor && Editor->IsPilotMode() && Editor->GetPilotedActor())
			{
				AActor* OwnerActor = EditorIcon->GetTypedOuter<AActor>();
				bShouldSkip = (OwnerActor == Editor->GetPilotedActor());
			}

			if (!bShouldSkip)
			{
				Context.EditorIcons.Add(EditorIcon);
			}
		}
		else if (auto BillBoard = Cast<UBillBoardComponent>(Prim))
		{
			Context.BillBoards.Add(BillBoard);
		}
		// 필요하면 다른 타입도 추가 가능
	}

	// HitProxyPass 실행
	HitProxyPass->Execute(Context);

	// 기즈모도 HitProxy 렌더링 (UI 우선순위로 씬 오브젝트 위에 그려짐)
	UEditor* Editor = GEditor->GetEditorModule();
	if (Editor && Editor->GetSelectedComponent())
	{
		Editor->RenderGizmoForHitProxy(InCamera, InViewport);
	}
}
