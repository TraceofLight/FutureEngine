#include "pch.h"
#include "Component/Public/ShapeComponent.h"

IMPLEMENT_ABSTRACT_CLASS(UShapeComponent, UPrimitiveComponent)

UShapeComponent::UShapeComponent()
{
	bCanEverTick = false;
	bGenerateHitEvents = true;  // Shapes default to blocking collision
}
