#pragma once
#include "Component/Public/ShapeComponent.h"

UCLASS()
class USphereComponent : public UShapeComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(USphereComponent, UShapeComponent)

public:
	USphereComponent();

	// Sphere radius (unscaled)
	float GetSphereRadius() const { return SphereRadius; }
	void SetSphereRadius(float InRadius);
	void InitSphereRadius(float InRadius);  // Set without triggering updates
	float GetScaledSphereRadius() const;
	float GetUnscaledSphereRadius() const { return SphereRadius; }

	// Collision system (SOLID)
	FBounds CalcBounds() const override;

	// Overrides
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	virtual UClass* GetSpecificWidgetClass() const override;
	void RenderDebugShape(UBatchLines& BatchLines) override;

private:
	float SphereRadius = 0.5f;
};
