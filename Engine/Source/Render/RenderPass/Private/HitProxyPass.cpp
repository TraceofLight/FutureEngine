#include "pch.h"
#include "Render/RenderPass/Public/HitProxyPass.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/DeviceResources.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Component/Public/EditorIconComponent.h"
#include "Editor/Public/Camera.h"
#include "Render/HitProxy/Public/HitProxy.h"

FHitProxyPass::FHitProxyPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferCamera, ID3D11Buffer* InConstantBufferModel,
                             ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS)
	: FRenderPass(InPipeline, InConstantBufferCamera, InConstantBufferModel), VS(InVS), PS(InPS), InputLayout(InLayout), DS(InDS)
{
	// HitProxyColor 상수 버퍼 생성 (b2 슬롯)
	ConstantBufferHitProxyColor = FRenderResourceFactory::CreateConstantBuffer<FVector4>();
}

void FHitProxyPass::Execute(FRenderingContext& Context)
{
	if (!VS || !PS || !InputLayout)
	{
		return;
	}

	const auto& Renderer = URenderer::GetInstance();
	const auto& DeviceResources = Renderer.GetDeviceResources();

	// Viewport 설정 (Context에서 전달받은 Viewport 사용)
	DeviceResources->GetDeviceContext()->RSSetViewports(1, &Context.Viewport);

	// 카메라 상수 버퍼 업데이트
	if (Context.CurrentCamera)
	{
		FCameraConstants CameraConstants;
		CameraConstants.View = Context.CurrentCamera->GetFViewProjConstants().View;
		CameraConstants.Projection = Context.CurrentCamera->GetFViewProjConstants().Projection;
		CameraConstants.ViewWorldLocation = Context.CurrentCamera->GetLocation();
		CameraConstants.NearClip = Context.CurrentCamera->GetNearZ();
		CameraConstants.FarClip = Context.CurrentCamera->GetFarZ();
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferCamera, CameraConstants);
	}

	// RenderState 설정 (StaticMeshPass와 동일)
	FRenderState RenderState = UStaticMeshComponent::GetClassDefaultRenderState();

	ID3D11RasterizerState* RS = FRenderResourceFactory::GetRasterizerState(RenderState);

	// Pipeline 설정
	FPipelineInfo PipelineInfo = { InputLayout, VS, RS, DS, PS, nullptr, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
	Pipeline->UpdatePipeline(PipelineInfo);

	// 상수 버퍼 바인딩
	Pipeline->SetConstantBuffer(0, EShaderType::VS, ConstantBufferModel);
	Pipeline->SetConstantBuffer(1, EShaderType::VS | EShaderType::PS, ConstantBufferCamera);
	Pipeline->SetConstantBuffer(2, EShaderType::PS, ConstantBufferHitProxyColor);

	// HitProxy RTV/DSV 설정
	ID3D11RenderTargetView* HitProxyRTV = DeviceResources->GetHitProxyRenderTargetView();
	ID3D11DepthStencilView* DSV = DeviceResources->GetDepthStencilView();
	Pipeline->SetRenderTargets(1, &HitProxyRTV, DSV);

	// HitProxy 텍스처 클리어 (검은색 = 배경)
	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	DeviceResources->GetDeviceContext()->ClearRenderTargetView(HitProxyRTV, ClearColor);
	DeviceResources->GetDeviceContext()->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// HitProxyManager 초기화
	FHitProxyManager& HitProxyManager = FHitProxyManager::GetInstance();
	HitProxyManager.ClearAllHitProxies();

	// EditorIcon 컴포넌트 렌더링
	for (UEditorIconComponent* IconComp : Context.EditorIcons)
	{
		if (!IconComp->IsVisible())
		{
			continue;
		}

		// Billboard 카메라 정렬 적용
		IconComp->FaceCamera(Context.CurrentCamera->GetForward());

		// HitProxy ID 할당
		HComponent* ComponentProxy = new HComponent(IconComp, InvalidHitProxyId);
		FHitProxyId ProxyId = HitProxyManager.AllocateHitProxyId(ComponentProxy);

		// HitProxyColor 상수 버퍼 업데이트
		FVector4 ProxyColor = ProxyId.GetColor();
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferHitProxyColor, ProxyColor);
		Pipeline->SetConstantBuffer(2, EShaderType::PS, ConstantBufferHitProxyColor);

		// Vertex/Index 버퍼 바인딩
		Pipeline->SetVertexBuffer(IconComp->GetVertexBuffer(), sizeof(FNormalVertex));
		Pipeline->SetIndexBuffer(IconComp->GetIndexBuffer(), 0);

		// Model 상수 버퍼 업데이트
		FMatrix WorldMatrix;
		if (IconComp->IsScreenSizeScaled())
		{
			FVector FixedWorldScale = IconComp->GetRelativeScale3D();
			FVector IconLocation = IconComp->GetWorldLocation();
			FQuaternion IconRotation = IconComp->GetWorldRotationAsQuaternion();
			WorldMatrix = FMatrix::GetModelMatrix(IconLocation, IconRotation, FixedWorldScale);
		}
		else
		{
			WorldMatrix = IconComp->GetWorldTransformMatrix();
		}
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, WorldMatrix);
		Pipeline->SetConstantBuffer(0, EShaderType::VS, ConstantBufferModel);

		// 렌더링
		Pipeline->DrawIndexed(IconComp->GetNumIndices(), 0, 0);
	}

	// StaticMesh 컴포넌트 렌더링
	FStaticMesh* CurrentMeshAsset = nullptr;

	for (UStaticMeshComponent* MeshComp : Context.StaticMeshes)
	{
		if (!MeshComp->IsVisible()) { continue; }
		if (!MeshComp->GetStaticMesh()) { continue; }

		FStaticMesh* MeshAsset = MeshComp->GetStaticMesh()->GetStaticMeshAsset();
		if (!MeshAsset) { continue; }

		// HitProxy ID 할당
		HComponent* ComponentProxy = new HComponent(MeshComp, InvalidHitProxyId);
		FHitProxyId ProxyId = HitProxyManager.AllocateHitProxyId(ComponentProxy);

		// HitProxyColor 상수 버퍼 업데이트
		FVector4 ProxyColor = ProxyId.GetColor();
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferHitProxyColor, ProxyColor);
		Pipeline->SetConstantBuffer(2, EShaderType::PS, ConstantBufferHitProxyColor);

		// 메쉬가 변경되면 버퍼 바인딩
		if (CurrentMeshAsset != MeshAsset)
		{
			CurrentMeshAsset = MeshAsset;
			Pipeline->SetVertexBuffer(MeshComp->GetVertexBuffer(), sizeof(FNormalVertex));
			Pipeline->SetIndexBuffer(MeshComp->GetIndexBuffer(), 0);
		}

		// Model 상수 버퍼 업데이트 (World Transform)
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, MeshComp->GetWorldTransformMatrix());
		Pipeline->SetConstantBuffer(0, EShaderType::VS, ConstantBufferModel);

		// 렌더링
		Pipeline->DrawIndexed(static_cast<uint32>(MeshAsset->Indices.Num()), 0, 0);
	}
}

void FHitProxyPass::Release()
{
	if (ConstantBufferHitProxyColor)
	{
		ConstantBufferHitProxyColor->Release();
		ConstantBufferHitProxyColor = nullptr;
	}
}

void FHitProxyPass::SetShaders(ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout)
{
	VS = InVS;
	PS = InPS;
	InputLayout = InLayout;
}
