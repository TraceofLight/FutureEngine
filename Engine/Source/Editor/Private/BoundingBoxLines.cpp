#include "pch.h"
#include "Editor/Public/BoundingBoxLines.h"
#include "Physics/Public/AABB.h"
#include "Physics/Public/OBB.h"
#include "Physics/Public/BoundingSphere.h"
#include "Physics/Public/Capsule.h"

UBoundingBoxLines::UBoundingBoxLines()
	: Vertices(TArray<FVector>()),
	BoundingBoxLineIdx{
		// 앞면
		0, 1,
		1, 2,
		2, 3,
		3, 0,

		// 뒷면
		4, 5,
		5, 6,
		6, 7,
		7, 4,

		// 옆면 연결
		0, 4,
		1, 5,
		2, 6,
		3, 7
	}
{
	Vertices.Reserve(NumVertices);
	UpdateVertices(GetDisabledBoundingBox());

}

void UBoundingBoxLines::MergeVerticesAt(TArray<FVector>& DestVertices, size_t InsertStartIndex)
{
	// 인덱스 범위 보정
	InsertStartIndex = std::min(static_cast<int32>(InsertStartIndex), DestVertices.Num());

	// 미리 메모리 확보
	DestVertices.Reserve(DestVertices.Num() + std::distance(Vertices.begin(), Vertices.end()));

	// 덮어쓸 수 있는 개수 계산
	size_t OverwriteCount = std::min(
		Vertices.Num(),
		DestVertices.Num() - static_cast<int32>(InsertStartIndex)
	);

	// 기존 요소 덮어쓰기
	std::copy(
		Vertices.begin(),
		Vertices.begin() + OverwriteCount,
		DestVertices.begin() + InsertStartIndex
	);
}

void UBoundingBoxLines::UpdateVertices(const IBoundingVolume* NewBoundingVolume)
{
	switch (NewBoundingVolume->GetType())
	{
	case EBoundingVolumeType::AABB:
		{
			const FAABB* AABB = static_cast<const FAABB*>(NewBoundingVolume);

			float MinX = AABB->Min.X, MinY = AABB->Min.Y, MinZ = AABB->Min.Z;
			float MaxX = AABB->Max.X, MaxY = AABB->Max.Y, MaxZ = AABB->Max.Z;

			CurrentType = EBoundingVolumeType::AABB;
			CurrentNumVertices = NumVertices;
			Vertices.SetNum(NumVertices);

			uint32 Idx = 0;
			Vertices[Idx++] = {MinX, MinY, MinZ}; // Front-Bottom-Left
			Vertices[Idx++] = {MaxX, MinY, MinZ}; // Front-Bottom-Right
			Vertices[Idx++] = {MaxX, MaxY, MinZ}; // Front-Top-Right
			Vertices[Idx++] = {MinX, MaxY, MinZ}; // Front-Top-Left
			Vertices[Idx++] = {MinX, MinY, MaxZ}; // Back-Bottom-Left
			Vertices[Idx++] = {MaxX, MinY, MaxZ}; // Back-Bottom-Right
			Vertices[Idx++] = {MaxX, MaxY, MaxZ}; // Back-Top-Right
			Vertices[Idx] = {MinX, MaxY, MaxZ}; // Back-Top-Left
			break;
		}
	case EBoundingVolumeType::OBB:
		{
			const FOBB* OBB = static_cast<const FOBB*>(NewBoundingVolume);
			const FVector& Extents = OBB->Extents;

			FMatrix OBBToWorld = OBB->ScaleRotation;
			OBBToWorld *= FMatrix::TranslationMatrix(OBB->Center);

			FVector LocalCorners[8] =
			{
				FVector(-Extents.X, -Extents.Y, -Extents.Z), // 0: FBL
				FVector(+Extents.X, -Extents.Y, -Extents.Z), // 1: FBR
				FVector(+Extents.X, +Extents.Y, -Extents.Z), // 2: FTR
				FVector(-Extents.X, +Extents.Y, -Extents.Z), // 3: FTL

				FVector(-Extents.X, -Extents.Y, +Extents.Z), // 4: BBL
				FVector(+Extents.X, -Extents.Y, +Extents.Z), // 5: BBR
				FVector(+Extents.X, +Extents.Y, +Extents.Z), // 6: BTR
				FVector(-Extents.X, +Extents.Y, +Extents.Z)  // 7: BTL
			};

			CurrentType = EBoundingVolumeType::OBB;
			CurrentNumVertices = NumVertices;
			Vertices.SetNum(NumVertices);

			for (uint32 Idx = 0; Idx < 8; ++Idx)
			{
				FVector WorldCorner = OBBToWorld.TransformPosition(LocalCorners[Idx]);

				Vertices[Idx] = {WorldCorner.X, WorldCorner.Y, WorldCorner.Z};
			}
			break;
		}
	case EBoundingVolumeType::SpotLight:
	{
		const FOBB* OBB = static_cast<const FOBB*>(NewBoundingVolume);
		const FVector& Extents = OBB->Extents;

		FMatrix OBBToWorld = OBB->ScaleRotation;
		OBBToWorld *= FMatrix::TranslationMatrix(OBB->Center);

		// 61개의 점들을 로컬에서 찍는다.
		constexpr uint32 NumSegments = 60;
		FVector LocalSpotLight[61];
		LocalSpotLight[0] = FVector(-Extents.X, 0.0f, 0.0f); // 0: Center

		for (int32 i = 0; i < 60;++i)
		{
			LocalSpotLight[i + 1] = FVector(Extents.X, cosf((6.0f * i) * (PI / 180.0f)) * 0.5f, sinf((6.0f * i) * (PI / 180.0f)) * 0.5f); 
		}

		CurrentType = EBoundingVolumeType::SpotLight;
		CurrentNumVertices = SpotLightVeitices;
		Vertices.SetNum(SpotLightVeitices);

		// 61개의 점을 월드로 변환하고 Vertices에 넣는다.
		// 인덱스에도 넣는다.

		// 0번인덱스는 미리 처리
		FVector WorldCorner = OBBToWorld.TransformPosition(LocalSpotLight[0]);
		Vertices[0] = { WorldCorner.X, WorldCorner.Y, WorldCorner.Z };

		SpotLightLineIdx.Empty();
		SpotLightLineIdx.Reserve(NumSegments * 4);

		// 꼭지점에서 원으로 뻗는 60개의 선을 월드로 바꾸고 인덱스 번호를 지정한다
		for (uint32 Idx = 1; Idx < 61; ++Idx)
		{
			FVector WorldCorner = OBBToWorld.TransformPosition(LocalSpotLight[Idx]);

			Vertices[Idx] = { WorldCorner.X, WorldCorner.Y, WorldCorner.Z };
			SpotLightLineIdx.Emplace(0);
			SpotLightLineIdx.Emplace(static_cast<int32>(Idx));
		}

		// 원에서 각 점을 잇는 선의 인덱스 번호를 지정한다
		SpotLightLineIdx.Emplace(60);
		SpotLightLineIdx.Emplace(1);
		for (uint32 Idx = 1; Idx < 60; ++Idx)
		{
			SpotLightLineIdx.Emplace(static_cast<int32>(Idx));
			SpotLightLineIdx.Emplace(static_cast<int32>(Idx + 1));
		}
		break;
	}
	case EBoundingVolumeType::Sphere:
	{
		const FBoundingSphere* Sphere = static_cast<const FBoundingSphere*>(NewBoundingVolume);

		const FVector Center = Sphere->Center;
		const float Radius = Sphere->Radius;

		CurrentType = EBoundingVolumeType::Sphere;
		CurrentNumVertices = SphereVertices;
		Vertices.SetNum(SphereVertices);

		uint32 VertexIndex = 0;

		// 1) XY 평면 대원
		for (int32 Step = 0; Step < 60; ++Step)
		{
			const float AngleRadians = (6.0f * Step) * (PI / 180.0f);
			const float CosValue = cosf(AngleRadians);
			const float SinValue = sinf(AngleRadians);

			Vertices[VertexIndex++] = FVector(
				Center.X + CosValue * Radius,
				Center.Y + SinValue * Radius,
				Center.Z
			);
		}

		// 2) XZ 평면 대원
		for (int32 Step = 0; Step < 60; ++Step)
		{
			const float AngleRadians = (6.0f * Step) * (PI / 180.0f);
			const float CosValue = cosf(AngleRadians);
			const float SinValue = sinf(AngleRadians);

			Vertices[VertexIndex++] = FVector(
				Center.X + CosValue * Radius,
				Center.Y,
				Center.Z + SinValue * Radius
			);
		}

		// 3) YZ 평면 대원
		for (int32 Step = 0; Step < 60; ++Step)
		{
			const float AngleRadians = (6.0f * Step) * (PI / 180.0f);
			const float CosValue = cosf(AngleRadians);
			const float SinValue = sinf(AngleRadians);

			Vertices[VertexIndex++] = FVector(
				Center.X,
				Center.Y + CosValue * Radius,
				Center.Z + SinValue * Radius
			);
		}

		int32 LineIndex = 0;

		// XY 원 인덱스
		{
			const int32 Base = 0;
			for (int32 Step = 0; Step < 59; ++Step)
			{
				SphereLineIdx[LineIndex++] = Base + Step;
				SphereLineIdx[LineIndex++] = Base + Step + 1;
			}
			SphereLineIdx[LineIndex++] = Base + 59;
			SphereLineIdx[LineIndex++] = Base + 0;
		}

		// XZ 원 인덱스
		{
			const int32 Base = 60;
			for (int32 Step = 0; Step < 59; ++Step)
			{
				SphereLineIdx[LineIndex++] = Base + Step;
				SphereLineIdx[LineIndex++] = Base + Step + 1;
			}
			SphereLineIdx[LineIndex++] = Base + 59;
			SphereLineIdx[LineIndex++] = Base + 0;
		}

		// YZ 원 인덱스
		{
			const int32 Base = 120;
			for (int32 Step = 0; Step < 59; ++Step)
			{
				SphereLineIdx[LineIndex++] = Base + Step;
				SphereLineIdx[LineIndex++] = Base + Step + 1;
			}
			SphereLineIdx[LineIndex++] = Base + 59;
			SphereLineIdx[LineIndex++] = Base + 0;
		}

		break;
	}
	case EBoundingVolumeType::Capsule:
	{
		const FCapsule* Capsule = static_cast<const FCapsule*>(NewBoundingVolume);

		const FVector Center = Capsule->Center;
		const FQuaternion Rotation = Capsule->Rotation;
		const float Radius = Capsule->Radius;
		const float HalfHeight = Capsule->HalfHeight;

		CurrentType = EBoundingVolumeType::Capsule;
		CurrentNumVertices = CapsuleVertices;
		Vertices.SetNum(CapsuleVertices);

		constexpr int32 CircleSegments = 32;
		constexpr int32 HemisphereRings = 4;
		constexpr int32 HemisphereSegments = 8;

		uint32 VertexIndex = 0;

		// Local up vector (capsule extends along Z-axis in local space)
		FVector LocalUp(0.0f, 0.0f, 1.0f);
		FVector WorldUp = Rotation.RotateVector(LocalUp);

		// Top and bottom circle centers
		FVector TopCenter = Center + WorldUp * HalfHeight;
		FVector BottomCenter = Center - WorldUp * HalfHeight;

		// 1) Top circle (32 segments on XY plane, rotated)
		for (int32 i = 0; i < CircleSegments; ++i)
		{
			float Angle = (2.0f * PI * i) / CircleSegments;
			FVector LocalPos(Radius * cosf(Angle), Radius * sinf(Angle), 0.0f);
			FVector WorldPos = Rotation.RotateVector(LocalPos) + TopCenter;
			Vertices[VertexIndex++] = WorldPos;
		}

		// 2) Bottom circle (32 segments on XY plane, rotated)
		for (int32 i = 0; i < CircleSegments; ++i)
		{
			float Angle = (2.0f * PI * i) / CircleSegments;
			FVector LocalPos(Radius * cosf(Angle), Radius * sinf(Angle), 0.0f);
			FVector WorldPos = Rotation.RotateVector(LocalPos) + BottomCenter;
			Vertices[VertexIndex++] = WorldPos;
		}

		// 3) Top hemisphere arcs (4 arcs, each with 8 segments)
		for (int32 ArcIdx = 0; ArcIdx < 4; ++ArcIdx)
		{
			float ArcAngle = (2.0f * PI * ArcIdx) / 4.0f; // 0, 90, 180, 270 degrees

			for (int32 Ring = 1; Ring <= HemisphereSegments; ++Ring)
			{
				float Phi = (PI / 2.0f) * (Ring / (float)HemisphereSegments); // 0 to 90 degrees
				float Z = Radius * sinf(Phi);
				float R = Radius * cosf(Phi);

				FVector LocalPos(
					R * cosf(ArcAngle),
					R * sinf(ArcAngle),
					Z
				);
				FVector WorldPos = Rotation.RotateVector(LocalPos) + TopCenter;
				Vertices[VertexIndex++] = WorldPos;
			}
		}

		// 4) Bottom hemisphere arcs (4 arcs, each with 8 segments)
		for (int32 ArcIdx = 0; ArcIdx < 4; ++ArcIdx)
		{
			float ArcAngle = (2.0f * PI * ArcIdx) / 4.0f;

			for (int32 Ring = 1; Ring <= HemisphereSegments; ++Ring)
			{
				float Phi = (PI / 2.0f) * (Ring / (float)HemisphereSegments);
				float Z = -Radius * sinf(Phi); // Negative for bottom hemisphere
				float R = Radius * cosf(Phi);

				FVector LocalPos(
					R * cosf(ArcAngle),
					R * sinf(ArcAngle),
					Z
				);
				FVector WorldPos = Rotation.RotateVector(LocalPos) + BottomCenter;
				Vertices[VertexIndex++] = WorldPos;
			}
		}

		int32 LineIndex = 0;

		// Top circle indices
		{
			const int32 Base = 0;
			for (int32 i = 0; i < CircleSegments - 1; ++i)
			{
				CapsuleLineIdx[LineIndex++] = Base + i;
				CapsuleLineIdx[LineIndex++] = Base + i + 1;
			}
			CapsuleLineIdx[LineIndex++] = Base + CircleSegments - 1;
			CapsuleLineIdx[LineIndex++] = Base + 0;
		}

		// Bottom circle indices
		{
			const int32 Base = CircleSegments;
			for (int32 i = 0; i < CircleSegments - 1; ++i)
			{
				CapsuleLineIdx[LineIndex++] = Base + i;
				CapsuleLineIdx[LineIndex++] = Base + i + 1;
			}
			CapsuleLineIdx[LineIndex++] = Base + CircleSegments - 1;
			CapsuleLineIdx[LineIndex++] = Base + 0;
		}

		// Vertical connecting lines (4 lines at 0, 90, 180, 270 degrees)
		for (int32 i = 0; i < 4; ++i)
		{
			int32 TopIdx = i * (CircleSegments / 4);
			int32 BottomIdx = CircleSegments + TopIdx;
			CapsuleLineIdx[LineIndex++] = TopIdx;
			CapsuleLineIdx[LineIndex++] = BottomIdx;
		}

		// Top hemisphere arc indices
		{
			const int32 Base = CircleSegments * 2;
			for (int32 ArcIdx = 0; ArcIdx < 4; ++ArcIdx)
			{
				int32 ArcBase = Base + ArcIdx * HemisphereSegments;
				int32 CircleStartIdx = ArcIdx * (CircleSegments / 4);

				// Connect circle to first arc point
				CapsuleLineIdx[LineIndex++] = CircleStartIdx;
				CapsuleLineIdx[LineIndex++] = ArcBase;

				// Connect arc segments
				for (int32 i = 0; i < HemisphereSegments - 1; ++i)
				{
					CapsuleLineIdx[LineIndex++] = ArcBase + i;
					CapsuleLineIdx[LineIndex++] = ArcBase + i + 1;
				}
			}
		}

		// Bottom hemisphere arc indices
		{
			const int32 Base = CircleSegments * 2 + HemisphereRings * HemisphereSegments;
			for (int32 ArcIdx = 0; ArcIdx < 4; ++ArcIdx)
			{
				int32 ArcBase = Base + ArcIdx * HemisphereSegments;
				int32 CircleStartIdx = CircleSegments + ArcIdx * (CircleSegments / 4);

				// Connect circle to first arc point
				CapsuleLineIdx[LineIndex++] = CircleStartIdx;
				CapsuleLineIdx[LineIndex++] = ArcBase;

				// Connect arc segments
				for (int32 i = 0; i < HemisphereSegments - 1; ++i)
				{
					CapsuleLineIdx[LineIndex++] = ArcBase + i;
					CapsuleLineIdx[LineIndex++] = ArcBase + i + 1;
				}
			}
		}

		break;
	}
	default:
		break;
	}
}

void UBoundingBoxLines::UpdateSpotLightVertices(const TArray<FVector>& InVertices)
{
	SpotLightLineIdx.Empty();

	if (InVertices.IsEmpty())
	{
		CurrentType = EBoundingVolumeType::SpotLight;
		CurrentNumVertices = 0;
		Vertices.Empty();
		SpotLightLineIdx.Empty();
		return;
	}

	const uint32 NumVerticesRequested = static_cast<uint32>(InVertices.Num());

	CurrentType = EBoundingVolumeType::SpotLight;
	CurrentNumVertices = NumVerticesRequested;
	Vertices.SetNum(NumVerticesRequested);

	std::ranges::copy(InVertices, Vertices.begin());

	constexpr uint32 NumSegments = 40;
	const int32 ApexIndex = 2 * (NumSegments + 1);
	const uint32 OuterStart = ApexIndex + 1;
	const uint32 OuterCount = NumVerticesRequested > OuterStart ? std::min(NumSegments, NumVerticesRequested - OuterStart) : 0;
	const uint32 InnerStart = OuterStart + OuterCount;
	const uint32 InnerCount = NumVerticesRequested > InnerStart ? std::min(NumSegments, NumVerticesRequested - InnerStart) : 0;

	if (OuterCount == 0)
	{
		return;
	}

	SpotLightLineIdx.Reserve((NumSegments * 4) + (OuterCount * 4) + (InnerCount * 4));

	for (uint32 Segment = 0; Segment < NumSegments; ++Segment)
	{
		// xy 평면 위 호
		SpotLightLineIdx.Emplace(Segment);
		SpotLightLineIdx.Emplace(Segment + 1);

		// zx 평면 위 호
		SpotLightLineIdx.Emplace(Segment + NumSegments + 1);
		SpotLightLineIdx.Emplace(Segment + NumSegments + 2);
	}
	
	// Apex에서 outer cone 밑면 각 점까지의 선분
	for (uint32 Segment = 0; Segment < OuterCount; ++Segment)
	{
		SpotLightLineIdx.Emplace(ApexIndex);
		SpotLightLineIdx.Emplace(static_cast<int32>(OuterStart + Segment));
	}
	
	// outer cone 밑면 둘레 선분
	for (uint32 Segment = 0; Segment < OuterCount; ++Segment)
	{
		const int32 Start = static_cast<int32>(OuterStart + Segment);
		const int32 End = static_cast<int32>(OuterStart + ((Segment + 1) % OuterCount));
		SpotLightLineIdx.Emplace(Start);
		SpotLightLineIdx.Emplace(End);
	}

	if (InnerCount >= 2)
	{
		// Apex에서 inner cone 밑면 각 점까지의 선분
		for (uint32 Segment = 0; Segment < InnerCount; ++Segment)
		{
			SpotLightLineIdx.Emplace(ApexIndex);
			SpotLightLineIdx.Emplace(static_cast<int32>(InnerStart + Segment));
		}

		// inner cone 밑면 둘레 선분
		for (uint32 Segment = 0; Segment < InnerCount; ++Segment)
		{
			const int32 Start = static_cast<int32>(InnerStart + Segment);
			const int32 End = static_cast<int32>(InnerStart + ((Segment + 1) % InnerCount));
			SpotLightLineIdx.Emplace(Start);
			SpotLightLineIdx.Emplace(End);
		}
	}
}

int32* UBoundingBoxLines::GetIndices(EBoundingVolumeType BoundingVolumeType)
{
	switch (BoundingVolumeType)
	{
	case EBoundingVolumeType::AABB:
	{
		return BoundingBoxLineIdx;
	}
	case EBoundingVolumeType::OBB:
	{
		return BoundingBoxLineIdx;
	}
	case EBoundingVolumeType::SpotLight:
	{
		return SpotLightLineIdx.IsEmpty() ? nullptr : SpotLightLineIdx.GetData();
	}
	case EBoundingVolumeType::Sphere:
	{
		return SphereLineIdx;
	}
	case EBoundingVolumeType::Capsule:
	{
		return CapsuleLineIdx;
	}
	default:
		break;
	}

	return nullptr;
}


uint32 UBoundingBoxLines::GetNumIndices(EBoundingVolumeType BoundingVolumeType) const
{
	switch (BoundingVolumeType)
	{
	case EBoundingVolumeType::AABB:
	case EBoundingVolumeType::OBB:
		return 24;
	case EBoundingVolumeType::SpotLight:
		return static_cast<uint32>(SpotLightLineIdx.Num());
	case EBoundingVolumeType::Sphere:
		return 360;
	case EBoundingVolumeType::Capsule:
		return 264;
	default:
		break;
	}

	return 0;
}
