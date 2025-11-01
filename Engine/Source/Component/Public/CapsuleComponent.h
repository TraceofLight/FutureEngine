#pragma once
#include "Component/Public/ShapeComponent.h"

UCLASS()
class UCapsuleComponent : public UShapeComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UCapsuleComponent, UShapeComponent)

public:
	UCapsuleComponent();

	// Capsule properties (unscaled)
	float GetCapsuleRadius() const { return CapsuleRadius; }
	float GetCapsuleHalfHeight() const { return CapsuleHalfHeight; }
	void SetCapsuleRadius(float InRadius, bool bUpdateOverlaps = true);
	void SetCapsuleHalfHeight(float InHalfHeight, bool bUpdateOverlaps = true);
	void SetCapsuleSize(float InRadius, float InHalfHeight, bool bUpdateOverlaps = true);
	void InitCapsuleSize(float InRadius, float InHalfHeight);  // Set without triggering updates

	float GetScaledCapsuleRadius() const;
	float GetScaledCapsuleHalfHeight() const;
	float GetUnscaledCapsuleRadius() const { return CapsuleRadius; }
	float GetUnscaledCapsuleHalfHeight() const { return CapsuleHalfHeight; }

	// Collision system (SOLID)
	FBounds CalcBounds() const override;

	// Overrides
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	virtual UClass* GetSpecificWidgetClass() const override;
	void RenderDebugShape(UBatchLines& BatchLines) override;

private:
	float CapsuleRadius = 0.5f;
	float CapsuleHalfHeight = 1.0f;  // From center to top (excluding hemisphere)
};
