#include "pch.h"
#include "Physics/Public/Capsule.h"
#include "Physics/Public/AABB.h"
#include <algorithm>

bool FCapsule::RaycastHit() const
{
	// TODO: Implement capsule raycast
	return false;
}

FAABB FCapsule::ToAABB() const
{
	// Capsule extends from Center - (0,0,HalfHeight) to Center + (0,0,HalfHeight)
	// Plus Radius in all directions

	// Get the capsule's up vector (Z-axis in local space, rotated)
	FVector LocalUp(0.0f, 0.0f, 1.0f);
	FVector WorldUp = Rotation.RotateVector(LocalUp);

	// Top and bottom sphere centers
	FVector TopCenter = Center + WorldUp * HalfHeight;
	FVector BottomCenter = Center - WorldUp * HalfHeight;

	// AABB must contain both spheres
	FVector Min = FVector(
		std::min(TopCenter.X, BottomCenter.X) - Radius,
		std::min(TopCenter.Y, BottomCenter.Y) - Radius,
		std::min(TopCenter.Z, BottomCenter.Z) - Radius
	);

	FVector Max = FVector(
		std::max(TopCenter.X, BottomCenter.X) + Radius,
		std::max(TopCenter.Y, BottomCenter.Y) + Radius,
		std::max(TopCenter.Z, BottomCenter.Z) + Radius
	);

	return FAABB(Min, Max);
}
