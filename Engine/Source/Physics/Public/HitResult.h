#pragma once
#include "Global/Vector.h"

// Forward declarations
class AActor;
class UPrimitiveComponent;

/**
 * Results of a collision/overlap test
 * Simplified version of Unreal Engine's FHitResult
 *
 * Contains information about what was hit, where it was hit,
 * and penetration details for overlap queries
 */
struct FHitResult
{
	// Location of impact/overlap (world space)
	FVector Location;

	// Surface normal at impact point
	FVector Normal;

	// Penetration depth (positive = overlapping, negative = separated)
	float PenetrationDepth;

	// Actor that was hit/overlapped
	AActor* Actor;

	// Component that was hit/overlapped
	UPrimitiveComponent* Component;

	// Did this result block movement?
	bool bBlockingHit;

	FHitResult()
		: Location(0.0f, 0.0f, 0.0f)
		, Normal(0.0f, 0.0f, 1.0f)
		, PenetrationDepth(0.0f)
		, Actor(nullptr)
		, Component(nullptr)
		, bBlockingHit(false)
	{
	}

	FHitResult(const FVector& InLocation, const FVector& InNormal)
		: Location(InLocation)
		, Normal(InNormal)
		, PenetrationDepth(0.0f)
		, Actor(nullptr)
		, Component(nullptr)
		, bBlockingHit(false)
	{
	}
};
