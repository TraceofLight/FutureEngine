#pragma once
#include "Global/Vector.h"

/**
 * Simple axis-aligned bounding box for broad-phase spatial queries
 * Used for: Octree partitioning, frustum culling, fast rejection tests
 * NOT used for: Actual collision detection (use FCollisionShape for that)
 */
struct FBounds
{
	FVector Min;
	FVector Max;

	FBounds()
		: Min(0.f, 0.f, 0.f)
		, Max(0.f, 0.f, 0.f)
	{}

	FBounds(const FVector& InMin, const FVector& InMax)
		: Min(InMin)
		, Max(InMax)
	{}

	// Create bounds from center and extents
	static FBounds FromCenterAndExtent(const FVector& Center, const FVector& Extent)
	{
		return FBounds(Center - Extent, Center + Extent);
	}

	// Create bounds from sphere
	static FBounds FromSphere(const FVector& Center, float Radius)
	{
		FVector Extent(Radius, Radius, Radius);
		return FBounds(Center - Extent, Center + Extent);
	}

	FVector GetCenter() const
	{
		return (Min + Max) * 0.5f;
	}

	FVector GetExtent() const
	{
		return (Max - Min) * 0.5f;
	}

	// Check if this bounds overlaps another
	bool Overlaps(const FBounds& Other) const
	{
		return (Min.X <= Other.Max.X && Max.X >= Other.Min.X) &&
		       (Min.Y <= Other.Max.Y && Max.Y >= Other.Min.Y) &&
		       (Min.Z <= Other.Max.Z && Max.Z >= Other.Min.Z);
	}

	// Calculate surface area (for SAH in octree)
	float GetSurfaceArea() const
	{
		FVector Size = Max - Min;
		return 2.0f * (Size.X * Size.Y + Size.Y * Size.Z + Size.Z * Size.X);
	}

	// Transform bounds by matrix
	FBounds TransformBy(const FMatrix& Transform) const
	{
		// Transform all 8 corners and find min/max
		FVector Corners[8] =
		{
			FVector(Min.X, Min.Y, Min.Z), FVector(Max.X, Min.Y, Min.Z),
			FVector(Min.X, Max.Y, Min.Z), FVector(Max.X, Max.Y, Min.Z),
			FVector(Min.X, Min.Y, Max.Z), FVector(Max.X, Min.Y, Max.Z),
			FVector(Min.X, Max.Y, Max.Z), FVector(Max.X, Max.Y, Max.Z)
		};

		FVector NewMin(+FLT_MAX, +FLT_MAX, +FLT_MAX);
		FVector NewMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for (int32 i = 0; i < 8; i++)
		{
			FVector TransformedCorner = Transform.TransformPosition(Corners[i]);
			NewMin.X = std::min(NewMin.X, TransformedCorner.X);
			NewMin.Y = std::min(NewMin.Y, TransformedCorner.Y);
			NewMin.Z = std::min(NewMin.Z, TransformedCorner.Z);
			NewMax.X = std::max(NewMax.X, TransformedCorner.X);
			NewMax.Y = std::max(NewMax.Y, TransformedCorner.Y);
			NewMax.Z = std::max(NewMax.Z, TransformedCorner.Z);
		}

		return FBounds(NewMin, NewMax);
	}
};
