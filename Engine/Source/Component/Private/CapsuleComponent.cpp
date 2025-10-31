#include "pch.h"
#include "Component/Public/CapsuleComponent.h"
#include "Physics/Public/Capsule.h"
#include "Utility/Public/JsonSerializer.h"
#include "Render/UI/Widget/Public/CapsuleComponentWidget.h"
#include "Editor/Public/BatchLines.h"
#include <algorithm>

IMPLEMENT_CLASS(UCapsuleComponent, UShapeComponent)

UCapsuleComponent::UCapsuleComponent()
{
	CapsuleRadius = 0.5f;
	CapsuleHalfHeight = 1.0f;
	UpdateBoundingVolume();
}

void UCapsuleComponent::SetCapsuleRadius(float InRadius, bool bUpdateOverlaps)
{
	if (CapsuleRadius != InRadius)
	{
		CapsuleRadius = std::max(0.0f, InRadius);
		UpdateBoundingVolume();
		MarkAsDirty();
	}
}

void UCapsuleComponent::SetCapsuleHalfHeight(float InHalfHeight, bool bUpdateOverlaps)
{
	if (CapsuleHalfHeight != InHalfHeight)
	{
		// HalfHeight must be at least as large as radius
		CapsuleHalfHeight = std::max(CapsuleRadius, InHalfHeight);
		UpdateBoundingVolume();
		MarkAsDirty();
	}
}

void UCapsuleComponent::SetCapsuleSize(float InRadius, float InHalfHeight, bool bUpdateOverlaps)
{
	CapsuleRadius = std::max(0.0f, InRadius);
	CapsuleHalfHeight = std::max(CapsuleRadius, InHalfHeight);
	UpdateBoundingVolume();
	MarkAsDirty();
}

void UCapsuleComponent::InitCapsuleSize(float InRadius, float InHalfHeight)
{
	CapsuleRadius = std::max(0.0f, InRadius);
	CapsuleHalfHeight = std::max(CapsuleRadius, InHalfHeight);
}

float UCapsuleComponent::GetScaledCapsuleRadius() const
{
	FVector Scale = GetWorldScale3D();
	// Use XY scale for radius (ignore Z)
	float MaxRadialScale = std::max(Scale.X, Scale.Y);
	return CapsuleRadius * MaxRadialScale;
}

float UCapsuleComponent::GetScaledCapsuleHalfHeight() const
{
	FVector Scale = GetWorldScale3D();
	return CapsuleHalfHeight * Scale.Z;
}

void UCapsuleComponent::UpdateBoundingVolume()
{
	// Clean up old bounding volume if we own it
	if (bOwnsBoundingBox && BoundingBox)
	{
		delete BoundingBox;
		BoundingBox = nullptr;
	}

	bOwnsBoundingBox = true;
	FCapsule* Capsule = new FCapsule();
	Capsule->Center = FVector(0.0f, 0.0f, 0.0f); // Local space
	Capsule->Rotation = FQuaternion::Identity();
	Capsule->Radius = CapsuleRadius;
	Capsule->HalfHeight = CapsuleHalfHeight;
	BoundingBox = Capsule;
	bIsAABBCacheDirty = true;
}

void UCapsuleComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FString CapsuleRadiusString;
		FJsonSerializer::ReadString(InOutHandle, "CapsuleRadius", CapsuleRadiusString, "0.5");
		CapsuleRadius = std::stof(CapsuleRadiusString);

		FString CapsuleHalfHeightString;
		FJsonSerializer::ReadString(InOutHandle, "CapsuleHalfHeight", CapsuleHalfHeightString, "1.0");
		CapsuleHalfHeight = std::stof(CapsuleHalfHeightString);

		UpdateBoundingVolume();
	}
	else
	{
		InOutHandle["CapsuleRadius"] = std::to_string(CapsuleRadius);
		InOutHandle["CapsuleHalfHeight"] = std::to_string(CapsuleHalfHeight);
	}
}

UClass* UCapsuleComponent::GetSpecificWidgetClass() const
{
	return UCapsuleComponentWidget::StaticClass();
}

void UCapsuleComponent::RenderDebugShape(UBatchLines& BatchLines)
{
	FVector WorldLocation = GetWorldLocation();
	FQuaternion WorldRotation = GetWorldRotationAsQuaternion();
	float ScaledRadius = GetScaledCapsuleRadius();
	float ScaledHalfHeight = GetScaledCapsuleHalfHeight();

	FCapsule WorldCapsule;
	WorldCapsule.Center = WorldLocation;
	WorldCapsule.Rotation = WorldRotation;
	WorldCapsule.Radius = ScaledRadius;
	WorldCapsule.HalfHeight = ScaledHalfHeight;

	BatchLines.UpdateBoundingBoxVertices(&WorldCapsule);
}
