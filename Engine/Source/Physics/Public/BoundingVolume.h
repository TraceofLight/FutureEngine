#pragma once

enum class EBoundingVolumeType
{
	None,
	AABB,    // Axis-Aligned Bounding Box
	OBB,
	SpotLight,
	Sphere,  // Bounding Sphere
	Capsule  // Capsule Shape
};

/**
 * Base interface for bounding volumes
 * Legacy: Used with Update() for mutable shapes (Octree, culling, etc)
 * New: Create immutable world-space shapes for collision detection
 */
class IBoundingVolume
{
public:
	virtual ~IBoundingVolume() = default;
	virtual bool RaycastHit() const = 0;
	virtual void Update(const FMatrix& WorldMatrix) {}  // Legacy: for backward compatibility
	virtual EBoundingVolumeType GetType() const = 0;
};
