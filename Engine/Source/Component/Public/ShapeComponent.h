#pragma once
#include "Component/Public/PrimitiveComponent.h"

// Forward declarations
class UBatchLines;

// Abstract base class for simple collision shapes
UCLASS()
class UShapeComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UShapeComponent, UPrimitiveComponent)

public:
	UShapeComponent();

	// Render debug visualization for this shape in world space
	virtual void RenderDebugShape(UBatchLines& BatchLines) = 0;
};
