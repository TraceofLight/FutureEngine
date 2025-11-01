#pragma once
#include "Physics/Public/BoundingVolume.h"

struct FAABB;
struct FHitResult;

/**
 * Oriented box collision shape
 * Legacy: Uses Update() for mutable transformation (Octree, culling)
 * New: Create world-space instances for collision detection
 */
struct FOBB : public IBoundingVolume
{
    FOBB() : Center(0.0f, 0.0f, 0.0f), Extents(0.0f, 0.0f, 0.0f), ScaleRotation(FMatrix::Identity())
    {
    }

    FOBB(const FVector& InCenter, const FVector& InExtents, const FMatrix& InRotation)
		: Center(InCenter), Extents(InExtents), ScaleRotation(InRotation)
	{}

    struct FAABB ToWorldAABB() const;

	//--- IBoundingVolume interface ---

    FVector Center;
    FVector Extents;
    FMatrix ScaleRotation;  // Rotation + Scale matrix (no translation)

    
    bool RaycastHit() const override { return false;}

    /**  @brief Collision detection algorithm based on SAT */
    bool Intersects(const FAABB& Other) const;

    /**  @brief Collision detection algorithm based on SAT */
    bool Intersects(const FOBB& Other) const;

    /**  @brief Collision detection with detailed hit info (Normal, PenetrationDepth) */
    bool Intersects(const FOBB& Other, FHitResult* OutHit) const;

    void Update(const FMatrix& WorldMatrix) override;

    EBoundingVolumeType GetType() const override { return EBoundingVolumeType::OBB; }

    FVector GetExtents()const
    {
        return Extents;
    }
};

struct FSpotLightOBB : public FOBB
{
	using FOBB::FOBB;

	EBoundingVolumeType GetType() const override { return EBoundingVolumeType::SpotLight; }
};
