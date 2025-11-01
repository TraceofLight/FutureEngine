#include "pch.h"
#include "Component/Public/DecalSpotLightComponent.h"
#include "Texture/Public/Texture.h"
#include "Physics/Public/OBB.h"
#include "Manager/Asset/Public/AssetManager.h"

IMPLEMENT_CLASS(UDecalSpotLightComponent, UDecalComponent)

UDecalSpotLightComponent::UDecalSpotLightComponent()
{
	bOwnsBoundingBox = true;
	SafeDelete(BoundingBox);
	BoundingBox = new FOBB(FVector(0.f, 0.f, 0.f), FVector(0.5f, 0.5f, 0.5f), FMatrix::Identity());

	const TMap<FName, UTexture*>& TextureCache = UAssetManager::GetInstance().GetTextureCache();
	if (!TextureCache.IsEmpty())
	{
		SetTexture(TextureCache.begin()->second);
	}
	SetFadeTexture(UAssetManager::GetInstance().LoadTexture(FName("Data/Texture/spotlight2.png")));

	SetPerspective(true);

	SpotLightBoundingBox = new FSpotLightOBB(FVector(0.f, 0.f, 0.f), FVector(0.5f, 0.5f, 0.5f), FMatrix::Identity());
}

UDecalSpotLightComponent::~UDecalSpotLightComponent()
{
	SafeDelete(SpotLightBoundingBox);
}

void UDecalSpotLightComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

void UDecalSpotLightComponent::UpdateProjectionMatrix()
{
	//FSpotLightOBB* Fobb = static_cast<FSpotLightOBB*>(BoundingBox);
	FOBB* OBB = static_cast<FOBB*>(BoundingBox);
	if (!OBB) { return; }

	float W = OBB->Extents.X;
	float H = OBB->Extents.Z;

	float FoV = 2 * atan(H / W);
	float AspectRatio = OBB->Extents.Y / OBB->Extents.Z;
	float NearClip = 0.1f;
	float FarClip = OBB->Extents.X * 2;

	float F = 2 * W / H;

	FMatrix MoveCamera = FMatrix::Identity();
	MoveCamera.Data[3][0] = 0.5f;

	// Manually calculate the perspective projection matrix

	// Initialize with a clear state
	ProjectionMatrix = FMatrix::Identity();

	// | f/aspect   0        0         0 |
	// |    0       f        0         0 |
	// |    0       0   zf/(zf-zn)     1 |
	// |    0       0  -zn*zf/(zf-zn)  0 |
	ProjectionMatrix.Data[1][1] = F / AspectRatio;
	ProjectionMatrix.Data[2][2] = F;
	ProjectionMatrix.Data[0][0] = FarClip / (FarClip - NearClip);
	ProjectionMatrix.Data[0][3] = 1.0f;
	ProjectionMatrix.Data[3][0] = (-NearClip * FarClip) / (FarClip - NearClip);
	ProjectionMatrix.Data[3][3] = 0.0f;

	ProjectionMatrix = MoveCamera * ProjectionMatrix;
	
}

UObject* UDecalSpotLightComponent::Duplicate()
{
	UDecalSpotLightComponent* DuplicatedComponent = Cast<UDecalSpotLightComponent>(Super::Duplicate());

	FOBB* OriginalOBB = static_cast<FOBB*>(BoundingBox);
	FOBB* DuplicatedOBB = static_cast<FOBB*>(DuplicatedComponent->BoundingBox);
	if (OriginalOBB && DuplicatedOBB)
	{
		DuplicatedOBB->Center = OriginalOBB->Center;
		DuplicatedOBB->Extents = OriginalOBB->Extents;
		DuplicatedOBB->ScaleRotation = OriginalOBB->ScaleRotation;
	}
	return DuplicatedComponent;
}

const IBoundingVolume* UDecalSpotLightComponent::GetBoundingBox()
{
	SpotLightBoundingBox->Update(GetWorldTransformMatrix());
	return Super::GetBoundingBox();
}
