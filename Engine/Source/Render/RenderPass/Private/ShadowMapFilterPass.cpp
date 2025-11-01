#include "pch.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/RenderPass/Public/ShadowMapPass.h"
#include "Render/RenderPass/Public/ShadowMapFilterPass.h"
#include "Component/Public/DirectionalLightComponent.h"
#include "Component/Public/SpotLightComponent.h"
#include "Component/Public/PointLightComponent.h"

FShadowMapFilterPass::FShadowMapFilterPass(FShadowMapPass* InShadowMapPass, UPipeline* InPipeline)
    : FRenderPass(InPipeline), ShadowMapPass(InShadowMapPass)
{
	TextureFilterMap[EShadowModeIndex::SMI_VSM_BOX] = std::make_unique<FTextureFilter>("Asset/Shader/BoxTextureFilter.hlsl");
	TextureFilterMap[EShadowModeIndex::SMI_SAVSM] = std::make_unique<FTextureFilter>("Asset/Shader/SummedAreaTextureFilter.hlsl");
	TextureFilterMap[EShadowModeIndex::SMI_VSM_GAUSSIAN] = std::make_unique<FTextureFilter>("Asset/Shader/GaussianTextureFilter.hlsl");
}

FShadowMapFilterPass::~FShadowMapFilterPass()
{
	// TODO: 소멸자에서 가상 함수 호출하지 말아야 함, 다른 렌더패스 확인 필요
    // Release();
}

void FShadowMapFilterPass::Execute(FRenderingContext& Context)
{
	// --- 1. Directional Lights ---
	for (uint32 i = 0; i < Context.DirectionalLights.Num(); ++i)
	{
		auto DirLight = Context.DirectionalLights[i];
		if (DirLight->GetCastShadows() && DirLight->GetLightEnabled())
		{
			for (int32 j = 0; j < UCascadeManager::GetInstance().GetSplitNum(); ++j)
			{
				FShadowMapResource* ShadowMap = ShadowMapPass->GetShadowAtlas();
				FShadowAtlasTilePos AtlasTilePos = ShadowMapPass->GetDirectionalAtlasTilePos(j);
				FilterShadowAtlasMap(
					DirLight,
					ShadowMap,
					AtlasTilePos.UV[0] * TEXTURE_WIDTH,
					AtlasTilePos.UV[1] * TEXTURE_HEIGHT,
					static_cast<uint32>(DirLight->GetShadowResolutionScale()),
					static_cast<uint32>(DirLight->GetShadowResolutionScale())
				);
			}
		}
	}

	// --- 2. SpotLights ---
	for (uint32 i = 0; i < Context.SpotLights.Num(); ++i)
	{
		auto SpotLight = Context.SpotLights[i];
		if (SpotLight->GetCastShadows() && SpotLight->GetLightEnabled())
		{
			FShadowMapResource* ShadowMap = ShadowMapPass->GetShadowAtlas();
			FShadowAtlasTilePos AtlasTilePos = ShadowMapPass->GetSpotAtlasTilePos(i);
			FilterShadowAtlasMap(
				SpotLight,
				ShadowMap,
				AtlasTilePos.UV[0] * TEXTURE_WIDTH,
				AtlasTilePos.UV[1] * TEXTURE_HEIGHT,
				static_cast<uint32>(SpotLight->GetShadowResolutionScale()),
				static_cast<uint32>(SpotLight->GetShadowResolutionScale())
			);
		}
	}

	// --- 3. Point Lights ---
	for (uint32 i = 0; i< Context.PointLights.Num(); ++i)
	{
		auto PointLight = Context.PointLights[i];
		if (PointLight->GetCastShadows() && PointLight->GetLightEnabled())
		{
			FShadowMapResource* ShadowMap = ShadowMapPass->GetShadowAtlas();
			FShadowAtlasPointLightTilePos AtlasTilePos = ShadowMapPass->GetPointAtlasTilePos(i);
			for (int j = 0; j < 6; ++j)
			{
				FilterShadowAtlasMap(
					PointLight,
					ShadowMap,
					AtlasTilePos.UV[j][0] * TEXTURE_WIDTH,
					AtlasTilePos.UV[j][1] * TEXTURE_HEIGHT,
					TEXTURE_WIDTH,
					TEXTURE_HEIGHT
				);	
			}
		}
	}
}

void FShadowMapFilterPass::Release()
{
	TextureFilterMap.clear();	
}

void FShadowMapFilterPass::FilterShadowMap(const ULightComponent* LightComponent, const FShadowMapResource* ShadowMap) 
{
	if (!ShadowMap || !ShadowMap->IsValid())
	{
		return;
	}

	switch (LightComponent->GetShadowModeIndex())
	{
	case EShadowModeIndex::SMI_VSM_BOX:
		{
			uint32 NumGroupsX = (ShadowMap->Resolution + THREAD_BLOCK_SIZE_X - 1) / THREAD_BLOCK_SIZE_X;
			uint32 NumGroupsY = (ShadowMap->Resolution + THREAD_BLOCK_SIZE_Y - 1) / THREAD_BLOCK_SIZE_Y;
			uint32 NumGroupsZ = 1;
			TextureFilterMap[EShadowModeIndex::SMI_VSM_BOX]->FilterTexture(
				ShadowMap->VarianceShadowSRV.Get(),
				ShadowMap->VarianceShadowUAV.Get(),
				NumGroupsX, NumGroupsY, NumGroupsZ,
				0, 0, TEXTURE_WIDTH, TEXTURE_HEIGHT,
				1.0f - LightComponent->GetShadowSharpen()
			);
			break;
		}
	case EShadowModeIndex::SMI_VSM_GAUSSIAN:
		{
			uint32 NumGroupsX = (ShadowMap->Resolution + THREAD_BLOCK_SIZE_X - 1) / THREAD_BLOCK_SIZE_X;
			uint32 NumGroupsY = (ShadowMap->Resolution + THREAD_BLOCK_SIZE_Y - 1) / THREAD_BLOCK_SIZE_Y;
			uint32 NumGroupsZ = 1;
			TextureFilterMap[EShadowModeIndex::SMI_VSM_GAUSSIAN]->FilterTexture(
				ShadowMap->VarianceShadowSRV.Get(),
				ShadowMap->VarianceShadowUAV.Get(),
				NumGroupsX, NumGroupsY, NumGroupsZ,
				0, 0, TEXTURE_WIDTH, TEXTURE_HEIGHT,
				1.0f - LightComponent->GetShadowSharpen()
			);
			break;
		}
	case EShadowModeIndex::SMI_SAVSM:
		{
			uint32 NumGroups = ShadowMap->Resolution;
			TextureFilterMap[EShadowModeIndex::SMI_SAVSM]->FilterTexture(
				ShadowMap->VarianceShadowSRV.Get(),
				ShadowMap->VarianceShadowUAV.Get(),
				NumGroups,
				0, 0, TEXTURE_WIDTH, TEXTURE_HEIGHT
			);
			break;
		}
	default:
		// 필터링 없음
		break;
	}
}

void FShadowMapFilterPass::FilterShadowAtlasMap(const ULightComponent* LightComponent,
	const FShadowMapResource* ShadowMap, uint32 RegionStartX, uint32 RegionStartY, uint32 RegionWidth, uint32 RegionHeight)
{
	if (!ShadowMap || !ShadowMap->IsValid())
	{
		return;
	}

	switch (LightComponent->GetShadowModeIndex())
	{
	case EShadowModeIndex::SMI_VSM_BOX:
		{
			uint32 NumGroupsX = (RegionWidth + THREAD_BLOCK_SIZE_X - 1) / THREAD_BLOCK_SIZE_X;
			uint32 NumGroupsY = (RegionHeight + THREAD_BLOCK_SIZE_Y - 1) / THREAD_BLOCK_SIZE_Y;
			uint32 NumGroupsZ = 1;
			TextureFilterMap[EShadowModeIndex::SMI_VSM_BOX]->FilterTexture(
				ShadowMap->VarianceShadowSRV.Get(),
				ShadowMap->VarianceShadowUAV.Get(),
				NumGroupsX, NumGroupsY, NumGroupsZ,
				RegionStartX, RegionStartY, RegionWidth, RegionHeight,
				1.0f - LightComponent->GetShadowSharpen()
			);
			break;
		}
	case EShadowModeIndex::SMI_VSM_GAUSSIAN:
		{
			uint32 NumGroupsX = (RegionWidth + THREAD_BLOCK_SIZE_X - 1) / THREAD_BLOCK_SIZE_X;
			uint32 NumGroupsY = (RegionHeight + THREAD_BLOCK_SIZE_Y - 1) / THREAD_BLOCK_SIZE_Y;
			uint32 NumGroupsZ = 1;
			TextureFilterMap[EShadowModeIndex::SMI_VSM_GAUSSIAN]->FilterTexture(
				ShadowMap->VarianceShadowSRV.Get(),
				ShadowMap->VarianceShadowUAV.Get(),
				NumGroupsX, NumGroupsY, NumGroupsZ,
				RegionStartX, RegionStartY, RegionWidth, RegionHeight,
				1.0f - LightComponent->GetShadowSharpen()
			);
			break;
		}
	case EShadowModeIndex::SMI_SAVSM:
		{
			uint32 NumGroups = RegionWidth;
			TextureFilterMap[EShadowModeIndex::SMI_SAVSM]->FilterTexture(
				ShadowMap->VarianceShadowSRV.Get(),
				ShadowMap->VarianceShadowUAV.Get(),
				NumGroups,
				RegionStartX, RegionStartY, RegionWidth, RegionHeight
			);
			break;
		}
	default:
		// 필터링 없음
		break;
	}	
}