#include "pch.h"
#include "Component/Public/EditorIconComponent.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Physics/Public/AABB.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/UI/Widget/Public/SpriteSelectionWidget.h"
#include "Texture/Public/Texture.h"
#include "Utility/Public/JsonSerializer.h"

#include <algorithm>

IMPLEMENT_CLASS(UEditorIconComponent, UPrimitiveComponent)

UEditorIconComponent::UEditorIconComponent()
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

	// 에디터 전용 및 Visualization 컴포넌트로 설정
	SetIsEditorOnly(true);
	SetIsVisualizationComponent(true);
}

UEditorIconComponent::~UEditorIconComponent() = default;

void UEditorIconComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FString SpritePath;
		FJsonSerializer::ReadString(InOutHandle, "EditorIconSprite", SpritePath, "");
		if (!SpritePath.empty())
		{
			SetSprite(UAssetManager::GetInstance().LoadTexture(FName(SpritePath)));
		}

		FString ScreenSizeScaledString;
		FJsonSerializer::ReadString(InOutHandle, "EditorIconScreenSizeScaled", ScreenSizeScaledString, "false");
		bScreenSizeScaled = ScreenSizeScaledString == "true";

		FString ScreenSizeString;
		FJsonSerializer::ReadString(InOutHandle, "EditorIconScreenSize", ScreenSizeString, "0.1");
		ScreenSize = stof(ScreenSizeString);
	}
	else
	{
		if (Sprite)
		{
			InOutHandle["EditorIconSprite"] = Sprite->GetFilePath().ToBaseNameString();
		}
		else
		{
			InOutHandle["EditorIconSprite"] = "";
		}
		InOutHandle["EditorIconScreenSizeScaled"] = bScreenSizeScaled ? "true" : "false";
		InOutHandle["EditorIconScreenSize"] = to_string(ScreenSize);
	}
}

void UEditorIconComponent::FaceCamera(const FVector& CameraForward)
{
	FVector Forward = CameraForward;
	FVector Right = FVector::UpVector().Cross(Forward);
	Right.Normalize();
	FVector Up = Forward.Cross(Right);
	Up.Normalize();

	FMatrix RotationMatrix = FMatrix(Forward, Right, Up);

	SetWorldRotation(FQuaternion::FromRotationMatrix(RotationMatrix));
}

UTexture* UEditorIconComponent::GetSprite() const
{
	return Sprite;
}

void UEditorIconComponent::SetSprite(UTexture* InSprite)
{
	Sprite = InSprite;
}

void UEditorIconComponent::SetSpriteTint(const FVector4& InTint)
{
	FVector4 ClampedTint = InTint;
	ClampedTint.X = std::clamp(ClampedTint.X, 0.0f, 1.0f);
	ClampedTint.Y = std::clamp(ClampedTint.Y, 0.0f, 1.0f);
	ClampedTint.Z = std::clamp(ClampedTint.Z, 0.0f, 1.0f);
	ClampedTint.W = std::clamp(ClampedTint.W, 0.0f, 1.0f);
	SpriteTint = ClampedTint;
}

void UEditorIconComponent::SetSpriteTint(const FVector& InTint, float Alpha)
{
	SetSpriteTint(FVector4(InTint.X, InTint.Y, InTint.Z, Alpha));
}

UClass* UEditorIconComponent::GetSpecificWidgetClass() const
{
	return USpriteSelectionWidget::StaticClass();
}

const FRenderState& UEditorIconComponent::GetClassDefaultRenderState()
{
	static FRenderState DefaultRenderState{ ECullMode::Back, EFillMode::Solid };
	return DefaultRenderState;
}

UObject* UEditorIconComponent::Duplicate()
{
	UEditorIconComponent* IconComponent = Cast<UEditorIconComponent>(Super::Duplicate());

	// Sprite 텍스처 복사
	IconComponent->Sprite = Sprite;
	IconComponent->SpriteTint = SpriteTint;

	// Screen size 설정 복사
	IconComponent->bScreenSizeScaled = bScreenSizeScaled;
	IconComponent->ScreenSize = ScreenSize;

	return IconComponent;
}
