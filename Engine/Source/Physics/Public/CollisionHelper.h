#pragma once
#include "Global/Vector.h"

// Forward declarations
class IBoundingVolume;
struct FBoundingSphere;
struct FOBB;
struct FCapsule;
struct FAABB;

/**
 * Static utility class for collision/overlap testing between different shape types
 * Follows Unreal Engine's pattern of centralized geometry tests
 *
 * Instead of using virtual methods (double dispatch), we use type switching
 * for better performance and simpler code organization
 */
class FCollisionHelper
{
public:
	// === Main Entry Point ===
	// Type switching으로 적절한 shape-pair 테스트 호출
	static bool TestOverlap(const IBoundingVolume* VolumeA, const IBoundingVolume* VolumeB);

	// === Sphere Tests ===
	static bool SphereToSphere(const FBoundingSphere& SphereA, const FBoundingSphere& SphereB);
	static bool SphereToBox(const FBoundingSphere& Sphere, const FOBB& Box);
	static bool SphereToCapsule(const FBoundingSphere& Sphere, const FCapsule& Capsule);

	// === Box Tests ===
	static bool BoxToBox(const FOBB& BoxA, const FOBB& BoxB);
	static bool BoxToCapsule(const FOBB& Box, const FCapsule& Capsule);

	// === Capsule Tests ===
	static bool CapsuleToCapsule(const FCapsule& CapsuleA, const FCapsule& CapsuleB);

	// === Point Containment Tests ===
	static bool IsPointInSphere(const FVector& Point, const FBoundingSphere& Sphere);
	static bool IsPointInBox(const FVector& Point, const FOBB& Box);
	static bool IsPointInCapsule(const FVector& Point, const FCapsule& Capsule);

private:
	// === Helper Functions ===

	// Find closest point on line segment to a given point
	static FVector ClosestPointOnSegment(const FVector& Point, const FVector& SegmentStart, const FVector& SegmentEnd);

	// Find closest points between two line segments
	static void ClosestPointsBetweenSegments(
		const FVector& Seg1Start, const FVector& Seg1End,
		const FVector& Seg2Start, const FVector& Seg2End,
		FVector& OutPoint1, FVector& OutPoint2);
};
