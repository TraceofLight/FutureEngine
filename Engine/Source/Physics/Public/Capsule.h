#pragma once
#include "Physics/Public/BoundingVolume.h"
#include "Global/Vector.h"
#include "Global/Quaternion.h"

struct FAABB;

/**
 * Capsule collision shape
 * New: Create world-space instances for collision detection
 * (No Update() needed - FCapsule is typically used as immutable world-space shape)
 */
struct FCapsule : public IBoundingVolume
{
	FVector Center;
	FQuaternion Rotation;
	float Radius;
	float HalfHeight;  // From center to top (excluding hemisphere)

	FCapsule()
		: Center(FVector(0.0f, 0.0f, 0.0f))
		, Rotation(FQuaternion::Identity())
		, Radius(0.5f)
		, HalfHeight(1.0f)
	{}

	FCapsule(const FVector& InCenter, const FQuaternion& InRotation, float InRadius, float InHalfHeight)
		: Center(InCenter)
		, Rotation(InRotation)
		, Radius(InRadius)
		, HalfHeight(InHalfHeight)
	{}

	bool RaycastHit() const override;
	EBoundingVolumeType GetType() const override { return EBoundingVolumeType::Capsule; }

	void Update(const FMatrix& WorldMatrix) override
	{
		Center = WorldMatrix.GetLocation();
		Rotation = WorldMatrix.ToQuaternion();
	}

	// Helper: Convert to AABB for broad-phase testing
	FAABB ToAABB() const;
};
