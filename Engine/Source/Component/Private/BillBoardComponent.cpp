#include "pch.h"
#include "Component/Public/BillBoardComponent.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Physics/Public/AABB.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/UI/Widget/Public/SpriteSelectionWidget.h"
#include "Texture/Public/Texture.h"
#include "Utility/Public/JsonSerializer.h"

#include <algorithm>

IMPLEMENT_CLASS(UBillBoardComponent, UPrimitiveComponent)

UBillBoardComponent::UBillBoardComponent()
{
    UAssetManager& ResourceManager = UAssetManager::GetInstance();

	Vertices = ResourceManager.GetVertexData(EPrimitiveType::Sprite);
	VertexBuffer = ResourceManager.GetVertexbuffer(EPrimitiveType::Sprite);
	NumVertices = ResourceManager.GetNumVertices(EPrimitiveType::Sprite);

	Indices = ResourceManager.GetIndexData(EPrimitiveType::Sprite);
	IndexBuffer = ResourceManager.GetIndexBuffer(EPrimitiveType::Sprite);
	NumIndices = ResourceManager.GetNumIndices(EPrimitiveType::Sprite);

	RenderState.CullMode = ECullMode::Back;
	RenderState.FillMode = EFillMode::Solid;
	BoundingBox = &ResourceManager.GetAABB(EPrimitiveType::Sprite);

    const TMap<FName, UTexture*>& TextureCache = UAssetManager::GetInstance().GetTextureCache();
    if (!TextureCache.IsEmpty())
    {
        Sprite = TextureCache.begin()->second;
    }

    bReceivesDecals = false;
}

UBillBoardComponent::~UBillBoardComponent() = default;

void UBillBoardComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        FString SpritePath;
        FJsonSerializer::ReadString(InOutHandle, "BillBoardSprite", SpritePath, "");
        if (!SpritePath.empty())
            SetSprite(UAssetManager::GetInstance().LoadTexture(FName(SpritePath)));

        FString ScreenSizeScaledString;
        FJsonSerializer::ReadString(InOutHandle, "BillBoardScreenSizeScaled", ScreenSizeScaledString, "false");
        bScreenSizeScaled = ScreenSizeScaledString == "true";

        FString ScreenSizeString;
        FJsonSerializer::ReadString(InOutHandle, "BillBoardScreenSize", ScreenSizeString, "0.1");
        ScreenSize = stof(ScreenSizeString);
    }
    // 저장
    else
    {
        if (Sprite)
        {
            InOutHandle["BillBoardSprite"] = Sprite->GetFilePath().ToBaseNameString();
        }
        else
        {
            InOutHandle["BillBoardSprite"] = "";
        }
        InOutHandle["BillBoardScreenSizeScaled"] = bScreenSizeScaled ? "true" : "false"; 
        InOutHandle["BillBoardScreenSize"] = to_string(ScreenSize); 
    }
}

void UBillBoardComponent::FaceCamera(const FVector& CameraForward)
{
    FVector Forward = CameraForward;
    FVector Right = FVector::UpVector().Cross(Forward); Right.Normalize();
    FVector Up = Forward.Cross(Right); Up.Normalize();
    
    // Construct the rotation matrix from the basis vectors
    FMatrix RotationMatrix = FMatrix(Forward, Right, Up);
    
    // Convert the rotation matrix to a quaternion and set the relative rotation
    SetWorldRotation(FQuaternion::FromRotationMatrix(RotationMatrix));
}

UTexture* UBillBoardComponent::GetSprite() const
{
    return Sprite;
}

void UBillBoardComponent::SetSprite(UTexture* InSprite)
{
    Sprite = InSprite;
}

void UBillBoardComponent::SetSpriteTint(const FVector4& InTint)
{
    FVector4 ClampedTint = InTint;
    ClampedTint.X = std::clamp(ClampedTint.X, 0.0f, 1.0f);
    ClampedTint.Y = std::clamp(ClampedTint.Y, 0.0f, 1.0f);
    ClampedTint.Z = std::clamp(ClampedTint.Z, 0.0f, 1.0f);
    ClampedTint.W = std::clamp(ClampedTint.W, 0.0f, 1.0f);
    SpriteTint = ClampedTint;
}

void UBillBoardComponent::SetSpriteTint(const FVector& InTint, float Alpha)
{
    SetSpriteTint(FVector4(InTint.X, InTint.Y, InTint.Z, Alpha));
}

UClass* UBillBoardComponent::GetSpecificWidgetClass() const
{
    return USpriteSelectionWidget::StaticClass();
}

const FRenderState& UBillBoardComponent::GetClassDefaultRenderState()
{
    static FRenderState DefaultRenderState { ECullMode::Back, EFillMode::Solid };
    return DefaultRenderState;
}

UObject* UBillBoardComponent::Duplicate()
{
    UBillBoardComponent* BillboardComponent = Cast<UBillBoardComponent>(Super::Duplicate());

    // Sprite 텍스처 복사
    BillboardComponent->Sprite = Sprite;
    BillboardComponent->SpriteTint = SpriteTint;

    // Screen size 설정 복사
    BillboardComponent->bScreenSizeScaled = bScreenSizeScaled;
    BillboardComponent->ScreenSize = ScreenSize;

    return BillboardComponent;
}
