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

class IBoundingVolume
{
public:
	virtual ~IBoundingVolume() = default;
	virtual bool RaycastHit() const = 0;
	virtual void Update(const FMatrix& WorldMatrix) {}
	virtual EBoundingVolumeType GetType() const = 0;
};
