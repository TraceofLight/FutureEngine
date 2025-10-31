#include "pch.h"
#include "Component/Public/ShapeComponent.h"

IMPLEMENT_ABSTRACT_CLASS(UShapeComponent, UPrimitiveComponent)

UShapeComponent::UShapeComponent()
{
	bCanEverTick = false;
	bOwnsBoundingBox = true;
}

const IBoundingVolume* UShapeComponent::GetBoundingBox()
{
	if (BoundingBox)
	{
		BoundingBox->Update(GetWorldTransformMatrix());
	}
	return BoundingBox;
}

void UShapeComponent::MarkAsDirty()
{
	Super::MarkAsDirty();
	UpdateBoundingVolume();
	bIsAABBCacheDirty = true;
}
