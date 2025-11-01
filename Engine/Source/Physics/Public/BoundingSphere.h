#pragma once
#include "Physics/Public/BoundingVolume.h"
#include "Global/Vector.h"

/**
 * Sphere collision shape
 * Legacy: Can be used with Update() (not typically needed for spheres)
 * New: Create world-space instances for collision detection
 */
struct FBoundingSphere : public IBoundingVolume
{
	FVector Center;
	float Radius;

	FBoundingSphere(const FVector& InCenter, float InRadius)
		: Center(InCenter), Radius(InRadius) {}

	bool RaycastHit() const override;
	EBoundingVolumeType GetType() const override { return EBoundingVolumeType::Sphere; }

	void Update(const FMatrix& WorldMatrix) override
	{
		Center = WorldMatrix.GetLocation();
	}
};
