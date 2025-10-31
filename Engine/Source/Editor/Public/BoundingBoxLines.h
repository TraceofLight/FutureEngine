#pragma once
#include "Core/Public/Object.h"
#include "Physics/Public/AABB.h"

class UBoundingBoxLines : UObject
{
public:
	UBoundingBoxLines();
	~UBoundingBoxLines() = default;

	void MergeVerticesAt(TArray<FVector>& DestVertices, size_t InsertStartIndex);
	void UpdateVertices(const IBoundingVolume* NewBoundingVolume);
	void UpdateSpotLightVertices(const TArray<FVector>& InVertices);
	int32* GetIndices(EBoundingVolumeType BoundingVolumeType);
	uint32 GetNumVertices() const
	{
		return CurrentNumVertices;
	}
	uint32 GetNumIndices(EBoundingVolumeType BoundingVolumeType) const;
	EBoundingVolumeType GetCurrentType() const
	{
		return CurrentType;
	}

	FAABB* GetDisabledBoundingBox() { return &DisabledBoundingBox; }

private:
	TArray<FVector> Vertices;
	uint32 NumVertices = 8;
	uint32 SpotLightVeitices = 61;
	uint32 SphereVertices = 180;      // 3개 대원 × 60 세그먼트
	uint32 CapsuleVertices = 128;     // 상하 원(64) + 상하 반구(64)
	uint32 CurrentNumVertices = NumVertices;
	EBoundingVolumeType CurrentType = EBoundingVolumeType::AABB;
	FAABB DisabledBoundingBox = FAABB(FVector(0, 0, 0), FVector(0, 0, 0));

	int32 BoundingBoxLineIdx[24];
	TArray<int32> SpotLightLineIdx;

	int32 SphereLineIdx[360];         // 각 대원 60세그 × 2 인덱스 × 3개
	int32 CapsuleLineIdx[264];        // 상하 원(128) + 연결선(8) + 상하 반구(128)
};

