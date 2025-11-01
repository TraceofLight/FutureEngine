#pragma once
#include "Component/Public/ShapeComponent.h"

UCLASS()
class UBoxComponent : public UShapeComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UBoxComponent, UShapeComponent)

public:
	UBoxComponent();

	// Box extent (unscaled half-extents)
	FVector GetBoxExtent() const { return BoxExtent; }
	void SetBoxExtent(const FVector& InExtent);
	void InitBoxExtent(const FVector& InExtent);  // Set without triggering updates
	FVector GetScaledBoxExtent() const;
	FVector GetUnscaledBoxExtent() const { return BoxExtent; }

	// Collision system (SOLID)
	FBounds CalcBounds() const override;

	// Overrides
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	virtual UClass* GetSpecificWidgetClass() const override;
	void RenderDebugShape(UBatchLines& BatchLines) override;

private:
	FVector BoxExtent = FVector(0.5f, 0.5f, 0.5f);
};
