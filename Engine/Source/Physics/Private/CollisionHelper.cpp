#include "pch.h"
#include "Physics/Public/CollisionHelper.h"
#include "Physics/Public/BoundingVolume.h"
#include "Physics/Public/BoundingSphere.h"
#include "Physics/Public/OBB.h"
#include "Physics/Public/Capsule.h"
#include "Physics/Public/AABB.h"
#include <algorithm>

// === Main Entry Point ===

bool FCollisionHelper::TestOverlap(const IBoundingVolume* VolumeA, const IBoundingVolume* VolumeB)
{
	if (!VolumeA || !VolumeB)
		return false;

	EBoundingVolumeType TypeA = VolumeA->GetType();
	EBoundingVolumeType TypeB = VolumeB->GetType();

	// Switch on both types to dispatch to appropriate test
	// Handle all N×N combinations (9 for Sphere/Box/Capsule)

	// === Sphere Tests ===
	if (TypeA == EBoundingVolumeType::Sphere && TypeB == EBoundingVolumeType::Sphere)
	{
		return SphereToSphere(*static_cast<const FBoundingSphere*>(VolumeA),
		                       *static_cast<const FBoundingSphere*>(VolumeB));
	}
	if (TypeA == EBoundingVolumeType::Sphere && TypeB == EBoundingVolumeType::OBB)
	{
		return SphereToBox(*static_cast<const FBoundingSphere*>(VolumeA),
		                    *static_cast<const FOBB*>(VolumeB));
	}
	if (TypeA == EBoundingVolumeType::OBB && TypeB == EBoundingVolumeType::Sphere)
	{
		return SphereToBox(*static_cast<const FBoundingSphere*>(VolumeB),
		                    *static_cast<const FOBB*>(VolumeA));
	}
	if (TypeA == EBoundingVolumeType::Sphere && TypeB == EBoundingVolumeType::Capsule)
	{
		return SphereToCapsule(*static_cast<const FBoundingSphere*>(VolumeA),
		                        *static_cast<const FCapsule*>(VolumeB));
	}
	if (TypeA == EBoundingVolumeType::Capsule && TypeB == EBoundingVolumeType::Sphere)
	{
		return SphereToCapsule(*static_cast<const FBoundingSphere*>(VolumeB),
		                        *static_cast<const FCapsule*>(VolumeA));
	}

	// === Box Tests ===
	if (TypeA == EBoundingVolumeType::OBB && TypeB == EBoundingVolumeType::OBB)
	{
		return BoxToBox(*static_cast<const FOBB*>(VolumeA),
		                 *static_cast<const FOBB*>(VolumeB));
	}
	if (TypeA == EBoundingVolumeType::OBB && TypeB == EBoundingVolumeType::Capsule)
	{
		return BoxToCapsule(*static_cast<const FOBB*>(VolumeA),
		                     *static_cast<const FCapsule*>(VolumeB));
	}
	if (TypeA == EBoundingVolumeType::Capsule && TypeB == EBoundingVolumeType::OBB)
	{
		return BoxToCapsule(*static_cast<const FOBB*>(VolumeB),
		                     *static_cast<const FCapsule*>(VolumeA));
	}

	// === Capsule Tests ===
	if (TypeA == EBoundingVolumeType::Capsule && TypeB == EBoundingVolumeType::Capsule)
	{
		return CapsuleToCapsule(*static_cast<const FCapsule*>(VolumeA),
		                         *static_cast<const FCapsule*>(VolumeB));
	}

	// Unknown combination or AABB (handle with broad-phase AABB test as fallback)
	return false;
}

// === Sphere Tests ===

bool FCollisionHelper::SphereToSphere(const FBoundingSphere& SphereA, const FBoundingSphere& SphereB)
{
	float DistSq = (SphereA.Center - SphereB.Center).LengthSquared();
	float RadiusSum = SphereA.Radius + SphereB.Radius;
	return DistSq <= (RadiusSum * RadiusSum);
}

bool FCollisionHelper::SphereToBox(const FBoundingSphere& Sphere, const FOBB& Box)
{
	// Transform sphere center to box local space
	FMatrix InvBoxTransform = Box.ScaleRotation.Inverse();
	FVector LocalSphereCenter = InvBoxTransform.TransformPosition(Sphere.Center - Box.Center);

	// Find closest point on box (in local space, box is axis-aligned with extents)
	FVector ClosestPoint;
	ClosestPoint.X = std::max(-Box.Extents.X, std::min(LocalSphereCenter.X, Box.Extents.X));
	ClosestPoint.Y = std::max(-Box.Extents.Y, std::min(LocalSphereCenter.Y, Box.Extents.Y));
	ClosestPoint.Z = std::max(-Box.Extents.Z, std::min(LocalSphereCenter.Z, Box.Extents.Z));

	// Check distance
	float DistSq = (ClosestPoint - LocalSphereCenter).LengthSquared();
	return DistSq <= (Sphere.Radius * Sphere.Radius);
}

bool FCollisionHelper::SphereToCapsule(const FBoundingSphere& Sphere, const FCapsule& Capsule)
{
	// Get capsule segment (center line)
	FVector CapsuleUp = Capsule.Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f));
	FVector SegmentStart = Capsule.Center - CapsuleUp * Capsule.HalfHeight;
	FVector SegmentEnd = Capsule.Center + CapsuleUp * Capsule.HalfHeight;

	// Find closest point on segment
	FVector ClosestPoint = ClosestPointOnSegment(Sphere.Center, SegmentStart, SegmentEnd);

	// Check distance
	float DistSq = (ClosestPoint - Sphere.Center).LengthSquared();
	float CombinedRadius = Sphere.Radius + Capsule.Radius;
	return DistSq <= (CombinedRadius * CombinedRadius);
}

// === Box Tests ===

bool FCollisionHelper::BoxToBox(const FOBB& BoxA, const FOBB& BoxB, FHitResult* OutHit)
{
	// Delegate to FOBB's SAT implementation with optional hit info
	return BoxA.Intersects(BoxB, OutHit);
}

bool FCollisionHelper::BoxToCapsule(const FOBB& Box, const FCapsule& Capsule)
{
	// Test capsule as two spheres + cylinder body
	FVector CapsuleAxis = Capsule.Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f));
	FVector TopCenter = Capsule.Center + CapsuleAxis * Capsule.HalfHeight;
	FVector BottomCenter = Capsule.Center - CapsuleAxis * Capsule.HalfHeight;

	// Test top sphere
	FBoundingSphere TopSphere(TopCenter, Capsule.Radius);
	if (SphereToBox(TopSphere, Box))
		return true;

	// Test bottom sphere
	FBoundingSphere BottomSphere(BottomCenter, Capsule.Radius);
	if (SphereToBox(BottomSphere, Box))
		return true;

	// Test cylinder body: find closest point on capsule segment to box
	// Transform box center to capsule segment space
	FVector SegmentStart = BottomCenter;
	FVector SegmentEnd = TopCenter;
	FVector ClosestOnSegment = ClosestPointOnSegment(Box.Center, SegmentStart, SegmentEnd);

	// Create sphere at closest point and test against box
	FBoundingSphere TestSphere(ClosestOnSegment, Capsule.Radius);
	return SphereToBox(TestSphere, Box);
}

// === Capsule Tests ===

bool FCollisionHelper::CapsuleToCapsule(const FCapsule& CapsuleA, const FCapsule& CapsuleB)
{
	// Get segment endpoints for both capsules
	FVector AxisA = CapsuleA.Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f));
	FVector A_Start = CapsuleA.Center - AxisA * CapsuleA.HalfHeight;
	FVector A_End = CapsuleA.Center + AxisA * CapsuleA.HalfHeight;

	FVector AxisB = CapsuleB.Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f));
	FVector B_Start = CapsuleB.Center - AxisB * CapsuleB.HalfHeight;
	FVector B_End = CapsuleB.Center + AxisB * CapsuleB.HalfHeight;

	// Find closest points between segments
	FVector ClosestA, ClosestB;
	ClosestPointsBetweenSegments(A_Start, A_End, B_Start, B_End, ClosestA, ClosestB);

	// Check distance
	float DistSq = (ClosestA - ClosestB).LengthSquared();
	float CombinedRadius = CapsuleA.Radius + CapsuleB.Radius;
	return DistSq <= (CombinedRadius * CombinedRadius);
}

// === Point Containment Tests ===

bool FCollisionHelper::IsPointInSphere(const FVector& Point, const FBoundingSphere& Sphere)
{
	float DistSq = (Point - Sphere.Center).LengthSquared();
	return DistSq <= (Sphere.Radius * Sphere.Radius);
}

bool FCollisionHelper::IsPointInBox(const FVector& Point, const FOBB& Box)
{
	// Transform point to box local space
	FMatrix InvBoxTransform = Box.ScaleRotation.Inverse();
	FVector LocalPoint = InvBoxTransform.TransformPosition(Point - Box.Center);

	// Check if point is inside extents in local space
	return (std::abs(LocalPoint.X) <= Box.Extents.X) &&
	       (std::abs(LocalPoint.Y) <= Box.Extents.Y) &&
	       (std::abs(LocalPoint.Z) <= Box.Extents.Z);
}

bool FCollisionHelper::IsPointInCapsule(const FVector& Point, const FCapsule& Capsule)
{
	// Get capsule segment (center line)
	FVector CapsuleUp = Capsule.Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f));
	FVector SegmentStart = Capsule.Center - CapsuleUp * Capsule.HalfHeight;
	FVector SegmentEnd = Capsule.Center + CapsuleUp * Capsule.HalfHeight;

	// Find closest point on segment
	FVector ClosestPoint = ClosestPointOnSegment(Point, SegmentStart, SegmentEnd);

	// Check if distance from point to segment is within radius
	float DistSq = (Point - ClosestPoint).LengthSquared();
	return DistSq <= (Capsule.Radius * Capsule.Radius);
}

// === Helper Functions ===

FVector FCollisionHelper::ClosestPointOnSegment(const FVector& Point, const FVector& SegmentStart, const FVector& SegmentEnd)
{
	FVector Segment = SegmentEnd - SegmentStart;
	FVector PointVector = Point - SegmentStart;

	float SegmentLengthSq = Segment.LengthSquared();
	if (SegmentLengthSq < 0.0001f)  // Degenerate segment
	{
		return SegmentStart;
	}

	float t = PointVector.Dot(Segment) / SegmentLengthSq;
	t = std::max(0.0f, std::min(1.0f, t));  // Clamp to [0,1]

	return SegmentStart + Segment * t;
}

void FCollisionHelper::ClosestPointsBetweenSegments(
	const FVector& Seg1Start, const FVector& Seg1End,
	const FVector& Seg2Start, const FVector& Seg2End,
	FVector& OutPoint1, FVector& OutPoint2)
{
	FVector d1 = Seg1End - Seg1Start;
	FVector d2 = Seg2End - Seg2Start;
	FVector r = Seg1Start - Seg2Start;

	float a = d1.Dot(d1);
	float e = d2.Dot(d2);
	float f = d2.Dot(r);

	// Check if either or both segments degenerate into points
	float EPSILON = 0.0001f;

	if (a <= EPSILON && e <= EPSILON)
	{
		// Both segments degenerate into points
		OutPoint1 = Seg1Start;
		OutPoint2 = Seg2Start;
		return;
	}

	float s, t;

	if (a <= EPSILON)
	{
		// First segment degenerates into a point
		s = 0.0f;
		t = f / e;  // s = 0 => t = (b*s + f) / e = f / e
		t = std::max(0.0f, std::min(1.0f, t));
	}
	else
	{
		float c = d1.Dot(r);
		if (e <= EPSILON)
		{
			// Second segment degenerates into a point
			t = 0.0f;
			s = std::max(0.0f, std::min(1.0f, -c / a));
		}
		else
		{
			// General case
			float b = d1.Dot(d2);
			float denom = a * e - b * b;  // Always nonnegative

			// If segments not parallel, compute closest point on infinite lines
			if (denom != 0.0f)
			{
				s = (b * f - c * e) / denom;
				s = std::max(0.0f, std::min(1.0f, s));
			}
			else
			{
				// Parallel segments, pick arbitrary s
				s = 0.0f;
			}

			// Compute point on L2 closest to S1(s)
			t = (b * s + f) / e;

			// If t in [0,1] done. Else clamp t, recompute s for new value of t
			if (t < 0.0f)
			{
				t = 0.0f;
				s = std::max(0.0f, std::min(1.0f, -c / a));
			}
			else if (t > 1.0f)
			{
				t = 1.0f;
				s = std::max(0.0f, std::min(1.0f, (b - c) / a));
			}
		}
	}

	OutPoint1 = Seg1Start + d1 * s;
	OutPoint2 = Seg2Start + d2 * t;
}
