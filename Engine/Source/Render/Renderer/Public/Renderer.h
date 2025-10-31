#pragma once
#include "Global/Types.h"
#include <filesystem>
#include "DeviceResources.h"
#include "Core/Public/Object.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Editor/Public/EditorPrimitive.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/RenderPass/Public/FXAAPass.h"

class FClusteredRenderingGridPass;
class FFXAAPass;
class FHitProxyPass;
class FLightPass;
class FShadowMapFilterPass;
class FShadowMapPass;
class FViewport;
class FViewportClient;
class UCamera;
class UPipeline;

// URenderer 내부에서 셰이더들은 용도에 따라 분류될 수 있음.
// 용도별로 Create Shader 메소드가 존재함. (e.g. CreateStaticmeshShader)
enum class ShaderUsage
{
	DEFAULT,
	TEXTURE,
	DECAL,
	FOG,
	FXAA,
	STATICMESH,
	GIZMO,
	CLUSTERED_RENDERING_GRID,
	SHADOWMAP,
	HITPROXY
};

/**
 * @brief Rendering Pipeline 전반을 처리하는 클래스
 */
UCLASS()
class URenderer : public UObject
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(URenderer, UObject)

public:
	void Init(HWND InWindowHandle);
	void Release();

	// Initialize
	void CreateDepthStencilState();
	void CreateBlendState();
	void CreateSamplerState();
	void CreateDefaultShader();
	void CreateTextureShader();
	void CreateDecalShader();
	void CreateFogShader();
	void CreateConstantBuffers();
	void CreateFXAAShader();
	void CreateStaticMeshShader();
	void CreateGizmoShader();
	void CreateClusteredRenderingGrid();
	void CreateDepthOnlyShader();
	void CreatePointLightShadowShader();
	void CreateHitProxyShader();
	void RenderHitProxyPass(UCamera* InCamera, const D3D11_VIEWPORT& InViewport);

	// HotReload
	/** @brief 런타임 중 VS, PS 셰이더 파일이 변경되었는지 확인하고, 변경된 파일을 사용하는 Shader Usage들을 반환합니다.*/
	TSet<ShaderUsage> GatherHotReloadTargets();
	/** @brief Shader Usage 단위로 vertex shader와 pixel shader를 핫 리로드합니다.*/
	void HotReloadShaders();

	// Release
	void ReleaseConstantBuffers();
	void ReleaseDefaultShader();
	void ReleaseDepthStencilState();
	void ReleaseBlendState();
	void ReleaseSamplerState();
	
	// Render
	void Update();
	void RenderBegin() const;
	void RenderLevel(FViewport* InViewport, int32 ViewportIndex);
	void RenderEnd() const;
	void RenderEditorPrimitive(const FEditorPrimitive& InPrimitive, const FRenderState& InRenderState, uint32 InStride = 0, uint32 InIndexBufferStride = 0);
	void RenderEditorPrimitiveIndexed(const FEditorPrimitive& InPrimitive, const FRenderState& InRenderState, uint32 InStride, uint32 InIndexBufferStride, uint32 StartIndexLocation, uint32 IndexCount);

	void OnResize(uint32 Inwidth = 0, uint32 InHeight = 0) const;

	// Getter & Setter
	ID3D11Device* GetDevice() const { return DeviceResources->GetDevice(); }
	ID3D11DeviceContext* GetDeviceContext() const { return DeviceResources->GetDeviceContext(); }
	IDXGISwapChain* GetSwapChain() const { return DeviceResources->GetSwapChain(); }
	
	ID3D11SamplerState* GetDefaultSampler() const { return DefaultSampler; }
	ID3D11SamplerState* GetShadowComparisonSampler() const { return ShadowComparisonSampler; }
	ID3D11SamplerState* GetVarianceShadowSampler() const { return VarianceShadowSampler; }
	ID3D11SamplerState* GetPointShadowSampler() const { return PointShadowSampler; }
	ID3D11ShaderResourceView* GetDepthSRV() const { return DeviceResources->GetDepthStencilSRV(); }
	
	ID3D11RenderTargetView* GetRenderTargetView() const { return DeviceResources->GetRenderTargetView(); }
	ID3D11RenderTargetView* GetSceneColorRenderTargetView()const {return DeviceResources->GetSceneColorRenderTargetView(); }
	
	UDeviceResources* GetDeviceResources() const { return DeviceResources; }
	FViewport* GetViewportClient() const { return ViewportClient; }
	UPipeline* GetPipeline() const { return Pipeline; }
	bool GetIsResizing() const { return bIsResizing; }
	bool GetFXAA() const { return bFXAAEnabled; }

	ID3D11DepthStencilState* GetDefaultDepthStencilState() const { return DefaultDepthStencilState; }
	ID3D11DepthStencilState* GetDisabledDepthStencilState() const { return DisabledDepthStencilState; }
	ID3D11BlendState* GetAlphaBlendState() const { return AlphaBlendState; }
	ID3D11Buffer* GetConstantBufferModels() const { return ConstantBufferModels; }
	ID3D11Buffer* GetConstantBufferViewProj() const { return ConstantBufferViewProj; }

	void SetIsResizing(bool isResizing) { bIsResizing = isResizing; }

	ID3D11VertexShader* GetVertexShader(EViewModeIndex ViewModeIndex) const;
	ID3D11PixelShader* GetPixelShader(EViewModeIndex ViewModeIndex) const;

	FLightPass* GetLightPass() { return LightPass; }
	FClusteredRenderingGridPass* GetClusteredRenderingGridPass() { return ClusteredRenderingGridPass; }
	FShadowMapPass* GetShadowMapPass() const { return ShadowMapPass; }

	const FRenderingContext& GetRenderingContext() const { return RenderingContext; }

private:
	/*
	* @brief URenderer가 Initialize 메소드로 vertex shader, pixel shader들을 생성할 때, 해당 셰이더 파일의 용도와 최종 수정 시간을 기록합니다.
	* 런타임 중 셰이더 파일이 변경되었을 때, 기록된 정보를 토대로 변경된 파일을 사용하는 Shader Usage들을 찾아 셰이더를 핫 리로드할 수 있습니다.
	*/
	void RegisterShaderReloadCache(const std::filesystem::path& ShaderPath, ShaderUsage Usage);

	UPipeline* Pipeline = nullptr;
	UDeviceResources* DeviceResources = nullptr;
	TArray<UPrimitiveComponent*> PrimitiveComponents;

	// States
	ID3D11DepthStencilState* DefaultDepthStencilState = nullptr;
	ID3D11DepthStencilState* DecalDepthStencilState = nullptr;
	ID3D11DepthStencilState* DisabledDepthStencilState = nullptr;
	ID3D11BlendState* AlphaBlendState = nullptr;
	ID3D11BlendState* AdditiveBlendState = nullptr;
	
	// Constant Buffers
	ID3D11Buffer* ConstantBufferModels = nullptr;
	ID3D11Buffer* ConstantBufferViewProj = nullptr;
	ID3D11Buffer* ConstantBufferColor = nullptr;
	FLOAT ClearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};

	// Default Shaders
	ID3D11VertexShader* DefaultVertexShader = nullptr;
	ID3D11PixelShader* DefaultPixelShader = nullptr;
	ID3D11InputLayout* DefaultInputLayout = nullptr;

	// FXAA Shaders
	ID3D11VertexShader* FXAAVertexShader = nullptr;
	ID3D11PixelShader* FXAAPixelShader = nullptr;
	ID3D11InputLayout* FXAAInputLayout = nullptr;
	ID3D11SamplerState* FXAASamplerState = nullptr;

	// StaticMesh Shaders
	ID3D11VertexShader* UberLitVertexShader = nullptr;
	ID3D11VertexShader* UberLitVertexShaderGouraud = nullptr;
	ID3D11PixelShader* UberLitPixelShader = nullptr;
	ID3D11PixelShader* UberLitPixelShaderGouraud = nullptr;
	ID3D11PixelShader* UberLitPixelShaderBlinnPhong = nullptr;
	ID3D11PixelShader* UberLitPixelShaderWorldNormal = nullptr;
	ID3D11InputLayout* UberLitInputLayout = nullptr;
	
	//Gizmo Shaders
	ID3D11InputLayout* GizmoInputLayout = nullptr;
	ID3D11VertexShader* GizmoVS = nullptr;
	ID3D11PixelShader* GizmoPS = nullptr;

	//ClusteredRenderingGrid
	ID3D11InputLayout* ClusteredRenderingGridInputLayout = nullptr;
	ID3D11PixelShader* ClusteredRenderingGridPS = nullptr;
	ID3D11VertexShader* ClusteredRenderingGridVS = nullptr;

	// Texture Shaders
	ID3D11VertexShader* TextureVertexShader = nullptr;
	ID3D11PixelShader* TexturePixelShader = nullptr;
	ID3D11InputLayout* TextureInputLayout = nullptr;

	// Decal Shaders
	ID3D11VertexShader* DecalVertexShader = nullptr;
	ID3D11PixelShader* DecalPixelShader = nullptr;
	ID3D11InputLayout* DecalInputLayout = nullptr;
	
	// Fog Shaders
	ID3D11VertexShader* FogVertexShader = nullptr;
	ID3D11PixelShader* FogPixelShader = nullptr;
	ID3D11InputLayout* FogInputLayout = nullptr;
	ID3D11SamplerState* DefaultSampler = nullptr;
	ID3D11SamplerState* ShadowComparisonSampler = nullptr;
	ID3D11SamplerState* VarianceShadowSampler = nullptr;
	ID3D11SamplerState* PointShadowSampler = nullptr;

	// Shadow Map Shaders
	ID3D11VertexShader* DepthOnlyVertexShader = nullptr;
	ID3D11PixelShader* DepthOnlyPixelShader = nullptr;
	ID3D11InputLayout* DepthOnlyInputLayout = nullptr;

	// Point Light Shadow Shaders (with linear distance output)
	ID3D11VertexShader* PointLightShadowVS = nullptr;
	ID3D11PixelShader* PointLightShadowPS = nullptr;
	ID3D11InputLayout* PointLightShadowInputLayout = nullptr;

	// HitProxy Shader
	ID3D11VertexShader* HitProxyVS = nullptr;
	ID3D11PixelShader* HitProxyPS = nullptr;
	ID3D11InputLayout* HitProxyInputLayout = nullptr;
	FHitProxyPass* HitProxyPass = nullptr;

	uint32 Stride = 0;

	FViewport* ViewportClient = nullptr;
	
	bool bIsResizing = false;
	bool bFXAAEnabled = true;

	FRenderingContext RenderingContext{};

	TArray<class FRenderPass*> RenderPasses;

	FFXAAPass* FXAAPass = nullptr;
	FLightPass* LightPass = nullptr;
	FClusteredRenderingGridPass* ClusteredRenderingGridPass = nullptr;
	FShadowMapPass* ShadowMapPass = nullptr;
	FShadowMapFilterPass* ShadowMapFilterPass = nullptr;

	// For Hot Reloading Shaders
	TMap<std::wstring, TSet<ShaderUsage>> ShaderFileUsageMap;
	TMap<std::wstring, std::filesystem::file_time_type> ShaderFileLastWriteTimeMap;
};
