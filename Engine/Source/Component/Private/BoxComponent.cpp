#include "pch.h"
#include "Component/Public/BoxComponent.h"
#include "Physics/Public/OBB.h"
#include "Utility/Public/JsonSerializer.h"
#include "Render/UI/Widget/Public/BoxComponentWidget.h"
#include "Editor/Public/BatchLines.h"
#include <algorithm>

IMPLEMENT_CLASS(UBoxComponent, UShapeComponent)

UBoxComponent::UBoxComponent()
{
	BoxExtent = FVector(0.5f, 0.5f, 0.5f);
	UpdateBoundingVolume();
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
		UpdateBoundingVolume();
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

void UBoxComponent::UpdateBoundingVolume()
{
	// Clean up old bounding volume if we own it
	if (bOwnsBoundingBox && BoundingBox)
	{
		delete BoundingBox;
		BoundingBox = nullptr;
	}

	bOwnsBoundingBox = true;
	FOBB* Box = new FOBB();
	Box->Center = FVector(0.0f, 0.0f, 0.0f); // Local space
	Box->Extents = BoxExtent;
	Box->ScaleRotation = FMatrix::Identity();
	BoundingBox = Box;
	bIsAABBCacheDirty = true;
}

void UBoxComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FVector ReadBoxExtent;
		FJsonSerializer::ReadVector(InOutHandle, "BoxExtent", ReadBoxExtent, FVector(0.5f, 0.5f, 0.5f));
		BoxExtent = ReadBoxExtent;
		UpdateBoundingVolume();
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
