#include "pch.h"
#include "Editor/Public/GizmoGeometry.h"

#include "Editor/Public/GizmoTypes.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Manager/UI/Public/ViewportManager.h"

void FGizmoGeometry::CreateTempBuffers(const TArray<FNormalVertex>& InVertices, const TArray<uint32>& InIndices,
                                        ID3D11Buffer** OutVB, ID3D11Buffer** OutIB)
{
	D3D11_BUFFER_DESC VBDesc = {};
	VBDesc.ByteWidth = static_cast<UINT>(sizeof(FNormalVertex) * InVertices.size());
	VBDesc.Usage = D3D11_USAGE_IMMUTABLE;
	VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA VBInitData = {};
	VBInitData.pSysMem = InVertices.data();

	URenderer::GetInstance().GetDevice()->CreateBuffer(&VBDesc, &VBInitData, OutVB);

	D3D11_BUFFER_DESC IBDesc = {};
	IBDesc.ByteWidth = static_cast<UINT>(sizeof(uint32) * InIndices.size());
	IBDesc.Usage = D3D11_USAGE_IMMUTABLE;
	IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA IBInitData = {};
	IBInitData.pSysMem = InIndices.data();

	URenderer::GetInstance().GetDevice()->CreateBuffer(&IBDesc, &IBInitData, OutIB);
}

void FGizmoGeometry::GenerateCircleLineMesh(const FVector& Axis0, const FVector& Axis1,
                                             float Radius, float Thickness,
                                             TArray<FNormalVertex>& OutVertices, TArray<uint32>& OutIndices)
{
	OutVertices.clear();
	OutIndices.clear();

	constexpr int32 NumSegments = 64;
	constexpr int32 NumThicknessSegments = 6;
	const float HalfThickness = Thickness * 0.5f;

	FVector RotationAxis = Axis0.Cross(Axis1);
	RotationAxis.Normalize();

	for (int32 i = 0; i <= NumSegments; ++i)
	{
		const float Angle = (2.0f * PI * static_cast<float>(i)) / NumSegments;

		const FQuaternion Rot = FQuaternion::FromAxisAngle(RotationAxis, -Angle);
		FVector CircleDir = Rot.RotateVector(Axis0);
		CircleDir.Normalize();

		const FVector CirclePoint = CircleDir * Radius;

		FVector Tangent = RotationAxis.Cross(CircleDir);
		Tangent.Normalize();

		const FVector ThicknessAxis1 = CircleDir;
		const FVector ThicknessAxis2 = Tangent;

		for (int32 j = 0; j < NumThicknessSegments; ++j)
		{
			const float ThickAngle = (2.0f * PI * static_cast<float>(j)) / NumThicknessSegments;
			const float CosThick = cosf(ThickAngle);
			const float SinThick = sinf(ThickAngle);

			const FVector Offset = ThicknessAxis1 * (HalfThickness * CosThick) + ThicknessAxis2 * (HalfThickness * SinThick);
			FNormalVertex Vtx;
			Vtx.Position = CirclePoint + Offset;
			Vtx.Normal = Offset.GetNormalized();
			Vtx.Color = FVector4(1, 1, 1, 1);
			OutVertices.push_back(Vtx);
		}
	}

	for (int32 i = 0; i < NumSegments; ++i)
	{
		for (int32 j = 0; j < NumThicknessSegments; ++j)
		{
			const int32 Current = i * NumThicknessSegments + j;
			const int32 Next = i * NumThicknessSegments + ((j + 1) % NumThicknessSegments);
			const int32 NextRing = (i + 1) * NumThicknessSegments + j;
			const int32 NextRingNext = (i + 1) * NumThicknessSegments + ((j + 1) % NumThicknessSegments);

			OutIndices.push_back(Current);
			OutIndices.push_back(NextRing);
			OutIndices.push_back(Next);

			OutIndices.push_back(Next);
			OutIndices.push_back(NextRing);
			OutIndices.push_back(NextRingNext);
		}
	}
}

void FGizmoGeometry::GenerateRotationArcMesh(const FVector& Axis0, const FVector& Axis1,
                                              float InnerRadius, float OuterRadius, float Thickness, float AngleInRadians,
                                              const FVector& StartDirection, TArray<FNormalVertex>& OutVertices,
                                              TArray<uint32>& OutIndices)
{
	OutVertices.clear();
	OutIndices.clear();

	if (std::abs(AngleInRadians) < 0.001f)
	{
		return;
	}

	const float AbsAngle = std::abs(AngleInRadians);
	const float AngleRatio = AbsAngle / (2.0f * PI);
	const int32 NumArcPoints = std::max(2, static_cast<int32>(FGizmoConstants::QuarterRingSegments * 4 * AngleRatio)) + 1;
	constexpr int32 NumThicknessSegments = 6;

	FVector ZAxis = Axis0.Cross(Axis1);
	ZAxis.Normalize();

	FVector StartAxis = Axis0;
	if (StartDirection.LengthSquared() > 0.001f)
	{
		FVector ProjectedStart = StartDirection - ZAxis * StartDirection.Dot(ZAxis);
		if (ProjectedStart.LengthSquared() > 0.001f)
		{
			StartAxis = ProjectedStart.GetNormalized();
		}
	}
	const float SignedAngle = AngleInRadians;
	const float RotationDirection = (SignedAngle >= 0.0f) ? -1.0f : 1.0f;

	const float MidRadius = (InnerRadius + OuterRadius) * 0.5f;
	const float RingWidth = (OuterRadius - InnerRadius) * 0.5f;
	const float HalfThickness = Thickness * 0.5f;

	for (int32 ArcIdx = 0; ArcIdx < NumArcPoints; ++ArcIdx)
	{
		const float ArcPercent = static_cast<float>(ArcIdx) / static_cast<float>(NumArcPoints - 1);
		const float CurrentAngle = ArcPercent * AbsAngle;
		const float CurrentAngleDeg = FVector::GetRadianToDegree(CurrentAngle);

		const FQuaternion ArcRotQuat = FQuaternion::FromAxisAngle(ZAxis, RotationDirection * FVector::GetDegreeToRadian(CurrentAngleDeg));
		FVector ArcDir = ArcRotQuat.RotateVector(StartAxis);
		ArcDir.Normalize();

		const FVector ArcCenter = ArcDir * MidRadius;

		const FVector& ThicknessAxis = ZAxis;
		const FVector WidthAxis = ArcDir;

		for (int32 ThickIdx = 0; ThickIdx < NumThicknessSegments; ++ThickIdx)
		{
			const float ThickAngle = (2.0f * PI * static_cast<float>(ThickIdx)) / NumThicknessSegments;
			const float CosThick = std::cosf(ThickAngle);
			const float SinThick = std::sinf(ThickAngle);

			const FVector Offset = WidthAxis * (RingWidth * CosThick) + ThicknessAxis * (HalfThickness * SinThick);
			const FVector VertexPos = ArcCenter + Offset;

			FVector Normal = Offset;
			Normal.Normalize();

			FNormalVertex Vertex;
			Vertex.Position = VertexPos;
			Vertex.Normal = Normal;
			Vertex.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
			Vertex.TexCoord = FVector2(ArcPercent, static_cast<float>(ThickIdx) / NumThicknessSegments);

			OutVertices.push_back(Vertex);
		}
	}

	for (int32 ArcIdx = 0; ArcIdx < NumArcPoints - 1; ++ArcIdx)
	{
		for (int32 ThickIdx = 0; ThickIdx < NumThicknessSegments; ++ThickIdx)
		{
			const int32 NextThickIdx = (ThickIdx + 1) % NumThicknessSegments;

			const int32 i0 = ArcIdx * NumThicknessSegments + ThickIdx;
			const int32 i1 = ArcIdx * NumThicknessSegments + NextThickIdx;
			const int32 i2 = (ArcIdx + 1) * NumThicknessSegments + ThickIdx;
			const int32 i3 = (ArcIdx + 1) * NumThicknessSegments + NextThickIdx;

			OutIndices.push_back(i0);
			OutIndices.push_back(i2);
			OutIndices.push_back(i1);

			OutIndices.push_back(i1);
			OutIndices.push_back(i2);
			OutIndices.push_back(i3);
		}
	}
}

void FGizmoGeometry::GenerateAngleTickMarks(const FVector& Axis0, const FVector& Axis1,
                                             float InnerRadius, float OuterRadius, float Thickness, float SnapAngleDegrees,
                                             TArray<FNormalVertex>& OutVertices, TArray<uint32>& OutIndices)
{
	OutVertices.clear();
	OutIndices.clear();

	FVector ZAxis = Axis0.Cross(Axis1);
	ZAxis.Normalize();

	uint32 BaseVertexIndex = 0;

	int32 AngleStep = static_cast<int32>(SnapAngleDegrees);
	if (AngleStep <= 0)
	{
		AngleStep = 10;
		UViewportManager::GetInstance().SetRotationSnapAngle(static_cast<float>(AngleStep));
	}

	for (int32 Degree = 0; Degree < 360; Degree += AngleStep)
	{
		const float AngleRad = FVector::GetDegreeToRadian(static_cast<float>(Degree));

		const bool bIsLargeTick = (Degree % 90 == 0);
		const float TickStartRadius = bIsLargeTick ? OuterRadius * 1.00f : OuterRadius * 0.95f;
		const float TickEndRadius = bIsLargeTick ? InnerRadius * 1.00f : InnerRadius * 1.05f;
		const float TickThickness = Thickness * (bIsLargeTick ? 0.8f : 0.5f);

		const FQuaternion RotQuat = FQuaternion::FromAxisAngle(ZAxis, -AngleRad);
		FVector TickDir = RotQuat.RotateVector(Axis0);
		TickDir.Normalize();

		const FVector TickStart = TickDir * TickStartRadius;
		const FVector TickEnd = TickDir * TickEndRadius;

		const FVector ThicknessOffset = ZAxis * (TickThickness * 0.5f);

		FNormalVertex Vtx0, Vtx1, Vtx2, Vtx3;
		Vtx0.Position = TickStart + ThicknessOffset;
		Vtx1.Position = TickStart - ThicknessOffset;
		Vtx2.Position = TickEnd + ThicknessOffset;
		Vtx3.Position = TickEnd - ThicknessOffset;

		FVector Normal = TickDir;
		Vtx0.Normal = Vtx1.Normal = Vtx2.Normal = Vtx3.Normal = Normal;

		constexpr FVector4 TickColor(1.0f, 1.0f, 0.0f, 1.0f);
		Vtx0.Color = Vtx1.Color = Vtx2.Color = Vtx3.Color = TickColor;

		OutVertices.push_back(Vtx0);
		OutVertices.push_back(Vtx1);
		OutVertices.push_back(Vtx2);
		OutVertices.push_back(Vtx3);

		OutIndices.push_back(BaseVertexIndex + 0);
		OutIndices.push_back(BaseVertexIndex + 2);
		OutIndices.push_back(BaseVertexIndex + 1);

		OutIndices.push_back(BaseVertexIndex + 1);
		OutIndices.push_back(BaseVertexIndex + 2);
		OutIndices.push_back(BaseVertexIndex + 3);

		BaseVertexIndex += 4;
	}
}

void FGizmoGeometry::GenerateQuarterRingMesh(const FVector& Axis0, const FVector& Axis1,
                                              float InnerRadius, float OuterRadius, float Thickness,
                                              TArray<FNormalVertex>& OutVertices, TArray<uint32>& OutIndices)
{
	OutVertices.clear();
	OutIndices.clear();

	constexpr int32 NumArcPoints = static_cast<int32>(FGizmoConstants::QuarterRingSegments * (FGizmoConstants::QuarterRingEndAngle - FGizmoConstants::QuarterRingStartAngle) / (PI / 2.0f)) + 1;
	constexpr int32 NumThicknessSegments = 6;

	FVector ZAxis = Axis0.Cross(Axis1);
	ZAxis.Normalize();

	const float MidRadius = (InnerRadius + OuterRadius) * 0.5f;
	const float RingWidth = (OuterRadius - InnerRadius) * 0.5f;
	const float HalfThickness = Thickness * 0.5f;

	for (int32 ArcIdx = 0; ArcIdx < NumArcPoints; ++ArcIdx)
	{
		const float ArcPercent = static_cast<float>(ArcIdx) / static_cast<float>(NumArcPoints - 1);
		const float ArcAngle = FGizmoConstants::QuarterRingStartAngle + ArcPercent * (FGizmoConstants::QuarterRingEndAngle - FGizmoConstants::QuarterRingStartAngle);
		const float ArcAngleDeg = FVector::GetRadianToDegree(ArcAngle);

		const FQuaternion ArcRotQuat = FQuaternion::FromAxisAngle(ZAxis, -FVector::GetDegreeToRadian(ArcAngleDeg));
		FVector ArcDir = ArcRotQuat.RotateVector(Axis0);
		ArcDir.Normalize();

		const FVector ArcCenter = ArcDir * MidRadius;

		const FVector& ThicknessAxis = ZAxis;
		const FVector WidthAxis = ArcDir;

		for (int32 ThickIdx = 0; ThickIdx < NumThicknessSegments; ++ThickIdx)
		{
			const float ThickAngle = (2.0f * PI *  static_cast<float>(ThickIdx)) / NumThicknessSegments;
			const float CosThick = std::cosf(ThickAngle);
			const float SinThick = std::sinf(ThickAngle);

			const FVector Offset = WidthAxis * (RingWidth * CosThick) + ThicknessAxis * (HalfThickness * SinThick);
			const FVector VertexPos = ArcCenter + Offset;

			FVector Normal = Offset;
			Normal.Normalize();

			FNormalVertex Vertex;
			Vertex.Position = VertexPos;
			Vertex.Normal = Normal;
			Vertex.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
			Vertex.TexCoord = FVector2(ArcPercent, static_cast<float>(ThickIdx) / NumThicknessSegments);

			OutVertices.push_back(Vertex);
		}
	}

	for (int32 ArcIdx = 0; ArcIdx < NumArcPoints - 1; ++ArcIdx)
	{
		for (int32 ThickIdx = 0; ThickIdx < NumThicknessSegments; ++ThickIdx)
		{
			const int32 NextThickIdx = (ThickIdx + 1) % NumThicknessSegments;

			const int32 Idx0 = ArcIdx * NumThicknessSegments + ThickIdx;
			const int32 Idx1 = ArcIdx * NumThicknessSegments + NextThickIdx;
			const int32 Idx2 = (ArcIdx + 1) * NumThicknessSegments + ThickIdx;
			const int32 Idx3 = (ArcIdx + 1) * NumThicknessSegments + NextThickIdx;

			OutIndices.push_back(Idx0);
			OutIndices.push_back(Idx2);
			OutIndices.push_back(Idx1);

			OutIndices.push_back(Idx1);
			OutIndices.push_back(Idx2);
			OutIndices.push_back(Idx3);
		}
	}
}
