#pragma once
#include "Physics/Public/BoundingVolume.h"
#include "Global/Vector.h"

struct FBoundingSphere : public IBoundingVolume
{
	FVector Center;
	float Radius;

	FBoundingSphere(const FVector& InCenter, float InRadius)
		: Center(InCenter), Radius(InRadius) {}

	bool RaycastHit() const override;
	EBoundingVolumeType GetType() const override { return EBoundingVolumeType::Sphere; }
};
