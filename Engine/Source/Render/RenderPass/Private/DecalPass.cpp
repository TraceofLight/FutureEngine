#include "pch.h"
#include "Component/Public/DecalComponent.h"
#include "Global/Octree.h"
#include "Level/Public/Level.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Physics/Public/OBB.h"
#include "Render/RenderPass/Public/DecalPass.h"
#include "Render/RenderPass/Public/RenderingContext.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Texture/Public/Texture.h"
#include "Render/UI/Overlay/Public/StatOverlay.h"


namespace
{
    // 컴포넌트 인덱싱용 헬퍼
    static inline float Comp(const FVector& v, int i) { return (i == 0) ? v.X : (i == 1) ? v.Y : v.Z; }
    static inline float Abs(float v) { return v >= 0.f ? v : -v; }

    // row-major, row-vector 가정
    bool Intersects(const FOBB& OBB, const FAABB& AABB)
    {
        constexpr float EPS = 1e-6f;

        // 1) AABB 중심/반지름
        const FVector AABBCenter = (AABB.Min + AABB.Max) * 0.5f;
        const FVector AABBHalf = (AABB.Max - AABB.Min) * 0.5f;

        // 2) OBB 축(Ux,Uy,Uz)과 축 스케일 길이 추출
        //    - ScaleRotation의 각 "행"에 스케일이 섞여 있음
        //    - 행 길이 si를 구해 b_i = Extents_i * si 로 월드 반지름 만들고,
        //      축은 행을 si로 나눠 정규화해서 U[i]로 사용
        FVector OBBAxisRowX(OBB.ScaleRotation.Data[0][0], OBB.ScaleRotation.Data[0][1], OBB.ScaleRotation.Data[0][2]);
        FVector OBBAxisRowY(OBB.ScaleRotation.Data[1][0], OBB.ScaleRotation.Data[1][1], OBB.ScaleRotation.Data[1][2]);
        FVector OBBAxisRowZ(OBB.ScaleRotation.Data[2][0], OBB.ScaleRotation.Data[2][1], OBB.ScaleRotation.Data[2][2]);

        const float OBBAxisScaleX = std::sqrt(OBBAxisRowX.LengthSquared());  // 축0의 스케일 길이
        const float OBBAxisScaleY = std::sqrt(OBBAxisRowY.LengthSquared());  // 축1의 스케일 길이
        const float OBBAxisScaleZ = std::sqrt(OBBAxisRowZ.LengthSquared());  // 축2의 스케일 길이


        // 여기서 연산이 많이 들어감. 현재 구조 상 남겨둠
        FVector U[3] = {
            (OBBAxisScaleX > 0.f) ? FVector(OBBAxisRowX.X / OBBAxisScaleX, OBBAxisRowX.Y / OBBAxisScaleX, OBBAxisRowX.Z / OBBAxisScaleX) : OBBAxisRowX, // Ux
            (OBBAxisScaleY > 0.f) ? FVector(OBBAxisRowY.X / OBBAxisScaleY, OBBAxisRowY.Y / OBBAxisScaleY, OBBAxisRowY.Z / OBBAxisScaleY) : OBBAxisRowY, // Uy
            (OBBAxisScaleZ > 0.f) ? FVector(OBBAxisRowZ.X / OBBAxisScaleZ, OBBAxisRowZ.Y / OBBAxisScaleZ, OBBAxisRowZ.Z / OBBAxisScaleZ) : OBBAxisRowZ  // Uz
        };

        // OBB 월드 반지름(half-extent) b = Extents * 축길이
        const float OBBExtents[3] = {
            OBB.Extents.X * OBBAxisScaleX,
            OBB.Extents.Y * OBBAxisScaleY,
            OBB.Extents.Z * OBBAxisScaleZ
        };

        // 3) R[i][j] = dot(World_i, U_j)  (World_i는 표준기저 → U_j의 해당 성분과 동일)
        float R[3][3], AbsR[3][3];
        for (int j = 0; j < 3; ++j) {
            R[0][j] = U[j].X;  AbsR[0][j] = Abs(R[0][j]) + EPS;
            R[1][j] = U[j].Y;  AbsR[1][j] = Abs(R[1][j]) + EPS;
            R[2][j] = U[j].Z;  AbsR[2][j] = Abs(R[2][j]) + EPS;
        }

        // 4) t = Cb - Ca
        const FVector Distance = OBB.Center - AABBCenter;

        // 5) 월드축(AABB 3축) 테스트 — R의 '행' 사용
        {
            float RightAABBValue, RightOBBValue;

            // ex
            RightAABBValue = AABBHalf.X;
            RightOBBValue = OBBExtents[0] * AbsR[0][0] + OBBExtents[1] * AbsR[0][1] + OBBExtents[2] * AbsR[0][2];
            if (Abs(Distance.X) > RightAABBValue + RightOBBValue) return false;

            // ey
            RightAABBValue = AABBHalf.Y;
            RightOBBValue = OBBExtents[0] * AbsR[1][0] + OBBExtents[1] * AbsR[1][1] + OBBExtents[2] * AbsR[1][2];
            if (Abs(Distance.Y) > RightAABBValue + RightOBBValue) return false;

            // ez
            RightAABBValue = AABBHalf.Z;
            RightOBBValue = OBBExtents[0] * AbsR[2][0] + OBBExtents[1] * AbsR[2][1] + OBBExtents[2] * AbsR[2][2];
            if (Abs(Distance.Z) > RightAABBValue + RightOBBValue) return false;
        }

        // 6) OBB축(3축) 테스트 — R의 '열' + dot_t
        {
            float RightAABBValue, Dot_t;

            // Ux (j=0)
            Dot_t = Distance.X * R[0][0] + Distance.Y * R[1][0] + Distance.Z * R[2][0];
            RightAABBValue = AABBHalf.X * AbsR[0][0] + AABBHalf.Y * AbsR[1][0] + AABBHalf.Z * AbsR[2][0];
            if (Abs(Dot_t) > OBBExtents[0] + RightAABBValue) return false;
 
            // Uy (j=1)
            Dot_t = Distance.X * R[0][1] + Distance.Y * R[1][1] + Distance.Z * R[2][1];
            RightAABBValue = AABBHalf.X * AbsR[0][1] + AABBHalf.Y * AbsR[1][1] + AABBHalf.Z * AbsR[2][1];
            if (Abs(Dot_t) > OBBExtents[1] + RightAABBValue) return false;

            // Uz (j=2)
            Dot_t = Distance.X * R[0][2] + Distance.Y * R[1][2] + Distance.Z * R[2][2];
            RightAABBValue = AABBHalf.X * AbsR[0][2] + AABBHalf.Y * AbsR[1][2] + AABBHalf.Z * AbsR[2][2];
            if (Abs(Dot_t) > OBBExtents[2] + RightAABBValue) return false;
        }

        // 7) 교차축 9개: Ai × Uj  (i=월드축, j=OBB축)
        const float AABBExtents[3] = { AABBHalf.X, AABBHalf.Y, AABBHalf.Z };
        for (int i = 0; i < 3; ++i) {
            const int i1 = (i + 1) % 3, i2 = (i + 2) % 3;
            for (int j = 0; j < 3; ++j) {
                const int j1 = (j + 1) % 3, j2 = (j + 2) % 3;

                // LHS = | t[i2]*R[i1][j] - t[i1]*R[i2][j] |
                const float LHS = Abs(Comp(Distance, i2) * R[i1][j] - Comp(Distance, i1) * R[i2][j]);

                // rA = a[i1]*absR[i2][j] + a[i2]*absR[i1][j]
                const float RightAABBValue = AABBExtents[i1] * AbsR[i2][j] + AABBExtents[i2] * AbsR[i1][j];

                // rB = b[j1]*absR[i][j2] + b[j2]*absR[i][j1]
                const float RightOBBValue = OBBExtents[j1] * AbsR[i][j2] + OBBExtents[j2] * AbsR[i][j1];

                if (LHS > RightAABBValue + RightOBBValue) return false; // 분리 축 발견 → 불충돌
            }
        }

        // 8) 모든 축에서 분리 실패 → 충돌
        return true;
    }
}

FDecalPass::FDecalPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferCamera, ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS_Read, ID3D11BlendState* InBlendState)
    : FRenderPass(InPipeline, InConstantBufferCamera, nullptr),
    VS(InVS), PS(InPS), InputLayout(InLayout), DS_Read(InDS_Read), BlendState(InBlendState)
{
    ConstantBufferPrim = FRenderResourceFactory::CreateConstantBuffer<FModelConstants>();
    ConstantBufferDecal = FRenderResourceFactory::CreateConstantBuffer<FDecalConstants>();
}

void FDecalPass::Execute(FRenderingContext& Context)
{
	TIME_PROFILE(DecalPass)

    if (!(Context.ShowFlags & EEngineShowFlags::SF_Decal) || (Context.ViewMode == EViewModeIndex::VMI_SceneDepth)) return;
    
    // --- Set Pipeline State ---
    FPipelineInfo PipelineInfo = { InputLayout, VS, FRenderResourceFactory::GetRasterizerState({ ECullMode::Back, EFillMode::Solid }),
        DS_Read, PS, BlendState, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    Pipeline->UpdatePipeline(PipelineInfo);
    Pipeline->SetConstantBuffer(1, EShaderType::VS, ConstantBufferCamera);

    // --- Decals Stats ---
    uint32 RenderedDecal = 0;
    uint32 CollidedComps = 0;
    
    TArray<UPrimitiveComponent*>& DynamicPrimitives = GWorld->GetLevel()->GetDynamicPrimitives();
    
    // --- Render Decals ---
    for (UDecalComponent* Decal : Context.Decals)
    {
        if (!Decal || !Decal->IsVisible()) { continue; }

        const IBoundingVolume* DecalBV = Decal->GetBoundingBox();
        if (!DecalBV || DecalBV->GetType() != EBoundingVolumeType::OBB) { continue; }
        RenderedDecal++;
        
        const FOBB* DecalOBB = static_cast<const FOBB*>(DecalBV);

        Decal->UpdateProjectionMatrix();

        // --- Get Decal Transform ---
        FMatrix View = Decal->GetWorldTransformMatrixInverse();

        // --- Update Decal Constant Buffer ---
        FDecalConstants DecalConstants;
        DecalConstants.DecalWorld = Decal->GetWorldTransformMatrix();
        DecalConstants.DecalViewProjection = View * Decal->GetProjectionMatrix();
        DecalConstants.FadeProgress = Decal->GetFadeProgress();

        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferDecal, DecalConstants);
        Pipeline->SetConstantBuffer(2, EShaderType::PS, ConstantBufferDecal);

        // --- Bind Decal Texture ---
        if (UTexture* DecalTexture = Decal->GetTexture())
        {
            Pipeline->SetShaderResourceView(0, EShaderType::PS, DecalTexture->GetTextureSRV());
            Pipeline->SetSamplerState(0, EShaderType::PS, DecalTexture->GetTextureSampler());
        }

        if (UTexture* FadeTexture = Decal->GetFadeTexture())
        {
            Pipeline->SetShaderResourceView(1, EShaderType::PS, FadeTexture->GetTextureSRV());
            Pipeline->SetSamplerState(1, EShaderType::PS, FadeTexture->GetTextureSampler());
        }

        TArray<UPrimitiveComponent*> Primitives;

        // --- Enable Octree Optimization --- 
        ULevel* CurrentLevel = GWorld->GetLevel();

        Query(CurrentLevel->GetStaticOctree(), Decal, Primitives);
        Primitives.Append(DynamicPrimitives);

        // --- Disable Octree Optimization --- 
        // Primitives = Context.DefaultPrimitives;

        for (UPrimitiveComponent* Prim : Primitives)
        {
            if (!Prim || !Prim->IsVisible() || Prim->IsVisualizationComponent() || !Prim->bReceivesDecals) { continue; }

            const IBoundingVolume* PrimBV = Prim->GetBoundingBox();
        	if (!PrimBV || PrimBV->GetType() != EBoundingVolumeType::AABB) { continue; }
        
        	FVector WorldMin, WorldMax;
        	Prim->GetWorldAABB(WorldMin, WorldMax);
        	const FAABB WorldAABB(WorldMin, WorldMax);
        
        	if (!Intersects(*DecalOBB, WorldAABB))
        	{
        		continue;
        	}
            CollidedComps++;

            FModelConstants ModelConstants{ Prim->GetWorldTransformMatrix(), Prim->GetWorldTransformMatrixInverse().Transpose() };
            FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPrim, ModelConstants);
            Pipeline->SetConstantBuffer(0, EShaderType::VS, ConstantBufferPrim);

            Pipeline->SetVertexBuffer(Prim->GetVertexBuffer(), sizeof(FNormalVertex));
            if (Prim->GetIndexBuffer() && Prim->GetIndicesData())
            {
                Pipeline->SetIndexBuffer(Prim->GetIndexBuffer(), 0);
                Pipeline->DrawIndexed(Prim->GetNumIndices(), 0, 0);
            }
            else
            {
                Pipeline->Draw(Prim->GetNumVertices(), 0);
            }
        }
    }

    UStatOverlay::GetInstance().RecordDecalStats(RenderedDecal, CollidedComps);
}

void FDecalPass::Release()
{
    SafeRelease(ConstantBufferPrim);
    SafeRelease(ConstantBufferDecal);
}

void FDecalPass::Query(FOctree* InOctree, UDecalComponent* InDecal, TArray<UPrimitiveComponent*>& OutPrimitives)
{
    /** @todo Use polymorphism to gracefully handle collsion between decal and octree. For now, use explicit casting. */
    auto BoundingBox = static_cast<const FOBB*>(InDecal->GetBoundingBox());

    if (!BoundingBox->Intersects(InOctree->GetBoundingBox()))
    {
        return;
    }

    auto Primitives = InOctree->GetPrimitives();
    OutPrimitives.Append(Primitives);
    if (InOctree->IsLeafNode())
    {
        return;
    }

    
    for (auto Child : InOctree->GetChildren())
    {
        Query(Child, InDecal, OutPrimitives);
    }
}
