#include "pch.h"
#include "Component/Public/SphereComponent.h"
#include "Physics/Public/BoundingSphere.h"
#include "Utility/Public/JsonSerializer.h"
#include "Render/UI/Widget/Public/SphereComponentWidget.h"
#include "Editor/Public/BatchLines.h"
#include <algorithm>

IMPLEMENT_CLASS(USphereComponent, UShapeComponent)

USphereComponent::USphereComponent()
{
	SphereRadius = 0.5f;
	UpdateBoundingVolume();
}

void USphereComponent::SetSphereRadius(float InRadius)
{
	if (SphereRadius != InRadius)
	{
		SphereRadius = std::max(0.0f, InRadius);
		UpdateBoundingVolume();
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

void USphereComponent::UpdateBoundingVolume()
{
	// Clean up old bounding volume if we own it
	if (bOwnsBoundingBox && BoundingBox)
	{
		delete BoundingBox;
		BoundingBox = nullptr;
	}

	bOwnsBoundingBox = true;
	FBoundingSphere* Sphere = new FBoundingSphere(FVector(0.0f, 0.0f, 0.0f), SphereRadius);
	BoundingBox = Sphere;
	bIsAABBCacheDirty = true;
}

void USphereComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FString SphereRadiusString;
		FJsonSerializer::ReadString(InOutHandle, "SphereRadius", SphereRadiusString, "0.5");
		SphereRadius = std::stof(SphereRadiusString);
		UpdateBoundingVolume();
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
