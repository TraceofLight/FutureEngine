#include "pch.h"
#include "Render/RenderPass/Public/LightPass.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Component/Public/AmbientLightComponent.h"
#include "component/Public/DirectionalLightComponent.h"
#include "component/Public/PointLightComponent.h"
#include "Component/Public/SpotLightComponent.h"
#include "Render/RenderPass/Public/ShadowMapPass.h"
#include "Source/Editor/Public/Camera.h"
#include "Render/UI/Overlay/Public/StatOverlay.h"

constexpr uint32 CSNumThread = 128;

FLightPass::FLightPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferCamera, 
	ID3D11InputLayout* InGizmoInputLayout, ID3D11VertexShader* InGizmoVS, ID3D11PixelShader* InGizmoPS,
	ID3D11DepthStencilState* InGizmoDSS) :
	FRenderPass(InPipeline, InConstantBufferCamera, nullptr)
{
	FRenderResourceFactory::CreateComputeShader(L"Asset/Shader/ViewClusterCS.hlsl", &ViewClusterCS);
	FRenderResourceFactory::CreateComputeShader(L"Asset/Shader/ClusteredLightCullingCS.hlsl", &ClusteredLightCullingCS);
	FRenderResourceFactory::CreateComputeShader(L"Asset/Shader/ClusterGizmoSetCS.hlsl", &ClusterGizmoSetCS);

	CreateOptionBuffers();
	GizmoInputLayout = InGizmoInputLayout;
	GizmoVS = InGizmoVS;
	GizmoPS = InGizmoPS;
	GizmoDSS = InGizmoDSS;
	CameraConstantBuffer = InConstantBufferCamera;

}
void FLightPass::CreateOptionBuffers()
{
	ViewClusterInfoConstantBuffer = FRenderResourceFactory::CreateConstantBuffer<FViewClusterInfo>();
	ClusterSliceInfoConstantBuffer = FRenderResourceFactory::CreateConstantBuffer<FClusterSliceInfo>();
	LightCountInfoConstantBuffer = FRenderResourceFactory::CreateConstantBuffer<FLightCountInfo>();
	GlobalLightConstantBuffer = FRenderResourceFactory::CreateConstantBuffer<FGlobalLightConstant>();
	PointLightStructuredBuffer = FRenderResourceFactory::CreateStructuredBuffer<FPointLightInfo>(PointLightBufferCount);
	SpotLightStructuredBuffer = FRenderResourceFactory::CreateStructuredBuffer<FSpotLightInfo>(SpotLightBufferCount);
	ClusterAABBRWStructuredBuffer = FRenderResourceFactory::CreateRWStructuredBuffer(24, GetClusterCount());
	PointLightIndicesRWStructuredBuffer = FRenderResourceFactory::CreateRWStructuredBuffer(4, GetClusterCount() * LightMaxCountPerCluster);
	SpotLightIndicesRWStructuredBuffer = FRenderResourceFactory::CreateRWStructuredBuffer(4, GetClusterCount() * LightMaxCountPerCluster);

	FRenderResourceFactory::CreateStructuredShaderResourceView(PointLightStructuredBuffer, &PointLightStructuredBufferSRV);
	FRenderResourceFactory::CreateStructuredShaderResourceView(SpotLightStructuredBuffer, &SpotLightStructuredBufferSRV);
	FRenderResourceFactory::CreateStructuredShaderResourceView(ClusterAABBRWStructuredBuffer, &ClusterAABBRWStructuredBufferSRV);
	FRenderResourceFactory::CreateUnorderedAccessView(ClusterAABBRWStructuredBuffer, &ClusterAABBRWStructuredBufferUAV);
	FRenderResourceFactory::CreateStructuredShaderResourceView(PointLightIndicesRWStructuredBuffer, &PointLightIndicesRWStructuredBufferSRV);
	FRenderResourceFactory::CreateStructuredShaderResourceView(SpotLightIndicesRWStructuredBuffer, &SpotLightIndicesRWStructuredBufferSRV);
	FRenderResourceFactory::CreateUnorderedAccessView(PointLightIndicesRWStructuredBuffer, &PointLightIndicesRWStructuredBufferUAV);
	FRenderResourceFactory::CreateUnorderedAccessView(SpotLightIndicesRWStructuredBuffer, &SpotLightIndicesRWStructuredBufferUAV);

	ClusterGizmoVertexRWStructuredBuffer = FRenderResourceFactory::CreateRWStructuredBuffer(28, (GetClusterCount() + 1) * 8);
	FRenderResourceFactory::CreateUnorderedAccessView(ClusterGizmoVertexRWStructuredBuffer, &ClusterGizmoVertexRWStructuredBufferUAV);
	FRenderResourceFactory::CreateStructuredShaderResourceView(ClusterGizmoVertexRWStructuredBuffer, &ClusterGizmoVertexRWStructuredBufferSRV);
}

void FLightPass::ReleaseOptionBuffers()
{
	SafeRelease(PointLightStructuredBufferSRV);
	SafeRelease(SpotLightStructuredBufferSRV);
	SafeRelease(ClusterAABBRWStructuredBufferSRV);
	SafeRelease(ClusterAABBRWStructuredBufferUAV);
	SafeRelease(PointLightIndicesRWStructuredBufferSRV);
	SafeRelease(PointLightIndicesRWStructuredBufferUAV);
	SafeRelease(SpotLightIndicesRWStructuredBufferSRV);
	SafeRelease(SpotLightIndicesRWStructuredBufferUAV);
	SafeRelease(ClusterGizmoVertexRWStructuredBufferUAV);
	SafeRelease(ClusterGizmoVertexRWStructuredBufferSRV);

	SafeRelease(ViewClusterInfoConstantBuffer);
	SafeRelease(ClusterSliceInfoConstantBuffer);
	SafeRelease(LightCountInfoConstantBuffer);
	SafeRelease(GlobalLightConstantBuffer);
	SafeRelease(PointLightStructuredBuffer);
	SafeRelease(SpotLightStructuredBuffer);
	SafeRelease(ClusterAABBRWStructuredBuffer);
	SafeRelease(PointLightIndicesRWStructuredBuffer);
	SafeRelease(SpotLightIndicesRWStructuredBuffer);
	SafeRelease(ClusterGizmoVertexRWStructuredBuffer);
}
bool FLightPass::IsChangeOption()
{
	static uint32 StaticClusterSliceNumX = ClusterSliceNumX;
	static uint32 StaticClusterSliceNumY = ClusterSliceNumY;
	static uint32 StaticClusterSliceNumZ = ClusterSliceNumZ;
	static uint32 StaticLightMaxCountPerCluster = LightMaxCountPerCluster;

	if ((StaticClusterSliceNumX == ClusterSliceNumX && StaticClusterSliceNumY == ClusterSliceNumY &&
		StaticClusterSliceNumZ == ClusterSliceNumZ && StaticLightMaxCountPerCluster == LightMaxCountPerCluster) == false)
	{
		StaticClusterSliceNumX = ClusterSliceNumX;
		StaticClusterSliceNumY = ClusterSliceNumY;
		StaticClusterSliceNumZ = ClusterSliceNumZ;
		StaticLightMaxCountPerCluster = LightMaxCountPerCluster;
		return true;
	}
	return false;
}
void FLightPass::Execute(FRenderingContext& Context)
{
	if (IsChangeOption())
	{
		ReleaseOptionBuffers();
		CreateOptionBuffers();
	}
	Pipeline->SetConstantBuffer(3, EShaderType::VS | EShaderType::PS, nullptr);
	Pipeline->SetConstantBuffer(4, EShaderType::VS | EShaderType::PS, nullptr);
	Pipeline->SetConstantBuffer(5, EShaderType::VS | EShaderType::PS, nullptr);

	Pipeline->SetShaderResourceView(6, EShaderType::VS | EShaderType::PS, nullptr);
	Pipeline->SetShaderResourceView(7, EShaderType::VS | EShaderType::PS, nullptr);
	Pipeline->SetShaderResourceView(8, EShaderType::VS | EShaderType::PS, nullptr);
	Pipeline->SetShaderResourceView(9, EShaderType::VS | EShaderType::PS, nullptr);


	// Setup lighting constant buffer from scene lights
	FGlobalLightConstant GlobalLightData = {};
	TArray<FPointLightInfo> PointLightDatas;
	TArray<FSpotLightInfo> SpotLightDatas;
	// 수정 필요: Context에서 가져오기
	if (!Context.AmbientLights.IsEmpty())
	{
		UAmbientLightComponent* VisibleAmbient = nullptr;
		for (UAmbientLightComponent* Ambient : Context.AmbientLights)
		{
			if (Ambient != nullptr && Ambient->GetVisible() && Ambient->GetLightEnabled())
			{
				VisibleAmbient = Ambient;
				break;
			}
		}
		if (VisibleAmbient != nullptr)
		{
			GlobalLightData.Ambient = VisibleAmbient->GetAmbientLightInfo();
		}
	}

	if (!Context.DirectionalLights.IsEmpty())
	{
		UDirectionalLightComponent* VisibleDirectional = nullptr;
		for (UDirectionalLightComponent* Directional : Context.DirectionalLights)
		{
			if (Directional != nullptr && Directional->GetVisible() && Directional->GetLightEnabled())
			{
				VisibleDirectional = Directional;
				break;
			}
		}
		if (VisibleDirectional != nullptr)
		{
			GlobalLightData.Directional = VisibleDirectional->GetDirectionalLightInfo();
		}
	}

	// Fill point lights from scene
	int PointLightComponentCount = Context.PointLights.Num();
	PointLightDatas.Reserve(PointLightComponentCount);
	for (int32 i = 0; i < PointLightComponentCount; ++i)
	{
		UPointLightComponent* Light = Context.PointLights[i];
		if (!Light || !Light->GetVisible() || !Light->GetLightEnabled()) continue;
		PointLightDatas.Add(Light->GetPointlightInfo());
	}
	// 5. Spot Lights 배열 채우기 (최대 NUM_SPOT_LIGHT개)
	int SpotLightComponentCount = Context.SpotLights.Num();
	SpotLightDatas.Reserve(SpotLightComponentCount);
	int CurSpotLightIdx = 0;
	for (int32 i = 0; i < SpotLightComponentCount; ++i)
	{
		USpotLightComponent* Light = Context.SpotLights[i];
		if (!Light || !Light->GetVisible() || !Light->GetLightEnabled()) continue;
		SpotLightDatas.Add(Light->GetSpotLightInfo());
	}

	uint32 PointLightCount = static_cast<uint32>(PointLightDatas.Num());
	uint32 SpotLightCount = static_cast<uint32>(SpotLightDatas.Num());
	// 최대갯수 재할당
	if (PointLightBufferCount < PointLightCount)
	{
		while (PointLightBufferCount < PointLightCount)
		{
			PointLightBufferCount = PointLightBufferCount << 1;
		}
		SafeRelease(PointLightStructuredBuffer);
		PointLightStructuredBuffer = FRenderResourceFactory::CreateStructuredBuffer<FPointLightInfo>(PointLightBufferCount);
		SafeRelease(PointLightStructuredBufferSRV);
		FRenderResourceFactory::CreateStructuredShaderResourceView(PointLightStructuredBuffer, &PointLightStructuredBufferSRV);
	}
	if (SpotLightBufferCount < SpotLightCount)
	{
		while (SpotLightBufferCount < SpotLightCount)
		{
			SpotLightBufferCount = SpotLightBufferCount << 1;
		}
		SafeRelease(SpotLightStructuredBuffer);
		SpotLightStructuredBuffer = FRenderResourceFactory::CreateStructuredBuffer<FSpotLightInfo>(SpotLightBufferCount);
		SafeRelease(SpotLightStructuredBufferSRV);
		FRenderResourceFactory::CreateStructuredShaderResourceView(SpotLightStructuredBuffer, &SpotLightStructuredBufferSRV);
	}

	int ThreadGroupCount = (ClusterSliceNumX * ClusterSliceNumY * ClusterSliceNumZ + CSNumThread - 1) / CSNumThread;


	FRenderResourceFactory::UpdateConstantBufferData(GlobalLightConstantBuffer, GlobalLightData);
	FRenderResourceFactory::UpdateStructuredBuffer(PointLightStructuredBuffer, PointLightDatas);
	FRenderResourceFactory::UpdateStructuredBuffer(SpotLightStructuredBuffer, SpotLightDatas);

	// Cluster AABB Set
	FCameraConstants Inv = Context.CurrentCamera->GetFViewProjConstantsInverse();
	FMatrix ProjectionInv = Inv.Projection;
	FMatrix ViewInv = Inv.View;
	FMatrix ViewMatrix = Context.CurrentCamera->GetFViewProjConstants().View;
	float CamNear = Context.CurrentCamera->GetNearZ();
	float CamFar = Context.CurrentCamera->GetFarZ();
	float Aspect = Context.CurrentCamera->GetAspect();
	float fov = Context.CurrentCamera->GetFovY();
	uint32 Orthographic = ECameraType::ECT_Orthographic == Context.CurrentCamera->GetCameraType() ? 1 : 0;

	FRenderResourceFactory::UpdateConstantBufferData(ViewClusterInfoConstantBuffer,
		FViewClusterInfo{ ProjectionInv, ViewInv, ViewMatrix, CamNear,CamFar,Aspect,fov});
	FRenderResourceFactory::UpdateConstantBufferData(ClusterSliceInfoConstantBuffer,
		FClusterSliceInfo{ ClusterSliceNumX, ClusterSliceNumY,ClusterSliceNumZ,LightMaxCountPerCluster,bSpotIntersectOpti, Orthographic });
	FRenderResourceFactory::UpdateConstantBufferData(LightCountInfoConstantBuffer,
		FLightCountInfo{ PointLightCount, SpotLightCount });
	Pipeline->SetConstantBuffer(0, EShaderType::CS, ViewClusterInfoConstantBuffer);
	Pipeline->SetConstantBuffer(1, EShaderType::CS, ClusterSliceInfoConstantBuffer);
	Pipeline->SetConstantBuffer(2, EShaderType::CS, LightCountInfoConstantBuffer);

	Pipeline->SetUnorderedAccessView(0, ClusterAABBRWStructuredBufferUAV);
	Pipeline->DispatchCS(ViewClusterCS, ThreadGroupCount, 1, 1);


	// Light 분류
	Pipeline->SetUnorderedAccessView(0, PointLightIndicesRWStructuredBufferUAV);
	Pipeline->SetUnorderedAccessView(1, SpotLightIndicesRWStructuredBufferUAV);
	Pipeline->SetShaderResourceView(0, EShaderType::CS, ClusterAABBRWStructuredBufferSRV);
	Pipeline->SetShaderResourceView(1, EShaderType::CS, PointLightStructuredBufferSRV);
	Pipeline->SetShaderResourceView(2, EShaderType::CS, SpotLightStructuredBufferSRV);
	Pipeline->DispatchCS(ClusteredLightCullingCS, ThreadGroupCount, 1, 1);
	Pipeline->SetUnorderedAccessView(0, nullptr);
	Pipeline->SetUnorderedAccessView(1, nullptr);
	Pipeline->SetShaderResourceView(0, EShaderType::CS, nullptr);
	Pipeline->SetShaderResourceView(1, EShaderType::CS, nullptr);
	Pipeline->SetShaderResourceView(2, EShaderType::CS, nullptr);

	// 클러스터 기즈모 제작
	if (bClusterGizmoSet == false) 
	{
		bClusterGizmoSet = true;
		Pipeline->SetUnorderedAccessView(0, ClusterGizmoVertexRWStructuredBufferUAV);
		Pipeline->SetShaderResourceView(0, EShaderType::CS, ClusterAABBRWStructuredBufferSRV);
		Pipeline->SetShaderResourceView(1, EShaderType::CS, PointLightStructuredBufferSRV);
		Pipeline->SetShaderResourceView(2, EShaderType::CS, SpotLightStructuredBufferSRV);
		Pipeline->SetShaderResourceView(3, EShaderType::CS, PointLightIndicesRWStructuredBufferSRV);
		Pipeline->SetShaderResourceView(4, EShaderType::CS, SpotLightIndicesRWStructuredBufferSRV);
		Pipeline->DispatchCS(ClusterGizmoSetCS, ThreadGroupCount, 1, 1);
		Pipeline->SetUnorderedAccessView(0, nullptr);
		Pipeline->SetShaderResourceView(0, EShaderType::CS, nullptr);
		Pipeline->SetShaderResourceView(1, EShaderType::CS, nullptr);
		Pipeline->SetShaderResourceView(2, EShaderType::CS, nullptr);
		Pipeline->SetShaderResourceView(3, EShaderType::CS, nullptr);
		Pipeline->SetShaderResourceView(4, EShaderType::CS, nullptr);
	}
	
	if (bRenderClusterGizmo) 
	{
		//클러스터 기즈모 출력
		ID3D11RasterizerState* RS = FRenderResourceFactory::GetRasterizerState(FRenderState());
		FPipelineInfo PipelineInfo = { nullptr, GizmoVS, RS, GizmoDSS, GizmoPS, nullptr, D3D11_PRIMITIVE_TOPOLOGY_LINELIST };
		Pipeline->UpdatePipeline(PipelineInfo);
		Pipeline->SetConstantBuffer(1, EShaderType::VS, ConstantBufferCamera);

		Pipeline->SetShaderResourceView(0, EShaderType::VS, ClusterGizmoVertexRWStructuredBufferSRV);

		URenderer& Renderer = URenderer::GetInstance();
		const auto& DeviceResources = Renderer.GetDeviceResources();
		ID3D11RenderTargetView* RTV = nullptr;
		if (Renderer.GetFXAA())
		{
			RTV = DeviceResources->GetSceneColorRenderTargetView();
		}
		else
		{
			RTV = DeviceResources->GetRenderTargetView();
		}
		ID3D11RenderTargetView* RTVs[] = { RTV };
		ID3D11DepthStencilView* DSV = DeviceResources->GetDepthStencilView();
		Pipeline->SetRenderTargets(1, RTVs, DSV);
		Pipeline->Draw((GetClusterCount() + 1) * 24, 0);
		Pipeline->SetShaderResourceView(0, EShaderType::VS, nullptr);

	}

	// 다음 메쉬 드로우를 위한 빛 관련 정보 업로드
	Pipeline->SetConstantBuffer(3, EShaderType::VS | EShaderType::PS, GlobalLightConstantBuffer);
	Pipeline->SetConstantBuffer(4, EShaderType::VS | EShaderType::PS, ClusterSliceInfoConstantBuffer);
	Pipeline->SetConstantBuffer(5, EShaderType::VS | EShaderType::PS, LightCountInfoConstantBuffer);

	Pipeline->SetShaderResourceView(6, EShaderType::VS | EShaderType::PS, PointLightIndicesRWStructuredBufferSRV);
	Pipeline->SetShaderResourceView(7, EShaderType::VS | EShaderType::PS, SpotLightIndicesRWStructuredBufferSRV);
	Pipeline->SetShaderResourceView(8, EShaderType::VS | EShaderType::PS, PointLightStructuredBufferSRV);
	Pipeline->SetShaderResourceView(9, EShaderType::VS | EShaderType::PS, SpotLightStructuredBufferSRV);

	// 라이트 통계 기록
	uint32 DirectionalCount = 0;
	uint32 AmbientCount = 0;
	if (!Context.DirectionalLights.IsEmpty())
	{
		for (UDirectionalLightComponent* Light : Context.DirectionalLights)
		{
			if (Light && Light->GetVisible() && Light->GetLightEnabled())
			{
				++DirectionalCount;
			}
		}
	}
	if (!Context.AmbientLights.IsEmpty())
	{
		for (UAmbientLightComponent* Light : Context.AmbientLights)
		{
			if (Light && Light->GetVisible() && Light->GetLightEnabled())
			{
				++AmbientCount;
			}
		}
	}

	// 섀도우맵 메모리 및 아틀라스 정보 계산
	uint64 ShadowMapMemory = 0;
	uint64 RenderTargetMemory = 0;
	uint32 UsedAtlasTiles = 0;
	uint32 MaxAtlasTiles = 0;

	URenderer& Renderer = URenderer::GetInstance();
	FShadowMapPass* ShadowMapPass = Renderer.GetShadowMapPass();
	if (ShadowMapPass)
	{
		ShadowMapMemory = ShadowMapPass->GetTotalShadowMapMemory();
		UsedAtlasTiles = ShadowMapPass->GetUsedAtlasTileCount();
		MaxAtlasTiles = ShadowMapPass->GetMaxAtlasTileCount();
	}

	// 렌더 타겟 메모리 계산
	UDeviceResources* DeviceResources = Renderer.GetDeviceResources();
	if (DeviceResources)
	{
		RenderTargetMemory = DeviceResources->GetTotalRenderTargetMemory();
	}

	UStatOverlay::GetInstance().RecordShadowStats(DirectionalCount, PointLightCount, SpotLightCount, AmbientCount, ShadowMapMemory, RenderTargetMemory, UsedAtlasTiles, MaxAtlasTiles);
}


void FLightPass::Release()
{
	SafeRelease(ViewClusterCS);
	SafeRelease(ClusteredLightCullingCS);
	SafeRelease(ClusterGizmoSetCS);

	ReleaseOptionBuffers();
}