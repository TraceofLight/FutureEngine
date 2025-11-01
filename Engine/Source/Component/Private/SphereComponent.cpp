#include "pch.h"
#include "Component/Public/SphereComponent.h"
#include "Physics/Public/BoundingSphere.h"
#include "Physics/Public/Bounds.h"
#include "Utility/Public/JsonSerializer.h"
#include "Render/UI/Widget/Public/SphereComponentWidget.h"
#include "Editor/Public/BatchLines.h"
#include <algorithm>

IMPLEMENT_CLASS(USphereComponent, UShapeComponent)

USphereComponent::USphereComponent()
{
	SphereRadius = 0.5f;
	bOwnsBoundingBox = true;
	BoundingBox = new FBoundingSphere(FVector(0.0f, 0.0f, 0.0f), SphereRadius);
}

void USphereComponent::SetSphereRadius(float InRadius)
{
	if (SphereRadius != InRadius)
	{
		SphereRadius = std::max(0.0f, InRadius);

		// Update BoundingBox radius (local space)
		if (BoundingBox && BoundingBox->GetType() == EBoundingVolumeType::Sphere)
		{
			FBoundingSphere* Sphere = static_cast<FBoundingSphere*>(BoundingBox);
			Sphere->Radius = SphereRadius;
		}

		MarkAsDirty();
	}
}

void USphereComponent::InitSphereRadius(float InRadius)
{
	SphereRadius = std::max(0.0f, InRadius);
}

float USphereComponent::GetScaledSphereRadius() const
{
	FVector Scale = GetWorldScale3D();
	float MaxScale = std::max(std::max(Scale.X, Scale.Y), Scale.Z);
	return SphereRadius * MaxScale;
}

// === Collision System (SOLID) ===

FBounds USphereComponent::CalcBounds() const
{
	FVector WorldCenter = GetWorldLocation();
	float ScaledRadius = GetScaledSphereRadius();
	return FBounds::FromSphere(WorldCenter, ScaledRadius);
}

UObject* USphereComponent::Duplicate()
{
	USphereComponent* NewSphere = Cast<USphereComponent>(Super::Duplicate());
	NewSphere->SphereRadius = SphereRadius;

	// Update BoundingBox radius (생성자에서 이미 생성된 BoundingBox의 값 업데이트)
	if (NewSphere->BoundingBox && NewSphere->BoundingBox->GetType() == EBoundingVolumeType::Sphere)
	{
		FBoundingSphere* Sphere = static_cast<FBoundingSphere*>(NewSphere->BoundingBox);
		Sphere->Radius = SphereRadius;
	}

	return NewSphere;
}

void USphereComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FString SphereRadiusString;
		FJsonSerializer::ReadString(InOutHandle, "SphereRadius", SphereRadiusString, "0.5");
		SphereRadius = std::stof(SphereRadiusString);

		// Update BoundingBox radius (local space)
		if (BoundingBox && BoundingBox->GetType() == EBoundingVolumeType::Sphere)
		{
			FBoundingSphere* Sphere = static_cast<FBoundingSphere*>(BoundingBox);
			Sphere->Radius = SphereRadius;
		}
	}
	else
	{
		InOutHandle["SphereRadius"] = std::to_string(SphereRadius);
	}
}

UClass* USphereComponent::GetSpecificWidgetClass() const
{
	return USphereComponentWidget::StaticClass();
}

void USphereComponent::RenderDebugShape(UBatchLines& BatchLines)
{
	FVector WorldLocation = GetWorldLocation();
	float ScaledRadius = GetScaledSphereRadius();
	FBoundingSphere WorldSphere(WorldLocation, ScaledRadius);
	BatchLines.UpdateBoundingBoxVertices(&WorldSphere);
}
