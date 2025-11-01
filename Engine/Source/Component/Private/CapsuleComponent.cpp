#include "pch.h"
#include "Component/Public/CapsuleComponent.h"
#include "Physics/Public/Capsule.h"
#include "Physics/Public/Bounds.h"
#include "Utility/Public/JsonSerializer.h"
#include "Render/UI/Widget/Public/CapsuleComponentWidget.h"
#include "Editor/Public/BatchLines.h"
#include <algorithm>

IMPLEMENT_CLASS(UCapsuleComponent, UShapeComponent)

UCapsuleComponent::UCapsuleComponent()
{
	CapsuleRadius = 0.5f;
	CapsuleHalfHeight = 1.0f;
	bOwnsBoundingBox = true;
	FCapsule* Capsule = new FCapsule();
	Capsule->Center = FVector(0.0f, 0.0f, 0.0f); // Local space
	Capsule->Rotation = FQuaternion::Identity();
	Capsule->Radius = CapsuleRadius;
	Capsule->HalfHeight = CapsuleHalfHeight;
	BoundingBox = Capsule;
}

void UCapsuleComponent::SetCapsuleRadius(float InRadius, bool bUpdateOverlaps)
{
	if (CapsuleRadius != InRadius)
	{
		CapsuleRadius = std::max(0.0f, InRadius);

		// Update BoundingBox radius (local space)
		if (BoundingBox && BoundingBox->GetType() == EBoundingVolumeType::Capsule)
		{
			FCapsule* Capsule = static_cast<FCapsule*>(BoundingBox);
			Capsule->Radius = CapsuleRadius;
		}

		MarkAsDirty();
	}
}

void UCapsuleComponent::SetCapsuleHalfHeight(float InHalfHeight, bool bUpdateOverlaps)
{
	if (CapsuleHalfHeight != InHalfHeight)
	{
		// HalfHeight must be at least as large as radius
		CapsuleHalfHeight = std::max(CapsuleRadius, InHalfHeight);

		// Update BoundingBox half height (local space)
		if (BoundingBox && BoundingBox->GetType() == EBoundingVolumeType::Capsule)
		{
			FCapsule* Capsule = static_cast<FCapsule*>(BoundingBox);
			Capsule->HalfHeight = CapsuleHalfHeight;
		}

		MarkAsDirty();
	}
}

void UCapsuleComponent::SetCapsuleSize(float InRadius, float InHalfHeight, bool bUpdateOverlaps)
{
	CapsuleRadius = std::max(0.0f, InRadius);
	CapsuleHalfHeight = std::max(CapsuleRadius, InHalfHeight);

	// Update BoundingBox size (local space)
	if (BoundingBox && BoundingBox->GetType() == EBoundingVolumeType::Capsule)
	{
		FCapsule* Capsule = static_cast<FCapsule*>(BoundingBox);
		Capsule->Radius = CapsuleRadius;
		Capsule->HalfHeight = CapsuleHalfHeight;
	}

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

// === Collision System (SOLID) ===

FBounds UCapsuleComponent::CalcBounds() const
{
	FVector WorldCenter = GetWorldLocation();
	FQuaternion WorldRotation = GetWorldRotationAsQuaternion();
	float ScaledRadius = GetScaledCapsuleRadius();
	float ScaledHalfHeight = GetScaledCapsuleHalfHeight();

	// Create temporary capsule and convert to AABB
	FCapsule TempCapsule(WorldCenter, WorldRotation, ScaledRadius, ScaledHalfHeight);
	FAABB TempAABB = TempCapsule.ToAABB();

	return FBounds(TempAABB.Min, TempAABB.Max);
}

UObject* UCapsuleComponent::Duplicate()
{
	UCapsuleComponent* NewCapsule = Cast<UCapsuleComponent>(Super::Duplicate());
	NewCapsule->CapsuleRadius = CapsuleRadius;
	NewCapsule->CapsuleHalfHeight = CapsuleHalfHeight;

	// Update BoundingBox size (생성자에서 이미 생성된 BoundingBox의 값 업데이트)
	if (NewCapsule->BoundingBox && NewCapsule->BoundingBox->GetType() == EBoundingVolumeType::Capsule)
	{
		FCapsule* Capsule = static_cast<FCapsule*>(NewCapsule->BoundingBox);
		Capsule->Radius = CapsuleRadius;
		Capsule->HalfHeight = CapsuleHalfHeight;
	}

	return NewCapsule;
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

		// Update BoundingBox size (local space)
		if (BoundingBox && BoundingBox->GetType() == EBoundingVolumeType::Capsule)
		{
			FCapsule* Capsule = static_cast<FCapsule*>(BoundingBox);
			Capsule->Radius = CapsuleRadius;
			Capsule->HalfHeight = CapsuleHalfHeight;
		}
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
