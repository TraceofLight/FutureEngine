#include "pch.h"
#include "Render/RenderPass/Public/ShadowMapPass.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Component/Public/DirectionalLightComponent.h"
#include "Component/Public/SpotLightComponent.h"
#include "Component/Public/PointLightComponent.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"

// Helper functions for matrix operations
namespace ShadowMatrixHelper
{
	// Create a look-at view matrix (Left-Handed)
	FMatrix CreateLookAtLH(const FVector& Eye, const FVector& Target, const FVector& Up)
	{
		FVector ZAxis = (Target - Eye).GetNormalized();
		FVector XAxis = Up.Cross(ZAxis).GetNormalized();
		FVector YAxis = ZAxis.Cross(XAxis);

		FMatrix Result;
		Result.Data[0][0] = XAxis.X;  Result.Data[0][1] = YAxis.X;  Result.Data[0][2] = ZAxis.X;  Result.Data[0][3] = 0.0f;
		Result.Data[1][0] = XAxis.Y;  Result.Data[1][1] = YAxis.Y;  Result.Data[1][2] = ZAxis.Y;  Result.Data[1][3] = 0.0f;
		Result.Data[2][0] = XAxis.Z;  Result.Data[2][1] = YAxis.Z;  Result.Data[2][2] = ZAxis.Z;  Result.Data[2][3] = 0.0f;
		Result.Data[3][0] = -XAxis.Dot(Eye);
		Result.Data[3][1] = -YAxis.Dot(Eye);
		Result.Data[3][2] = -ZAxis.Dot(Eye);
		Result.Data[3][3] = 1.0f;

		return Result;
	}

	// Create an orthographic projection matrix (Left-Handed)
	FMatrix CreateOrthoLH(float Left, float Right, float Bottom, float Top, float Near, float Far)
	{
		FMatrix Result;
		Result.Data[0][0] = 2.0f / (Right - Left);
		Result.Data[0][1] = 0.0f;
		Result.Data[0][2] = 0.0f;
		Result.Data[0][3] = 0.0f;

		Result.Data[1][0] = 0.0f;
		Result.Data[1][1] = 2.0f / (Top - Bottom);
		Result.Data[1][2] = 0.0f;
		Result.Data[1][3] = 0.0f;

		Result.Data[2][0] = 0.0f;
		Result.Data[2][1] = 0.0f;
		Result.Data[2][2] = 1.0f / (Far - Near);
		Result.Data[2][3] = 0.0f;

		Result.Data[3][0] = (Left + Right) / (Left - Right);
		Result.Data[3][1] = (Top + Bottom) / (Bottom - Top);
		Result.Data[3][2] = Near / (Near - Far);
		Result.Data[3][3] = 1.0f;

		return Result;
	}

	// Create a perspective projection matrix (Left-Handed)
	// Used for spot light shadow mapping
	FMatrix CreatePerspectiveFovLH(float FovYRadians, float Aspect, float Near, float Far)
	{
		// f = 1 / tan(fovY/2)
		const float F = 1.0f / std::tanf(FovYRadians * 0.5f);

		FMatrix Result = FMatrix::Identity();
		// | f/aspect   0        0         0 |
		// |    0       f        0         0 |
		// |    0       0   zf/(zf-zn)     1 |
		// |    0       0  -zn*zf/(zf-zn)  0 |
		Result.Data[0][0] = F / Aspect;
		Result.Data[1][1] = F;
		Result.Data[2][2] = Far / (Far - Near);
		Result.Data[2][3] = 1.0f;
		Result.Data[3][2] = (-Near * Far) / (Far - Near);
		Result.Data[3][3] = 0.0f;

		return Result;
	}
}

FShadowMapPass::FShadowMapPass(UPipeline* InPipeline,
	ID3D11Buffer* InConstantBufferCamera,
	ID3D11Buffer* InConstantBufferModel,
	ID3D11VertexShader* InDepthOnlyVS,
	ID3D11PixelShader* InDepthOnlyPS,
	ID3D11InputLayout* InDepthOnlyInputLayout,
	ID3D11VertexShader* InPointLightShadowVS,
	ID3D11PixelShader* InPointLightShadowPS,
	ID3D11InputLayout* InPointLightShadowInputLayout)
	: FRenderPass(InPipeline, InConstantBufferCamera, InConstantBufferModel)
	, DepthOnlyVS(InDepthOnlyVS)
	, DepthOnlyPS(InDepthOnlyPS)
	, DepthOnlyInputLayout(InDepthOnlyInputLayout)
	, PointLightShadowVS(InPointLightShadowVS)
	, PointLightShadowPS(InPointLightShadowPS)
	, PointLightShadowInputLayout(InPointLightShadowInputLayout)
{
	ID3D11Device* Device = URenderer::GetInstance().GetDevice();

	// 1. Shadow depth stencil state 생성
	D3D11_DEPTH_STENCIL_DESC DSDesc = {};
	DSDesc.DepthEnable = TRUE;
	DSDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DSDesc.DepthFunc = D3D11_COMPARISON_LESS;
	DSDesc.StencilEnable = FALSE;

	HRESULT hr = Device->CreateDepthStencilState(&DSDesc, &ShadowDepthStencilState);
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create shadow depth stencil state");
	}

	// 2. Shadow rasterizer state (slope-scale depth bias 지원)
	D3D11_RASTERIZER_DESC RastDesc = {};
	RastDesc.FillMode = D3D11_FILL_SOLID;
	RastDesc.CullMode = D3D11_CULL_BACK;  // Back-face culling
	RastDesc.FrontCounterClockwise = FALSE;
	RastDesc.DepthBias = 0;               // 동적으로 설정
	RastDesc.DepthBiasClamp = 0.0f;
	RastDesc.SlopeScaledDepthBias = 0.0f; // 동적으로 설정
	RastDesc.DepthClipEnable = TRUE;
	RastDesc.ScissorEnable = FALSE;
	RastDesc.MultisampleEnable = FALSE;
	RastDesc.AntialiasedLineEnable = FALSE;

	hr = Device->CreateRasterizerState(&RastDesc, &ShadowRasterizerState);
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create shadow rasterizer state");
	}

	// 3. Shadow view-projection constant buffer 생성 (DepthOnlyVS.hlsl의 PerFrame과 동일)
	ShadowViewProjConstantBuffer = FRenderResourceFactory::CreateConstantBuffer<FShadowViewProjConstant>();

	// 5. Point Light Shadow constant buffer 생성
	PointLightShadowParamsBuffer = FRenderResourceFactory::CreateConstantBuffer<FPointLightShadowParams>();
}

FShadowMapPass::~FShadowMapPass()
{
	Release();
}

void FShadowMapPass::Execute(FRenderingContext& Context)
{
	// IMPORTANT: Unbind shadow map SRVs before rendering to them as DSV
	// This prevents D3D11 resource hazard warnings
	const auto& Renderer = URenderer::GetInstance();
	ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();
	ID3D11ShaderResourceView* NullSRVs[2] = { nullptr, nullptr };
	DeviceContext->PSSetShaderResources(10, 2, NullSRVs);  // Unbind t10-t11 (DirectionalShadowMap, SpotShadowMap)

	// Phase 1: Directional Lights
	for (auto DirLight : Context.DirectionalLights)
	{
		if (DirLight->GetCastShadows() && DirLight->GetLightEnabled())
		{
			RenderDirectionalShadowMap(DirLight, Context.StaticMeshes);
		}
	}

	// Phase 2: Spot Lights
	for (auto SpotLight : Context.SpotLights)
	{
		if (SpotLight->GetCastShadows() && SpotLight->GetLightEnabled())
		{
			RenderSpotShadowMap(SpotLight, Context.StaticMeshes);
		}
	}

	// Phase 3: Point Lights
	for (auto PointLight : Context.PointLights)
	{
		if (PointLight->GetCastShadows() && PointLight->GetLightEnabled())
		{
			RenderPointShadowMap(PointLight, Context.StaticMeshes);
		}
	}
}

void FShadowMapPass::RenderDirectionalShadowMap(UDirectionalLightComponent* Light,
	const TArray<UStaticMeshComponent*>& Meshes)
{
	FShadowMapResource* ShadowMap = GetOrCreateShadowMap(Light);
	if (!ShadowMap || !ShadowMap->IsValid())
		return;

	const auto& Renderer = URenderer::GetInstance();
	ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();
	const auto& DeviceResources = Renderer.GetDeviceResources();

	// 0. 현재 상태 저장 (복원용)
	ID3D11RenderTargetView* OriginalRTV = nullptr;
	ID3D11DepthStencilView* OriginalDSV = nullptr;
	DeviceContext->OMGetRenderTargets(1, &OriginalRTV, &OriginalDSV);

	D3D11_VIEWPORT OriginalViewport;
	UINT NumViewports = 1;
	DeviceContext->RSGetViewports(&NumViewports, &OriginalViewport);

	// 1. Shadow render target 설정
	// Note: RenderTargets는 Pipeline API 사용, Viewport는 Pipeline 미지원으로 DeviceContext 직접 사용
	Pipeline->SetRenderTargets(1, ShadowMap->VarianceShadowRTV.GetAddressOf(), ShadowMap->ShadowDSV.Get());
	DeviceContext->RSSetViewports(1, &ShadowMap->ShadowViewport);

	// Clear the render target view (for VSM/SAVSM)
	const float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	DeviceContext->ClearRenderTargetView(ShadowMap->VarianceShadowRTV.Get(), ClearColor);
	DeviceContext->ClearDepthStencilView(ShadowMap->ShadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	// 2. Light별 캐싱된 rasterizer state 가져오기 (DepthBias 포함)
	ID3D11RasterizerState* RastState = GetOrCreateRasterizerState(Light);

	// 3. Pipeline을 통해 shadow rendering state 설정
	FPipelineInfo ShadowPipelineInfo = {
		DepthOnlyInputLayout,
		DepthOnlyVS,
		RastState,  // 캐싱된 state 사용 (매 프레임 생성/해제 방지)
		ShadowDepthStencilState,
		DepthOnlyPS,
		nullptr,  // No blend state
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	};
	Pipeline->UpdatePipeline(ShadowPipelineInfo);

	// 4. Light view-projection 계산
	FMatrix LightView, LightProj;
	CalculateDirectionalLightViewProj(Light, Meshes, LightView, LightProj);

	// Store the calculated shadow view-projection matrix in the light component
	FMatrix LightViewProj = LightView * LightProj;
	Light->SetShadowViewProjection(LightViewProj);

	// 5. 각 메시 렌더링
	for (auto Mesh : Meshes)
	{
		if (Mesh->IsVisible())
		{
			RenderMeshDepth(Mesh, LightView, LightProj);
		}
	}

	// 6. 상태 복원
	// RenderTarget과 DepthStencil 복원 (Pipeline API 사용)
	Pipeline->SetRenderTargets(1, &OriginalRTV, OriginalDSV);

	// Viewport 복원 (DeviceContext 직접 사용)
	DeviceContext->RSSetViewports(1, &OriginalViewport);

	// 임시 리소스 해제
	if (OriginalRTV)
		OriginalRTV->Release();
	if (OriginalDSV)
		OriginalDSV->Release();

	// Note: RastState는 캐싱되므로 여기서 해제하지 않음 (Release()에서 일괄 해제)
}

void FShadowMapPass::RenderSpotShadowMap(USpotLightComponent* Light,
	const TArray<UStaticMeshComponent*>& Meshes)
{
	FShadowMapResource* ShadowMap = GetOrCreateShadowMap(Light);
	if (!ShadowMap || !ShadowMap->IsValid())
		return;

	const auto& Renderer = URenderer::GetInstance();
	ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();

	// 0. 현재 상태 저장 (복원용)
	ID3D11RenderTargetView* OriginalRTV = nullptr;
	ID3D11DepthStencilView* OriginalDSV = nullptr;
	DeviceContext->OMGetRenderTargets(1, &OriginalRTV, &OriginalDSV);

	D3D11_VIEWPORT OriginalViewport;
	UINT NumViewports = 1;
	DeviceContext->RSGetViewports(&NumViewports, &OriginalViewport);

	// 1. Shadow render target 설정
	// Note: RenderTargets는 Pipeline API 사용, Viewport는 Pipeline 미지원으로 DeviceContext 직접 사용
	Pipeline->SetRenderTargets(1, ShadowMap->VarianceShadowRTV.GetAddressOf(), ShadowMap->ShadowDSV.Get());
	DeviceContext->RSSetViewports(1, &ShadowMap->ShadowViewport);

	// Clear the render target view (for VSM/SAVSM)
	const float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	DeviceContext->ClearRenderTargetView(ShadowMap->VarianceShadowRTV.Get(), ClearColor);
	DeviceContext->ClearDepthStencilView(ShadowMap->ShadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	// 2. Light별 캐싱된 rasterizer state 가져오기 (DepthBias 포함)
	ID3D11RasterizerState* RastState = GetOrCreateRasterizerState(Light);

	// 3. Pipeline을 통해 shadow rendering state 설정
	FPipelineInfo ShadowPipelineInfo = {
		DepthOnlyInputLayout,
		DepthOnlyVS,
		RastState,  // 캐싱된 state 사용 (매 프레임 생성/해제 방지)
		ShadowDepthStencilState,
		DepthOnlyPS,  
		nullptr,  // No blend state
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	};
	Pipeline->UpdatePipeline(ShadowPipelineInfo);

	// 4. Light view-projection 계산 (Perspective projection for cone-shaped frustum)
	FMatrix LightView, LightProj;
	CalculateSpotLightViewProj(Light, Meshes, LightView, LightProj);

	// Store the calculated shadow view-projection matrix in the light component
	FMatrix LightViewProj = LightView * LightProj;
	Light->SetShadowViewProjection(LightViewProj);  // Will be added to SpotLightComponent in Phase 6

	// 5. 각 메시 렌더링
	for (auto Mesh : Meshes)
	{
		if (Mesh->IsVisible())
		{
			RenderMeshDepth(Mesh, LightView, LightProj);
		}
	}

	// 6. 상태 복원
	// RenderTarget과 DepthStencil 복원 (Pipeline API 사용)
	Pipeline->SetRenderTargets(1, &OriginalRTV, OriginalDSV);

	// Viewport 복원 (DeviceContext 직접 사용)
	DeviceContext->RSSetViewports(1, &OriginalViewport);

	// 임시 리소스 해제
	if (OriginalRTV)
		OriginalRTV->Release();
	if (OriginalDSV)
		OriginalDSV->Release();

	// Note: RastState는 캐싱되므로 여기서 해제하지 않음 (Release()에서 일괄 해제)
}

void FShadowMapPass::RenderPointShadowMap(UPointLightComponent* Light,
	const TArray<UStaticMeshComponent*>& Meshes)
{
	FCubeShadowMapResource* ShadowMap = GetOrCreateCubeShadowMap(Light);
	if (!ShadowMap || !ShadowMap->IsValid())
		return;

	const auto& Renderer = URenderer::GetInstance();
	ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();

	// 0. 현재 상태 저장 (복원용)
	ID3D11RenderTargetView* OriginalRTV = nullptr;
	ID3D11DepthStencilView* OriginalDSV = nullptr;
	DeviceContext->OMGetRenderTargets(1, &OriginalRTV, &OriginalDSV);

	D3D11_VIEWPORT OriginalViewport;
	UINT NumViewports = 1;
	DeviceContext->RSGetViewports(&NumViewports, &OriginalViewport);

	// 1. Rasterizer state
	ID3D11RasterizerState* RastState = GetOrCreateRasterizerState(Light);

	// 2. Pipeline 설정 (Point Light는 linear distance를 depth로 저장하므로 pixel shader 필요)
	FPipelineInfo ShadowPipelineInfo = {
		PointLightShadowInputLayout,
		PointLightShadowVS,
		RastState,
		ShadowDepthStencilState,
		PointLightShadowPS,  // Pixel shader for linear distance output
		nullptr,  // No blend state
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	};
	Pipeline->UpdatePipeline(ShadowPipelineInfo);

	// 2.5. Point Light shadow params 설정 (light position, range)
	FPointLightShadowParams Params;
	Params.LightPosition = Light->GetWorldLocation();
	Params.LightRange = Light->GetAttenuationRadius();
	FRenderResourceFactory::UpdateConstantBufferData(PointLightShadowParamsBuffer, Params);
	Pipeline->SetConstantBuffer(2, EShaderType::PS, PointLightShadowParamsBuffer);

	// 3. 6개 View-Projection 계산
	FMatrix ViewProj[6];
	CalculatePointLightViewProj(Light, ViewProj);

	// 4. 6개 면 렌더링 (+X, -X, +Y, -Y, +Z, -Z)
	for (int Face = 0; Face < 6; Face++)
	{
		// 4-1. DSV 설정 (각 면)
		ID3D11RenderTargetView* NullRTV = nullptr;
		Pipeline->SetRenderTargets(1, &NullRTV, ShadowMap->ShadowDSVs[Face].Get());
		DeviceContext->RSSetViewports(1, &ShadowMap->ShadowViewport);
		DeviceContext->ClearDepthStencilView(
			ShadowMap->ShadowDSVs[Face].Get(),
			D3D11_CLEAR_DEPTH,
			1.0f,
			0
		);

		// 4-2. Constant buffer 업데이트 (각 면의 ViewProj)
		FShadowViewProjConstant CBData;
		CBData.ViewProjection = ViewProj[Face];
		FRenderResourceFactory::UpdateConstantBufferData(ShadowViewProjConstantBuffer, CBData);
		Pipeline->SetConstantBuffer(1, EShaderType::VS, ShadowViewProjConstantBuffer);

		// 4-3. 메시 렌더링
		for (auto Mesh : Meshes)
		{
			if (Mesh->IsVisible())
			{
				// Model transform 업데이트
				FMatrix WorldMatrix = Mesh->GetWorldTransformMatrix();
				FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, WorldMatrix);
				Pipeline->SetConstantBuffer(0, EShaderType::VS, ConstantBufferModel);

				// Vertex/Index buffer 바인딩
				ID3D11Buffer* VertexBuffer = Mesh->GetVertexBuffer();
				ID3D11Buffer* IndexBuffer = Mesh->GetIndexBuffer();
				uint32 IndexCount = Mesh->GetNumIndices();

				if (!VertexBuffer || !IndexBuffer || IndexCount == 0)
					continue;

				Pipeline->SetVertexBuffer(VertexBuffer, sizeof(FNormalVertex));
				Pipeline->SetIndexBuffer(IndexBuffer, 0);

				// Draw call
				Pipeline->DrawIndexed(IndexCount, 0, 0);
			}
		}
	}

	// 5. 상태 복원
	Pipeline->SetRenderTargets(1, &OriginalRTV, OriginalDSV);
	DeviceContext->RSSetViewports(1, &OriginalViewport);

	if (OriginalRTV)
		OriginalRTV->Release();
	if (OriginalDSV)
		OriginalDSV->Release();

	// Note: 6개 ViewProj를 PointLightComponent에 저장하는 것은 비효율적이므로,
	// Shader에서 Light position 기반으로 direction을 계산하도록 구현
}

void FShadowMapPass::CalculateDirectionalLightViewProj(UDirectionalLightComponent* Light,
	const TArray<UStaticMeshComponent*>& Meshes, FMatrix& OutView, FMatrix& OutProj)
{
	// 1. 모든 메시의 AABB를 포함하는 bounding box 계산
	FVector MinBounds(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector MaxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	bool bHasValidMeshes = false;
	for (auto Mesh : Meshes)
	{
		if (!Mesh->IsVisible())
			continue;

		// 메시의 world AABB 가져오기
		FVector WorldMin, WorldMax;
		Mesh->GetWorldAABB(WorldMin, WorldMax);

		MinBounds.X = std::min(MinBounds.X, WorldMin.X);
		MinBounds.Y = std::min(MinBounds.Y, WorldMin.Y);
		MinBounds.Z = std::min(MinBounds.Z, WorldMin.Z);

		MaxBounds.X = std::max(MaxBounds.X, WorldMax.X);
		MaxBounds.Y = std::max(MaxBounds.Y, WorldMax.Y);
		MaxBounds.Z = std::max(MaxBounds.Z, WorldMax.Z);

		bHasValidMeshes = true;
	}

	// 메시가 없으면 기본 행렬 반환
	if (!bHasValidMeshes)
	{
		OutView = FMatrix::Identity();
		OutProj = FMatrix::Identity();
		return;
	}

	// 2. Light direction 기준으로 view matrix 생성
	FVector LightDir = Light->GetForwardVector();
	if (LightDir.Length() < 1e-6f)
		LightDir = FVector(0, 0, -1);
	else
		LightDir = LightDir.GetNormalized();

	FVector SceneCenter = (MinBounds + MaxBounds) * 0.5f;
	float SceneRadius = (MaxBounds - MinBounds).Length() * 0.5f;

	// Light position은 scene 중심에서 light direction 반대로 충분히 멀리
	FVector LightPos = SceneCenter - LightDir * (SceneRadius + 50.0f);

	// Up vector 계산 (Z-Up, X-Forward, Y-Right Left-Handed 좌표계)
	FVector Up = FVector(0, 0, 1);  // Z-Up
	if (std::abs(LightDir.Z) > 0.99f)  // Light가 거의 수직(Z축과 평행)이면
		Up = FVector(1, 0, 0);  // X-Forward를 fallback으로

	OutView = ShadowMatrixHelper::CreateLookAtLH(LightPos, SceneCenter, Up);

	// 3. AABB를 light view space로 변환하여 orthographic projection 범위 계산
	FVector LightSpaceMin(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector LightSpaceMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	// AABB의 8개 코너를 light view space로 변환
	FVector Corners[8] = {
		FVector(MinBounds.X, MinBounds.Y, MinBounds.Z),
		FVector(MaxBounds.X, MinBounds.Y, MinBounds.Z),
		FVector(MinBounds.X, MaxBounds.Y, MinBounds.Z),
		FVector(MaxBounds.X, MaxBounds.Y, MinBounds.Z),
		FVector(MinBounds.X, MinBounds.Y, MaxBounds.Z),
		FVector(MaxBounds.X, MinBounds.Y, MaxBounds.Z),
		FVector(MinBounds.X, MaxBounds.Y, MaxBounds.Z),
		FVector(MaxBounds.X, MaxBounds.Y, MaxBounds.Z)
	};

	for (int i = 0; i < 8; i++)
	{
		FVector LightSpaceCorner = OutView.TransformPosition(Corners[i]);

		LightSpaceMin.X = std::min(LightSpaceMin.X, LightSpaceCorner.X);
		LightSpaceMin.Y = std::min(LightSpaceMin.Y, LightSpaceCorner.Y);
		LightSpaceMin.Z = std::min(LightSpaceMin.Z, LightSpaceCorner.Z);

		LightSpaceMax.X = std::max(LightSpaceMax.X, LightSpaceCorner.X);
		LightSpaceMax.Y = std::max(LightSpaceMax.Y, LightSpaceCorner.Y);
		LightSpaceMax.Z = std::max(LightSpaceMax.Z, LightSpaceCorner.Z);
	}

	// 4. Orthographic projection 생성
	// Scene 크기에 비례한 padding 사용 (씬이 크면 padding도 크게)
	float SceneSizeZ = LightSpaceMax.Z - LightSpaceMin.Z;
	float Padding = std::max(SceneSizeZ * 0.5f, 50.0f);  // 최소 50, 씬 깊이의 50%

	float Left = LightSpaceMin.X;
	float Right = LightSpaceMax.X;
	float Bottom = LightSpaceMin.Y;
	float Top = LightSpaceMax.Y;
	float Near = LightSpaceMin.Z - Padding;
	float Far = LightSpaceMax.Z + Padding;

	// Near는 음수가 되면 안됨 (orthographic에서는 괜찮지만, 안전을 위해)
	if (Near < 0.1f)
	{
		Near = 0.1f;
	}

	OutProj = ShadowMatrixHelper::CreateOrthoLH(Left, Right, Bottom, Top, Near, Far);
}

void FShadowMapPass::CalculateSpotLightViewProj(USpotLightComponent* Light,
	const TArray<UStaticMeshComponent*>& Meshes, FMatrix& OutView, FMatrix& OutProj)
{
	// 1. Light의 위치와 방향 가져오기
	FVector LightPos = Light->GetWorldLocation();
	FVector LightDir = Light->GetForwardVector();

	// LightDir이 거의 0이면 기본값 사용
	if (LightDir.Length() < 1e-6f)
		LightDir = FVector(1, 0, 0);  // X-Forward (engine default)
	else
		LightDir = LightDir.GetNormalized();

	// 2. View Matrix 생성: Light 위치에서 Direction 방향으로
	FVector Target = LightPos + LightDir;

	// Up vector 계산 (Z-Up, X-Forward, Y-Right Left-Handed 좌표계)
	FVector Up = FVector(0, 0, 1);  // Z-Up
	if (std::abs(LightDir.Z) > 0.99f)  // Light가 거의 수직(Z축과 평행)이면
		Up = FVector(1, 0, 0);  // X-Forward를 fallback으로

	OutView = ShadowMatrixHelper::CreateLookAtLH(LightPos, Target, Up);

	// 3. Perspective Projection 생성: Cone 모양의 frustum
	float FovY = Light->GetOuterConeAngle() * 2.0f;  // Full cone angle
	float Aspect = 1.0f;  // Square shadow map
	float Near = 1.0f;    // 너무 작으면 depth precision 문제
	float Far = Light->GetAttenuationRadius();  // Light range

	// FovY가 너무 작거나 크면 clamp (유효 범위: 0.1 ~ PI - 0.1)
	FovY = std::clamp(FovY, 0.1f, PI - 0.1f);

	// Far가 Near보다 작거나 같으면 기본값 사용
	if (Far <= Near)
		Far = Near + 10.0f;

	OutProj = ShadowMatrixHelper::CreatePerspectiveFovLH(FovY, Aspect, Near, Far);
}

void FShadowMapPass::CalculatePointLightViewProj(UPointLightComponent* Light, FMatrix OutViewProj[6])
{
	// 1. Light position
	FVector LightPos = Light->GetWorldLocation();

	// 2. Near/Far planes
	float Near = 1.0f;
	float Far = Light->GetAttenuationRadius();
	if (Far <= Near)
		Far = Near + 10.0f;

	// 3. Perspective projection (90 degree FOV for cube faces)
	FMatrix Proj = ShadowMatrixHelper::CreatePerspectiveFovLH(
		PI / 2.0f,  // 90 degrees FOV
		1.0f,       // Aspect ratio 1:1 (square)
		Near,
		Far
	);

	// 4. 6 directions for cube faces (DirectX cube map order)
	// Order: +X, -X, +Y, -Y, +Z, -Z
	FVector Directions[6] = {
		FVector(1.0f, 0.0f, 0.0f),   // +X
		FVector(-1.0f, 0.0f, 0.0f),  // -X
		FVector(0.0f, 1.0f, 0.0f),   // +Y
		FVector(0.0f, -1.0f, 0.0f),  // -Y
		FVector(0.0f, 0.0f, 1.0f),   // +Z
		FVector(0.0f, 0.0f, -1.0f)   // -Z
	};

	// 5. Up vectors for each direction (avoid gimbal lock)
	FVector Ups[6] = {
		FVector(0.0f, 1.0f, 0.0f),   // +X: Y-Up
		FVector(0.0f, 1.0f, 0.0f),   // -X: Y-Up
		FVector(0.0f, 0.0f, -1.0f),  // +Y: -Z-Up (looking up, so up is -Z)
		FVector(0.0f, 0.0f, 1.0f),   // -Y: +Z-Up (looking down, so up is +Z)
		FVector(0.0f, 1.0f, 0.0f),   // +Z: Y-Up
		FVector(0.0f, 1.0f, 0.0f)    // -Z: Y-Up
	};

	// 6. Calculate View-Projection for each face
	for (int i = 0; i < 6; i++)
	{
		FVector Target = LightPos + Directions[i];
		FMatrix View = ShadowMatrixHelper::CreateLookAtLH(LightPos, Target, Ups[i]);
		OutViewProj[i] = View * Proj;
	}
}

FShadowMapResource* FShadowMapPass::GetOrCreateShadowMap(ULightComponent* Light)
{
	FShadowMapResource* ShadowMap = nullptr;
	ID3D11Device* Device = URenderer::GetInstance().GetDevice();

	if (auto DirLight = Cast<UDirectionalLightComponent>(Light))
	{
		auto It = DirectionalShadowMaps.find(DirLight);
		if (It == DirectionalShadowMaps.end())
		{
			// 새로 생성
			ShadowMap = new FShadowMapResource();
			ShadowMap->Initialize(Device, Light->GetShadowMapResolution());
			DirectionalShadowMaps[DirLight] = ShadowMap;
		}
		else
		{
			ShadowMap = It->second;
			// 해상도 변경 체크
			if (ShadowMap->NeedsResize(Light->GetShadowMapResolution()))
			{
				ShadowMap->Initialize(Device, Light->GetShadowMapResolution());
			}
		}
	}
	else if (auto SpotLight = Cast<USpotLightComponent>(Light))
	{
		auto It = SpotShadowMaps.find(SpotLight);
		if (It == SpotShadowMaps.end())
		{
			ShadowMap = new FShadowMapResource();
			ShadowMap->Initialize(Device, Light->GetShadowMapResolution());
			SpotShadowMaps[SpotLight] = ShadowMap;
		}
		else
		{
			ShadowMap = It->second;
			if (ShadowMap->NeedsResize(Light->GetShadowMapResolution()))
			{
				ShadowMap->Initialize(Device, Light->GetShadowMapResolution());
			}
		}
	}

	return ShadowMap;
}

FCubeShadowMapResource* FShadowMapPass::GetOrCreateCubeShadowMap(UPointLightComponent* Light)
{
	FCubeShadowMapResource* ShadowMap = nullptr;
	ID3D11Device* Device = URenderer::GetInstance().GetDevice();

	auto It = PointShadowMaps.find(Light);
	if (It == PointShadowMaps.end())
	{
		// 새로 생성
		ShadowMap = new FCubeShadowMapResource();
		ShadowMap->Initialize(Device, Light->GetShadowMapResolution());
		PointShadowMaps[Light] = ShadowMap;
	}
	else
	{
		ShadowMap = It->second;
		// 해상도 변경 체크
		if (ShadowMap->NeedsResize(Light->GetShadowMapResolution()))
		{
			ShadowMap->Initialize(Device, Light->GetShadowMapResolution());
		}
	}

	return ShadowMap;
}

FCubeShadowMapResource* FShadowMapPass::GetPointShadowMap(UPointLightComponent* Light)
{
	auto It = PointShadowMaps.find(Light);
	return It != PointShadowMaps.end() ? It->second : nullptr;
}

void FShadowMapPass::RenderMeshDepth(UStaticMeshComponent* Mesh, const FMatrix& View, const FMatrix& Proj)
{
	// 1. Constant buffer 업데이트 (DepthOnlyVS.hlsl의 ViewProj 구조체에 맞춤)
	FShadowViewProjConstant CBData;
	CBData.ViewProjection = View * Proj;  // View와 Projection을 곱해서 전달
	FRenderResourceFactory::UpdateConstantBufferData(ShadowViewProjConstantBuffer, CBData);
	Pipeline->SetConstantBuffer(1, EShaderType::VS, ShadowViewProjConstantBuffer);

	// 2. Model transform 업데이트
	FMatrix WorldMatrix = Mesh->GetWorldTransformMatrix();
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, WorldMatrix);
	Pipeline->SetConstantBuffer(0, EShaderType::VS, ConstantBufferModel);

	// 3. Vertex/Index buffer 바인딩
	ID3D11Buffer* VertexBuffer = Mesh->GetVertexBuffer();
	ID3D11Buffer* IndexBuffer = Mesh->GetIndexBuffer();
	uint32 IndexCount = Mesh->GetNumIndices();

	if (!VertexBuffer || !IndexBuffer || IndexCount == 0)
		return;

	Pipeline->SetVertexBuffer(VertexBuffer, sizeof(FNormalVertex));
	Pipeline->SetIndexBuffer(IndexBuffer, 0);

	// 4. Draw call
	Pipeline->DrawIndexed(IndexCount, 0, 0);
}

void FShadowMapPass::Release()
{
	// Shadow maps 해제
	for (auto& Pair : DirectionalShadowMaps)
	{
		if (Pair.second)
		{
			Pair.second->Release();
			delete Pair.second;
		}
	}
	DirectionalShadowMaps.clear();

	// Rasterizer state 캐시 해제 (매 프레임 생성 방지를 위해 캐싱했던 states)
	for (auto& Pair : DirectionalRasterizerStates)
	{
		if (Pair.second)
			Pair.second->Release();
	}
	DirectionalRasterizerStates.clear();

	for (auto& Pair : SpotRasterizerStates)
	{
		if (Pair.second)
			Pair.second->Release();
	}
	SpotRasterizerStates.clear();

	for (auto& Pair : PointRasterizerStates)
	{
		if (Pair.second)
			Pair.second->Release();
	}
	PointRasterizerStates.clear();

	for (auto& Pair : SpotShadowMaps)
	{
		if (Pair.second)
		{
			Pair.second->Release();
			delete Pair.second;
		}
	}
	SpotShadowMaps.clear();

	for (auto& Pair : PointShadowMaps)
	{
		if (Pair.second)
		{
			Pair.second->Release();
			delete Pair.second;
		}
	}
	PointShadowMaps.clear();

	// States 해제
	if (ShadowDepthStencilState)
	{
		ShadowDepthStencilState->Release();
		ShadowDepthStencilState = nullptr;
	}

	if (ShadowRasterizerState)
	{
		ShadowRasterizerState->Release();
		ShadowRasterizerState = nullptr;
	}

	if (ShadowViewProjConstantBuffer)
	{
		ShadowViewProjConstantBuffer->Release();
		ShadowViewProjConstantBuffer = nullptr;
	}

	if (PointLightShadowParamsBuffer)
	{
		PointLightShadowParamsBuffer->Release();
		PointLightShadowParamsBuffer = nullptr;
	}

	// Shader와 InputLayout은 Renderer가 소유하므로 여기서 해제하지 않음
}

FShadowMapResource* FShadowMapPass::GetDirectionalShadowMap(UDirectionalLightComponent* Light)
{
	auto It = DirectionalShadowMaps.find(Light);
	return It != DirectionalShadowMaps.end() ? It->second : nullptr;
}

FShadowMapResource* FShadowMapPass::GetSpotShadowMap(USpotLightComponent* Light)
{
	auto It = SpotShadowMaps.find(Light);
	return It != SpotShadowMaps.end() ? It->second : nullptr;
}

ID3D11RasterizerState* FShadowMapPass::GetOrCreateRasterizerState(UDirectionalLightComponent* Light)
{
	// 이미 생성된 state가 있으면 재사용
	auto It = DirectionalRasterizerStates.find(Light);
	if (It != DirectionalRasterizerStates.end())
		return It->second;

	// 새로 생성
	const auto& Renderer = URenderer::GetInstance();
	D3D11_RASTERIZER_DESC RastDesc = {};
	ShadowRasterizerState->GetDesc(&RastDesc);

	// Light별 DepthBias 설정
	// DepthBias: Shadow acne (자기 그림자 아티팩트) 방지
	//   - 공식: FinalDepth = OriginalDepth + DepthBias*r + SlopeScaledDepthBias*MaxSlope
	//   - r: Depth buffer의 최소 표현 단위 (format dependent)
	//   - MaxSlope: max(|dz/dx|, |dz/dy|) - 표면의 기울기
	//   - 100000.0f: float → integer 변환 스케일
	RastDesc.DepthBias = static_cast<INT>(Light->GetShadowBias() * 100000.0f);
	RastDesc.SlopeScaledDepthBias = Light->GetShadowSlopeBias();

	ID3D11RasterizerState* NewState = nullptr;
	Renderer.GetDevice()->CreateRasterizerState(&RastDesc, &NewState);

	// 캐시에 저장
	DirectionalRasterizerStates[Light] = NewState;

	return NewState;
}

ID3D11RasterizerState* FShadowMapPass::GetOrCreateRasterizerState(USpotLightComponent* Light)
{
	// 이미 생성된 state가 있으면 재사용
	auto It = SpotRasterizerStates.find(Light);
	if (It != SpotRasterizerStates.end())
		return It->second;

	// 새로 생성
	const auto& Renderer = URenderer::GetInstance();
	D3D11_RASTERIZER_DESC RastDesc = {};
	ShadowRasterizerState->GetDesc(&RastDesc);

	// Light별 DepthBias 설정
	// DepthBias: Shadow acne (자기 그림자 아티팩트) 방지
	//   - 공식: FinalDepth = OriginalDepth + DepthBias*r + SlopeScaledDepthBias*MaxSlope
	//   - r: Depth buffer의 최소 표현 단위 (format dependent)
	//   - MaxSlope: max(|dz/dx|, |dz/dy|) - 표면의 기울기
	//   - 100000.0f: float → integer 변환 스케일
	RastDesc.DepthBias = static_cast<INT>(Light->GetShadowBias() * 100000.0f);
	RastDesc.SlopeScaledDepthBias = Light->GetShadowSlopeBias();

	ID3D11RasterizerState* NewState = nullptr;
	Renderer.GetDevice()->CreateRasterizerState(&RastDesc, &NewState);

	// 캐시에 저장
	SpotRasterizerStates[Light] = NewState;

	return NewState;
}

ID3D11RasterizerState* FShadowMapPass::GetOrCreateRasterizerState(UPointLightComponent* Light)
{
	// 이미 생성된 state가 있으면 재사용
	auto It = PointRasterizerStates.find(Light);
	if (It != PointRasterizerStates.end())
		return It->second;

	// 새로 생성
	const auto& Renderer = URenderer::GetInstance();
	D3D11_RASTERIZER_DESC RastDesc = {};
	ShadowRasterizerState->GetDesc(&RastDesc);

	// Light별 DepthBias 설정
	// DepthBias: Shadow acne (자기 그림자 아티팩트) 방지
	//   - 공식: FinalDepth = OriginalDepth + DepthBias*r + SlopeScaledDepthBias*MaxSlope
	//   - r: Depth buffer의 최소 표현 단위 (format dependent)
	//   - MaxSlope: max(|dz/dx|, |dz/dy|) - 표면의 기울기
	//   - 100000.0f: float → integer 변환 스케일
	RastDesc.DepthBias = static_cast<INT>(Light->GetShadowBias() * 100000.0f);
	RastDesc.SlopeScaledDepthBias = Light->GetShadowSlopeBias();

	ID3D11RasterizerState* NewState = nullptr;
	Renderer.GetDevice()->CreateRasterizerState(&RastDesc, &NewState);

	// 캐시에 저장
	PointRasterizerStates[Light] = NewState;

	return NewState;
}
