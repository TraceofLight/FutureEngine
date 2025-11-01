#include "pch.h"
#include "Component/Public/BoxComponent.h"
#include "Physics/Public/OBB.h"
#include "Physics/Public/Bounds.h"
#include "Utility/Public/JsonSerializer.h"
#include "Render/UI/Widget/Public/BoxComponentWidget.h"
#include "Editor/Public/BatchLines.h"
#include <algorithm>

IMPLEMENT_CLASS(UBoxComponent, UShapeComponent)

UBoxComponent::UBoxComponent()
{
	BoxExtent = FVector(0.5f, 0.5f, 0.5f);
	bOwnsBoundingBox = true;
	FOBB* Box = new FOBB();
	Box->Center = FVector(0.0f, 0.0f, 0.0f); // Local space
	Box->Extents = BoxExtent;
	Box->ScaleRotation = FMatrix::Identity();
	BoundingBox = Box;
}

void UBoxComponent::SetBoxExtent(const FVector& InExtent)
{
	FVector ClampedExtent(
		std::max(0.0f, InExtent.X),
		std::max(0.0f, InExtent.Y),
		std::max(0.0f, InExtent.Z)
	);

	if (BoxExtent != ClampedExtent)
	{
		BoxExtent = ClampedExtent;

		// Update BoundingBox extents (local space)
		if (BoundingBox && BoundingBox->GetType() == EBoundingVolumeType::OBB)
		{
			FOBB* Box = static_cast<FOBB*>(BoundingBox);
			Box->Extents = BoxExtent;
		}

		MarkAsDirty();
	}
}

void UBoxComponent::InitBoxExtent(const FVector& InExtent)
{
	BoxExtent = FVector(
		std::max(0.0f, InExtent.X),
		std::max(0.0f, InExtent.Y),
		std::max(0.0f, InExtent.Z)
	);
}

FVector UBoxComponent::GetScaledBoxExtent() const
{
	FVector Scale = GetWorldScale3D();
	return BoxExtent * Scale;
}

// === Collision System (SOLID) ===

FBounds UBoxComponent::CalcBounds() const
{
	FVector WorldCenter = GetWorldLocation();
	FVector ScaledExtent = GetScaledBoxExtent();

	// For rotated boxes, we need to calculate the AABB of the OBB
	FQuaternion WorldRotation = GetWorldRotationAsQuaternion();
	FMatrix RotationMatrix = WorldRotation.ToRotationMatrix();

	// Create OBB and convert to AABB
	FOBB TempOBB(WorldCenter, ScaledExtent, RotationMatrix);
	FAABB TempAABB = TempOBB.ToWorldAABB();

	return FBounds(TempAABB.Min, TempAABB.Max);
}

void UBoxComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FVector ReadBoxExtent;
		FJsonSerializer::ReadVector(InOutHandle, "BoxExtent", ReadBoxExtent, FVector(0.5f, 0.5f, 0.5f));
		BoxExtent = ReadBoxExtent;

		// Update BoundingBox extents (local space)
		if (BoundingBox && BoundingBox->GetType() == EBoundingVolumeType::OBB)
		{
			FOBB* Box = static_cast<FOBB*>(BoundingBox);
			Box->Extents = BoxExtent;
		}
	}
	else
	{
		InOutHandle["BoxExtent"] = FJsonSerializer::VectorToJson(BoxExtent);
	}
}

UClass* UBoxComponent::GetSpecificWidgetClass() const
{
	return UBoxComponentWidget::StaticClass();
}

void UBoxComponent::RenderDebugShape(UBatchLines& BatchLines)
{
	FVector WorldLocation = GetWorldLocation();
	FQuaternion WorldRotation = GetWorldRotationAsQuaternion();
	FVector ScaledExtent = GetScaledBoxExtent();

	FOBB WorldBox;
	WorldBox.Center = WorldLocation;
	WorldBox.Extents = ScaledExtent;
	WorldBox.ScaleRotation = WorldRotation.ToRotationMatrix();

	BatchLines.UpdateBoundingBoxVertices(&WorldBox);
}
