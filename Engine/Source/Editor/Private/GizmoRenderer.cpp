#include "pch.h"
#include "Editor/Public/Gizmo.h"
#include "Editor/Public/GizmoGeometry.h"
#include "Editor/Public/GizmoMath.h"

#include "Editor/Public/Editor.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Manager/UI/Public/ViewportManager.h"
#include "Render/Renderer/Public/Renderer.h"

void FGizmoBatchRenderer::AddMesh(const TArray<FNormalVertex>& InVertices, const TArray<uint32>& InIndices,
                                   const FVector4& InColor, const FVector& InLocation,
                                   const FQuaternion& InRotation, const FVector& InScale)
{
	FBatchedMesh Mesh;
	Mesh.Vertices = InVertices;
	Mesh.Indices = InIndices;
	Mesh.Color = InColor;
	Mesh.Location = InLocation;
	Mesh.Rotation = InRotation;
	Mesh.Scale = InScale;
	Meshes.Add(Mesh);
}

void FGizmoBatchRenderer::FlushAndRender(const FRenderState& InRenderState)
{
	if (Meshes.IsEmpty())
	{
		return;
	}

	URenderer& Renderer = URenderer::GetInstance();

	for (const FBatchedMesh& Mesh : Meshes)
	{
		if (Mesh.Vertices.IsEmpty() || Mesh.Indices.IsEmpty())
		{
			continue;
		}

		// 임시 버퍼 생성
		ID3D11Buffer* TempVB = nullptr;
		ID3D11Buffer* TempIB = nullptr;

		D3D11_BUFFER_DESC VBDesc = {};
		VBDesc.Usage = D3D11_USAGE_DEFAULT;
		VBDesc.ByteWidth = static_cast<UINT>(sizeof(FNormalVertex) * Mesh.Vertices.Num());
		VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		D3D11_SUBRESOURCE_DATA VBData = {};
		VBData.pSysMem = Mesh.Vertices.GetData();
		Renderer.GetDevice()->CreateBuffer(&VBDesc, &VBData, &TempVB);

		D3D11_BUFFER_DESC IBDesc = {};
		IBDesc.Usage = D3D11_USAGE_DEFAULT;
		IBDesc.ByteWidth = static_cast<UINT>(sizeof(uint32) * Mesh.Indices.Num());
		IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		D3D11_SUBRESOURCE_DATA IBData = {};
		IBData.pSysMem = Mesh.Indices.GetData();
		Renderer.GetDevice()->CreateBuffer(&IBDesc, &IBData, &TempIB);

		// Primitive 설정 및 렌더링
		FEditorPrimitive PrimitiveInfo;
		PrimitiveInfo.Location = Mesh.Location;
		PrimitiveInfo.Rotation = Mesh.Rotation;
		PrimitiveInfo.Scale = Mesh.Scale;
		PrimitiveInfo.VertexBuffer = TempVB;
		PrimitiveInfo.NumVertices = static_cast<uint32>(Mesh.Vertices.Num());
		PrimitiveInfo.IndexBuffer = TempIB;
		PrimitiveInfo.NumIndices = static_cast<uint32>(Mesh.Indices.Num());
		PrimitiveInfo.Color = Mesh.Color;
		PrimitiveInfo.bShouldAlwaysVisible = Mesh.bAlwaysVisible;
		PrimitiveInfo.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		Renderer.RenderEditorPrimitive(PrimitiveInfo, InRenderState);

		// 버퍼 해제
		TempVB->Release();
		TempIB->Release();
	}

	Clear();
}

void FGizmoBatchRenderer::Clear()
{
	Meshes.Empty();
}

void UGizmo::RenderGizmo(UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
	TargetComponent = Cast<USceneComponent>(GEditor->GetEditorModule()->GetSelectedComponent());
	if (!TargetComponent || !InCamera)
	{
		return;
	}

	// 스크린에서 균일한 사이즈를 가지도록 하기 위한 스케일 조정
	FVector GizmoLocation;
	if (bUseFixedLocation)
	{
		// Pilot Mode: 고정 위치 사용
		GizmoLocation = FixedLocation;
	}
	else
	{
		GizmoLocation = TargetComponent->GetWorldLocation();
	}

	const float RenderScale = FGizmoMath::CalculateScreenSpaceScale(InCamera, InViewport, GizmoLocation, 120.0f);

	URenderer& Renderer = URenderer::GetInstance();
	const int Mode = static_cast<int>(GizmoMode);
	auto& P = Primitives[Mode];
	P.Location = GizmoLocation;

	P.Scale = FVector(RenderScale, RenderScale, RenderScale);

	// 기즈모 기준 회전 결정 (World/Local 모드)
	FQuaternion BaseRot;
	if (GizmoMode == EGizmoMode::Scale)
	{
		// 스케일 모드: 항상 로컬 좌표
		BaseRot = TargetComponent->GetWorldRotationAsQuaternion();
	}
	else if (GizmoMode == EGizmoMode::Rotate && IsDragging())
	{
		// 회전 모드 드래그 중: 드래그 시작 시 회전으로 고정 (기즈모가 움직이지 않도록)
		BaseRot = bIsWorld ? FQuaternion::Identity() : GetDragStartActorRotationQuat();
	}
	else
	{
		// 평행이동/회전 모드: World는 Identity, Local은 현재 회전
		BaseRot = bIsWorld ? FQuaternion::Identity() : TargetComponent->GetWorldRotationAsQuaternion();
	}

	// 회전 모드 확인
	UAssetManager& AssetManager = UAssetManager::GetInstance();
	bool bIsRotateMode = (GizmoMode == EGizmoMode::Rotate);

	// 각 링을 해당 평면으로 회전시키는 변환 (드래그 시 Ring 사용)
	FQuaternion AxisRots[3] = {
		FQuaternion::Identity(),  // X링: YZ 평면
		FQuaternion::FromAxisAngle(FVector::UpVector(), FVector::GetDegreeToRadian(90.0f)),  // Y링: XZ 평면
		FQuaternion::FromAxisAngle(FVector::RightVector(), FVector::GetDegreeToRadian(-90.0f))  // Z링: XY 평면
	};

	// 각 축의 BaseAxis 정의
	const FVector BaseAxis0[3] = {
		FVector(0, 0, 1),  // X축: Z
		FVector(1, 0, 0),  // Y축: X
		FVector(1, 0, 0)   // Z축: X
	};
	const FVector BaseAxis1[3] = {
		FVector(0, 1, 0),  // X축: Y
		FVector(0, 0, 1),  // Y축: Z
		FVector(0, 1, 0)   // Z축: Y
	};

	// 오쏘 뷰에서 World 모드일 때 어떤 축을 표시할지 결정
	const bool bIsOrtho = (InCamera->GetCameraType() == ECameraType::ECT_Orthographic);
	EGizmoDirection OrthoWorldAxis = EGizmoDirection::None;

	if (bIsOrtho && bIsWorld && bIsRotateMode)
	{
		// 카메라 Forward 방향으로 어떤 평면을 보고 있는지 판단
		const FVector CamForward = InCamera->GetForward();
		const float AbsX = std::abs(CamForward.X);
		const float AbsY = std::abs(CamForward.Y);
		const float AbsZ = std::abs(CamForward.Z);

		// 가장 지배적인 축 방향 찾기
		if (AbsZ > AbsX && AbsZ > AbsY)
		{
			// Top/Bottom 뷰 (XY 평면) -> Z축 회전
			OrthoWorldAxis = EGizmoDirection::Up;
		}
		else if (AbsY > AbsX && AbsY > AbsZ)
		{
			// Left/Right 뷰 (XZ 평면) -> Y축 회전
			OrthoWorldAxis = EGizmoDirection::Right;
		}
		else
		{
			// Front/Back 뷰 (YZ 평면) -> X축 회전
			OrthoWorldAxis = EGizmoDirection::Forward;
		}
	}

	// Rotate 모드에서 드래그 중이면 해당 축만 표시
	// 오쏘 뷰 + World 모드면 단일 축만 표시
	bool bShowX = !bIsDragging || !bIsRotateMode || GizmoDirection == EGizmoDirection::Forward;
	bool bShowY = !bIsDragging || !bIsRotateMode || GizmoDirection == EGizmoDirection::Right;
	bool bShowZ = !bIsDragging || !bIsRotateMode || GizmoDirection == EGizmoDirection::Up;

	if (bIsOrtho && bIsWorld && bIsRotateMode && !bIsDragging)
	{
		bShowX = (OrthoWorldAxis == EGizmoDirection::Forward);
		bShowY = (OrthoWorldAxis == EGizmoDirection::Right);
		bShowZ = (OrthoWorldAxis == EGizmoDirection::Up);
	}

	// 평면 기즈모가 하이라이팅되지 않은 경우 먼저 렌더링 (축이 위에 그려지도록)
	bool bAnyPlaneHighlighted = (GizmoDirection == EGizmoDirection::XY_Plane ||
	                              GizmoDirection == EGizmoDirection::XZ_Plane ||
	                              GizmoDirection == EGizmoDirection::YZ_Plane);

	if (!bAnyPlaneHighlighted)
	{
		if (GizmoMode == EGizmoMode::Translate)
		{
			RenderTranslatePlanes(P, BaseRot, RenderScale);
		}
		else if (GizmoMode == EGizmoMode::Scale)
		{
			RenderScalePlanes(P, BaseRot, RenderScale);
		}
	}

	// X축 (Forward) - 빨간색
	if (bShowX)
	{
		if (bIsRotateMode && bIsDragging && GizmoDirection == EGizmoDirection::Forward)
		{
			// BaseAxis가 이미 평면을 정의하므로 AxisRotation 불필요 (Identity 사용)
			RenderRotationCircles(P, FQuaternion::Identity(), BaseRot, GizmoColor[0], BaseAxis0[0], BaseAxis1[0], InCamera);
		}
		else if (bIsRotateMode)
		{
			RenderRotationQuarterRing(P, BaseRot, EGizmoDirection::Forward, InCamera, BaseAxis0[0], BaseAxis1[0]);
		}
		else
		{
			// Translate/Scale 모드
			P.Rotation = BaseRot * AxisRots[0];
			P.Color = ColorFor(EGizmoDirection::Forward);
			Renderer.RenderEditorPrimitive(P, RenderState);
		}
	}

	// Y축 (Right) - 초록색
	if (bShowY)
	{
		if (bIsRotateMode && bIsDragging && GizmoDirection == EGizmoDirection::Right)
		{
			// BaseAxis가 이미 평면을 정의하므로 AxisRotation 불필요 (Identity 사용)
			RenderRotationCircles(P, FQuaternion::Identity(), BaseRot, GizmoColor[1], BaseAxis0[1], BaseAxis1[1], InCamera);
		}
		else if (bIsRotateMode)
		{
			RenderRotationQuarterRing(P, BaseRot, EGizmoDirection::Right, InCamera, BaseAxis0[1], BaseAxis1[1]);
		}
		else
		{
			// Translate/Scale 모드
			P.Rotation = BaseRot * AxisRots[1];
			P.Color = ColorFor(EGizmoDirection::Right);
			Renderer.RenderEditorPrimitive(P, RenderState);
		}
	}

	// Z축 (Up) - 파란색
	if (bShowZ)
	{
		if (bIsRotateMode && bIsDragging && GizmoDirection == EGizmoDirection::Up)
		{
			// BaseAxis가 이미 평면을 정의하므로 AxisRotation 불필요 (Identity 사용)
			RenderRotationCircles(P, FQuaternion::Identity(), BaseRot, GizmoColor[2], BaseAxis0[2], BaseAxis1[2], InCamera);
		}
		else if (bIsRotateMode)
		{
			RenderRotationQuarterRing(P, BaseRot, EGizmoDirection::Up, InCamera, BaseAxis0[2], BaseAxis1[2]);
		}
		else
		{
			// Translate/Scale 모드
			P.Rotation = BaseRot * AxisRots[2];
			P.Color = ColorFor(EGizmoDirection::Up);
			Renderer.RenderEditorPrimitive(P, RenderState);
		}
	}

	// 중심 구체 렌더링
	if (GizmoMode == EGizmoMode::Translate || GizmoMode == EGizmoMode::Scale)
	{
		RenderCenterSphere(P, RenderScale);
	}

	// 평면 기즈모가 하이라이팅된 경우 마지막에 렌더링 (축 위에 보이도록)
	if (bAnyPlaneHighlighted)
	{
		if (GizmoMode == EGizmoMode::Translate)
		{
			RenderTranslatePlanes(P, BaseRot, RenderScale);
		}
		else if (GizmoMode == EGizmoMode::Scale)
		{
			RenderScalePlanes(P, BaseRot, RenderScale);
		}
	}
}

/**
 * @brief Translation 및 Scale 모드의 중심 구체 렌더링
 * @param P 기즈모 Primitive (Location 정보 사용)
 * @param RenderScale 스크린 공간 스케일
 */
void UGizmo::RenderCenterSphere(const FEditorPrimitive& P, float RenderScale)
{
	const float SphereRadius = 0.10f * RenderScale;
	constexpr int NumSegments = 16;
	constexpr int NumRings = 8;

	TArray<FNormalVertex> vertices;
	TArray<uint32> Indices;

	// UV Sphere 생성
	for (int ring = 0; ring <= NumRings; ++ring)
	{
		float phi = static_cast<float>(ring) / NumRings * PI;
		float sinPhi = std::sin(phi);
		float cosPhi = std::cos(phi);

		for (int seg = 0; seg <= NumSegments; ++seg)
		{
			float theta = static_cast<float>(seg) / NumSegments * 2.0f * PI;
			float sinTheta = std::sin(theta);
			float cosTheta = std::cos(theta);

			FVector pos(sinPhi * cosTheta, sinPhi * sinTheta, cosPhi);
			FVector normal = pos.GetNormalized();
			pos = pos * SphereRadius;

			vertices.Add({pos, normal});
		}
	}

	// 인덱스 생성
	for (int ring = 0; ring < NumRings; ++ring)
	{
		for (int seg = 0; seg < NumSegments; ++seg)
		{
			int current = ring * (NumSegments + 1) + seg;
			int next = current + NumSegments + 1;

			Indices.Add(current);
			Indices.Add(next);
			Indices.Add(current + 1);

			Indices.Add(current + 1);
			Indices.Add(next);
			Indices.Add(next + 1);
		}
	}

	// 중심 구체 색상: 흰색 또는 하이라이트
	bool bIsCenterSelected = (GizmoDirection == EGizmoDirection::Center);
	FVector4 SphereColor;
	if (bIsCenterSelected && bIsDragging)
	{
		SphereColor = FVector4(0.8f, 0.8f, 0.0f, 1.0f);  // 드래그 중: 짙은 노란색
	}
	else if (bIsCenterSelected)
	{
		SphereColor = FVector4(1.0f, 1.0f, 0.0f, 1.0f);  // 호버 중: 밝은 노란색
	}
	else
	{
		SphereColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);  // 기본: 흰색
	}

	// 배칭에 추가 및 즉시 렌더링
	FGizmoBatchRenderer Batch;
	Batch.AddMesh(vertices, Indices, SphereColor, P.Location);
	Batch.FlushAndRender(RenderState);
}

/**
 * @brief Translation 모드의 평면 기즈모 렌더링 (직각 표시)
 * @param P 기즈모 Primitive (Location 정보 사용)
 * @param BaseRot 기즈모 회전 (World/Local 모드)
 * @param RenderScale 스크린 공간 스케일
 */
void UGizmo::RenderTranslatePlanes(const FEditorPrimitive& P, const FQuaternion& BaseRot, float RenderScale)
{
	const float CornerPos = 0.3f * RenderScale;
	const float HandleRadius = 0.02f * RenderScale;
	const int NumSegments = 8;

	struct FPlaneInfo
	{
		EGizmoDirection Direction;
		FVector Tangent1;
		FVector Tangent2;
	};

	FPlaneInfo Planes[3] = {
		{EGizmoDirection::XY_Plane, {1, 0, 0}, {0, 1, 0}},
		{EGizmoDirection::XZ_Plane, {1, 0, 0}, {0, 0, 1}},
		{EGizmoDirection::YZ_Plane, {0, 1, 0}, {0, 0, 1}}
	};

	FGizmoBatchRenderer Batch;

	for (const FPlaneInfo& PlaneInfo : Planes)
	{
		FVector T1 = PlaneInfo.Tangent1;
		FVector T2 = PlaneInfo.Tangent2;
		FVector PlaneNormal = Cross(T1, T2).GetNormalized();

		EGizmoDirection Seg1Color, Seg2Color;
		if (PlaneInfo.Direction == EGizmoDirection::XY_Plane)
		{
			Seg1Color = EGizmoDirection::Forward;
			Seg2Color = EGizmoDirection::Right;
		}
		else if (PlaneInfo.Direction == EGizmoDirection::XZ_Plane)
		{
			Seg1Color = EGizmoDirection::Forward;
			Seg2Color = EGizmoDirection::Up;
		}
		else
		{
			Seg1Color = EGizmoDirection::Right;
			Seg2Color = EGizmoDirection::Up;
		}

		bool bIsPlaneSelected = (GizmoDirection == PlaneInfo.Direction);

		// 색상 계산
		FVector4 Seg1Color_Final, Seg2Color_Final;
		if (bIsPlaneSelected && bIsDragging)
		{
			Seg1Color_Final = Seg2Color_Final = FVector4(0.8f, 0.8f, 0.0f, 1.0f);
		}
		else if (bIsPlaneSelected)
		{
			Seg1Color_Final = Seg2Color_Final = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
		}
		else
		{
			Seg1Color_Final = GizmoColor[FGizmoMath::DirectionToAxisIndex(Seg1Color)];
			Seg2Color_Final = GizmoColor[FGizmoMath::DirectionToAxisIndex(Seg2Color)];
		}

		// 선분 1 메쉬 생성
		{
			TArray<FNormalVertex> vertices;
			TArray<uint32> Indices;

			FVector Start1 = T1 * CornerPos;
			FVector End1 = T1 * CornerPos + T2 * CornerPos;
			FVector Dir1 = (End1 - Start1).GetNormalized();
			FVector Perp1_1 = Cross(Dir1, PlaneNormal).GetNormalized();
			FVector Perp1_2 = Cross(Dir1, Perp1_1).GetNormalized();

			for (int i = 0; i < NumSegments; ++i)
			{
				float Angle = static_cast<float>(i) / NumSegments * 2.0f * PI;
				FVector Offset = (Perp1_1 * std::cos(Angle) + Perp1_2 * std::sin(Angle)) * HandleRadius;

				vertices.Add({Start1 + Offset, Offset.GetNormalized()});
				vertices.Add({End1 + Offset, Offset.GetNormalized()});
			}

			for (int i = 0; i < NumSegments; ++i)
			{
				int Next = (i + 1) % NumSegments;
				Indices.Add(i * 2 + 0);
				Indices.Add(i * 2 + 1);
				Indices.Add(Next * 2 + 0);
				Indices.Add(Next * 2 + 0);
				Indices.Add(i * 2 + 1);
				Indices.Add(Next * 2 + 1);
			}

			Batch.AddMesh(vertices, Indices, Seg1Color_Final, P.Location, BaseRot);
		}

		// 선분 2 메쉬 생성
		{
			TArray<FNormalVertex> vertices;
			TArray<uint32> Indices;

			FVector Start2 = T2 * CornerPos;
			FVector End2 = T1 * CornerPos + T2 * CornerPos;
			FVector Dir2 = (End2 - Start2).GetNormalized();
			FVector Perp2_1 = Cross(Dir2, PlaneNormal).GetNormalized();
			FVector Perp2_2 = Cross(Dir2, Perp2_1).GetNormalized();

			for (int i = 0; i < NumSegments; ++i)
			{
				float Angle = static_cast<float>(i) / NumSegments * 2.0f * PI;
				FVector Offset = (Perp2_1 * std::cos(Angle) + Perp2_2 * std::sin(Angle)) * HandleRadius;

				vertices.Add({Start2 + Offset, Offset.GetNormalized()});
				vertices.Add({End2 + Offset, Offset.GetNormalized()});
			}

			for (int i = 0; i < NumSegments; ++i)
			{
				int Next = (i + 1) % NumSegments;
				Indices.Add(i * 2 + 0);
				Indices.Add(i * 2 + 1);
				Indices.Add(Next * 2 + 0);
				Indices.Add(Next * 2 + 0);
				Indices.Add(i * 2 + 1);
				Indices.Add(Next * 2 + 1);
			}

			Batch.AddMesh(vertices, Indices, Seg2Color_Final, P.Location, BaseRot);
		}
	}

	// 모든 메쉬를 일괄 렌더링
	Batch.FlushAndRender(RenderState);
}

/**
 * @brief Scale 모드의 평면 기즈모 렌더링 (대각선 형태)
 * @param P 기즈모 Primitive (Location 정보 사용)
 * @param BaseRot 기즈모 회전 (항상 로컬 좌표)
 * @param RenderScale 스크린 공간 스케일
 */
void UGizmo::RenderScalePlanes(const FEditorPrimitive& P, const FQuaternion& BaseRot, float RenderScale)
{
	const float MidPoint = 0.5f * RenderScale;
	const float HandleRadius = 0.02f * RenderScale;
	constexpr int NumSegments = 8;

	struct FPlaneInfo
	{
		EGizmoDirection Direction;
		FVector Tangent1;
		FVector Tangent2;
	};

	FPlaneInfo Planes[3] = {
		{EGizmoDirection::XY_Plane, {1, 0, 0}, {0, 1, 0}},
		{EGizmoDirection::XZ_Plane, {1, 0, 0}, {0, 0, 1}},
		{EGizmoDirection::YZ_Plane, {0, 1, 0}, {0, 0, 1}}
	};

	FGizmoBatchRenderer Batch;

	for (const FPlaneInfo& PlaneInfo : Planes)
	{
		// 로컬 공간에서 메시 생성
		FVector T1 = PlaneInfo.Tangent1;
		FVector T2 = PlaneInfo.Tangent2;

		FVector Point1 = T1 * MidPoint;
		FVector Point2 = T2 * MidPoint;
		FVector MidCenter = (Point1 + Point2) * 0.5f;

		FVector PlaneNormal = Cross(T1, T2).GetNormalized();

		EGizmoDirection Seg1Color, Seg2Color;
		if (PlaneInfo.Direction == EGizmoDirection::XY_Plane)
		{
			Seg1Color = EGizmoDirection::Forward;
			Seg2Color = EGizmoDirection::Right;
		}
		else if (PlaneInfo.Direction == EGizmoDirection::XZ_Plane)
		{
			Seg1Color = EGizmoDirection::Forward;
			Seg2Color = EGizmoDirection::Up;
		}
		else
		{
			Seg1Color = EGizmoDirection::Right;
			Seg2Color = EGizmoDirection::Up;
		}

		bool bIsPlaneSelected = (GizmoDirection == PlaneInfo.Direction);
		bool bIsCenterSelected = (GizmoDirection == EGizmoDirection::Center);

		// 색상 계산 (평면 자체 선택 또는 Center 선택 시 하이라이팅)
		FVector4 Seg1Color_Final, Seg2Color_Final;
		if ((bIsPlaneSelected || bIsCenterSelected) && bIsDragging)
		{
			Seg1Color_Final = Seg2Color_Final = FVector4(0.8f, 0.8f, 0.0f, 1.0f);
		}
		else if (bIsPlaneSelected || bIsCenterSelected)
		{
			Seg1Color_Final = Seg2Color_Final = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
		}
		else
		{
			Seg1Color_Final = GizmoColor[FGizmoMath::DirectionToAxisIndex(Seg1Color)];
			Seg2Color_Final = GizmoColor[FGizmoMath::DirectionToAxisIndex(Seg2Color)];
		}

		// 선분 1 메쉬 생성
		{
			TArray<FNormalVertex> vertices;
			TArray<uint32> Indices;

			FVector Start = Point1;
			FVector End = MidCenter;
			FVector DiagDir = (End - Start).GetNormalized();
			FVector Perp1 = Cross(DiagDir, PlaneNormal).GetNormalized();
			FVector Perp2 = Cross(DiagDir, Perp1).GetNormalized();

			for (int i = 0; i < NumSegments; ++i)
			{
				float Angle = static_cast<float>(i) / NumSegments * 2.0f * PI;
				FVector Offset = (Perp1 * std::cos(Angle) + Perp2 * std::sin(Angle)) * HandleRadius;

				vertices.Add({Start + Offset, Offset.GetNormalized()});
				vertices.Add({End + Offset, Offset.GetNormalized()});
			}

			for (int i = 0; i < NumSegments; ++i)
			{
				int Next = (i + 1) % NumSegments;
				Indices.Add(i * 2 + 0);
				Indices.Add(i * 2 + 1);
				Indices.Add(Next * 2 + 0);
				Indices.Add(Next * 2 + 0);
				Indices.Add(i * 2 + 1);
				Indices.Add(Next * 2 + 1);
			}

			Batch.AddMesh(vertices, Indices, Seg1Color_Final, P.Location, BaseRot);
		}

		// 선분 2 메쉬 생성
		{
			TArray<FNormalVertex> vertices;
			TArray<uint32> Indices;

			FVector Start = MidCenter;
			FVector End = Point2;
			FVector DiagDir = (End - Start).GetNormalized();
			FVector Perp1 = Cross(DiagDir, PlaneNormal).GetNormalized();
			FVector Perp2 = Cross(DiagDir, Perp1).GetNormalized();

			for (int i = 0; i < NumSegments; ++i)
			{
				float Angle = static_cast<float>(i) / NumSegments * 2.0f * PI;
				FVector Offset = (Perp1 * std::cos(Angle) + Perp2 * std::sin(Angle)) * HandleRadius;

				vertices.Add({Start + Offset, Offset.GetNormalized()});
				vertices.Add({End + Offset, Offset.GetNormalized()});
			}

			for (int i = 0; i < NumSegments; ++i)
			{
				int Next = (i + 1) % NumSegments;
				Indices.Add(i * 2 + 0);
				Indices.Add(i * 2 + 1);
				Indices.Add(Next * 2 + 0);
				Indices.Add(Next * 2 + 0);
				Indices.Add(i * 2 + 1);
				Indices.Add(Next * 2 + 1);
			}

			Batch.AddMesh(vertices, Indices, Seg2Color_Final, P.Location, BaseRot);
		}
	}

	// 모든 메쉬를 일괄 렌더링
	Batch.FlushAndRender(RenderState);
}

void UGizmo::RenderRotationCircles(const FEditorPrimitive& P, const FQuaternion& AxisRotation,
	const FQuaternion& BaseRot, const FVector4& AxisColor, const FVector& BaseAxis0, const FVector& BaseAxis1, UCamera* InCamera)
{
	URenderer& Renderer = URenderer::GetInstance();

	FVector RenderWorldAxis0 = BaseRot.RotateVector(BaseAxis0);
	FVector RenderWorldAxis1 = BaseRot.RotateVector(BaseAxis1);

	// Inner circle: 두꺼운 축 색상 선
	TArray<FNormalVertex> innerVertices, outerVertices;
	TArray<uint32> innerIndices, outerIndices;

	const float InnerLineThickness = RotateCollisionConfig.Thickness * 2.0f;
	FGizmoGeometry::GenerateCircleLineMesh(RenderWorldAxis0, RenderWorldAxis1,
		RotateCollisionConfig.InnerRadius, InnerLineThickness, innerVertices, innerIndices);

	if (!innerIndices.IsEmpty())
	{
		ID3D11Buffer* innerVB = nullptr;
		ID3D11Buffer* innerIB = nullptr;
		FGizmoGeometry::CreateTempBuffers(innerVertices, innerIndices, &innerVB, &innerIB);

		FEditorPrimitive InnerPrim = P;
		InnerPrim.VertexBuffer = innerVB;
		InnerPrim.NumVertices = static_cast<uint32>(innerVertices.Num());
		InnerPrim.IndexBuffer = innerIB;
		InnerPrim.NumIndices = static_cast<uint32>(innerIndices.Num());
		InnerPrim.Rotation = FQuaternion::Identity(); // 이미 월드 공간
		InnerPrim.Color = AxisColor;
		Renderer.RenderEditorPrimitive(InnerPrim, RenderState);

		innerVB->Release();
		innerIB->Release();
	}

	// Outer circle: 얇은 노란색 선
	const float OuterLineThickness = RotateCollisionConfig.Thickness * 1.0f;
	FGizmoGeometry::GenerateCircleLineMesh(RenderWorldAxis0, RenderWorldAxis1,
		RotateCollisionConfig.OuterRadius, OuterLineThickness, outerVertices, outerIndices);

	if (!outerIndices.IsEmpty())
	{
		ID3D11Buffer* outerVB = nullptr;
		ID3D11Buffer* outerIB = nullptr;
		FGizmoGeometry::CreateTempBuffers(outerVertices, outerIndices, &outerVB, &outerIB);

		FEditorPrimitive OuterPrim = P;
		OuterPrim.VertexBuffer = outerVB;
		OuterPrim.NumVertices = static_cast<uint32>(outerVertices.Num());
		OuterPrim.IndexBuffer = outerIB;
		OuterPrim.NumIndices = static_cast<uint32>(outerIndices.Num());
		OuterPrim.Rotation = FQuaternion::Identity(); // 이미 월드 공간
		OuterPrim.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f); // 노란색
		Renderer.RenderEditorPrimitive(OuterPrim, RenderState);

		outerVB->Release();
		outerIB->Release();
	}

	// 각도 눈금 렌더링 (스냅 각도마다 작은 눈금, 90도마다 큰 눈금)
	{
		TArray<FNormalVertex> tickVertices;
		TArray<uint32> tickIndices;

		const float SnapAngle = UViewportManager::GetInstance().GetRotationSnapAngle();
		FGizmoGeometry::GenerateAngleTickMarks(RenderWorldAxis0, RenderWorldAxis1,
			RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius,
			RotateCollisionConfig.Thickness, SnapAngle, tickVertices, tickIndices);

		if (!tickIndices.IsEmpty())
		{
			ID3D11Buffer* tickVB = nullptr;
			ID3D11Buffer* tickIB = nullptr;
			FGizmoGeometry::CreateTempBuffers(tickVertices, tickIndices, &tickVB, &tickIB);

			FEditorPrimitive TickPrim = P;
			TickPrim.VertexBuffer = tickVB;
			TickPrim.NumVertices = static_cast<uint32>(tickVertices.Num());
			TickPrim.IndexBuffer = tickIB;
			TickPrim.NumIndices = static_cast<uint32>(tickIndices.Num());
			TickPrim.Rotation = FQuaternion::Identity(); // 이미 월드 공간
			TickPrim.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
			Renderer.RenderEditorPrimitive(TickPrim, RenderState);

			tickVB->Release();
			tickIB->Release();
		}
	}

	// 회전 각도 Arc 렌더링 (Inner/Outer Circle과 동일 평면)
	float DisplayAngle = CurrentRotationAngle;
	if (UViewportManager::GetInstance().IsRotationSnapEnabled())
	{
		const float SnapAngle = UViewportManager::GetInstance().GetRotationSnapAngle();
		DisplayAngle = GetSnappedRotationAngle(SnapAngle);
	}

	// RotateVector 각도 보정
	const bool bIsXAxis = (BaseAxis0.X < 0.1f && BaseAxis0.Y < 0.1f && BaseAxis0.Z > 0.9f &&
	                       BaseAxis1.X < 0.1f && BaseAxis1.Y > 0.9f && BaseAxis1.Z < 0.1f);
	const bool bIsYAxis = (BaseAxis0.X > 0.9f && BaseAxis0.Y < 0.1f && BaseAxis0.Z < 0.1f &&
	                       BaseAxis1.X < 0.1f && BaseAxis1.Y < 0.1f && BaseAxis1.Z > 0.9f);
	if (bIsXAxis || bIsYAxis)
	{
		DisplayAngle = -DisplayAngle;
	}

	if (std::abs(DisplayAngle) > 0.001f)
	{
		TArray<FNormalVertex> arcVertices;
		TArray<uint32> arcIndices;

		// Arc를 약간 더 두껍게 (링보다 살짝 크게)
		const float ArcInnerRadius = RotateCollisionConfig.InnerRadius * 0.98f;
		const float ArcOuterRadius = RotateCollisionConfig.OuterRadius * 1.02f;

		// Arc 시작 방향: World 모드는 Axis0, Local 모드는 물체의 로컬 0 지점 (회전 적용 전 BaseAxis0)
		FVector StartDir = bIsWorld ? FVector(0,0,0) : BaseRot.RotateVector(BaseAxis0);

		// Inner/Outer Circle과 동일 평면: RenderWorldAxis 사용 (이미 회전된 월드 공간)
		FGizmoGeometry::GenerateRotationArcMesh(RenderWorldAxis0, RenderWorldAxis1,
			ArcInnerRadius, ArcOuterRadius, RotateCollisionConfig.Thickness,
			DisplayAngle, StartDir, arcVertices, arcIndices);

		if (!arcIndices.IsEmpty())
		{
			ID3D11Buffer* arcVB = nullptr;
			ID3D11Buffer* arcIB = nullptr;
			FGizmoGeometry::CreateTempBuffers(arcVertices, arcIndices, &arcVB, &arcIB);

			FEditorPrimitive ArcPrim = P;
			ArcPrim.VertexBuffer = arcVB;
			ArcPrim.NumVertices = static_cast<uint32>(arcVertices.Num());
			ArcPrim.IndexBuffer = arcIB;
			ArcPrim.NumIndices = static_cast<uint32>(arcIndices.Num());
			ArcPrim.Rotation = FQuaternion::Identity(); // 이미 월드 공간
			ArcPrim.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f); // 노란색
			Renderer.RenderEditorPrimitive(ArcPrim, RenderState);

			arcVB->Release();
			arcIB->Release();
		}
	}
}

void UGizmo::RenderRotationQuarterRing(const FEditorPrimitive& P, const FQuaternion& BaseRot,
	EGizmoDirection Direction, UCamera* InCamera, const FVector& BaseAxis0, const FVector& BaseAxis1)
{
	URenderer& Renderer = URenderer::GetInstance();
	UAssetManager& AssetManager = UAssetManager::GetInstance();
	const bool bIsOrtho = InCamera->GetCameraType() == ECameraType::ECT_Orthographic;

	if (bIsOrtho && bIsWorld)
	{
		// 오쏘 뷰 + World 모드: Full Ring (피킹 영역과 동일한 InnerRadius~OuterRadius)
		TArray<FNormalVertex> ringVertices;
		TArray<uint32> ringIndices;

		// 360도 Arc = Full Ring (InnerRadius~OuterRadius 사이를 채움)
		FGizmoGeometry::GenerateRotationArcMesh(BaseAxis0, BaseAxis1,
			RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius,
			RotateCollisionConfig.Thickness, 2.0f * PI, FVector(0,0,0), ringVertices, ringIndices);

		if (!ringIndices.IsEmpty())
		{
			ID3D11Buffer* ringVB = nullptr;
			ID3D11Buffer* ringIB = nullptr;
			FGizmoGeometry::CreateTempBuffers(ringVertices, ringIndices, &ringVB, &ringIB);

			FEditorPrimitive RingPrim = P;
			RingPrim.VertexBuffer = ringVB;
			RingPrim.NumVertices = static_cast<uint32>(ringVertices.Num());
			RingPrim.IndexBuffer = ringIB;
			RingPrim.NumIndices = static_cast<uint32>(ringIndices.Num());
			RingPrim.Rotation = BaseRot; // World 모드는 Identity
			RingPrim.Color = ColorFor(Direction);
			Renderer.RenderEditorPrimitive(RingPrim, RenderState);

			ringVB->Release();
			ringIB->Release();
		}
	}
	else
	{
		// 퍼스펙티브 또는 오쏘 뷰 Local 모드: QuarterRing
		const FVector GizmoLoc = P.Location;
		const FVector CameraLoc = InCamera->GetLocation();
		const FVector DirectionToWidget = (GizmoLoc - CameraLoc).GetNormalized();

		// 월드 좌표에서 축 계산
		FVector WorldAxis0 = BaseRot.RotateVector(BaseAxis0);
		FVector WorldAxis1 = BaseRot.RotateVector(BaseAxis1);

		// 플립 판정 (언리얼 표준)
		// InDirectionToWidget = 카메라 -> 위젯
		// (Axis · InDirectionToWidget) <= 0 -> 축이 카메라 반대를 향함 -> Axis 그대로
		// (Axis · InDirectionToWidget) > 0 -> 축이 카메라를 향함 -> -Axis
		const bool bMirrorAxis0 = (WorldAxis0.Dot(DirectionToWidget) <= 0.0f);
		const bool bMirrorAxis1 = (WorldAxis1.Dot(DirectionToWidget) <= 0.0f);

		// 월드 공간에서 플립
		const FVector RenderWorldAxis0 = bMirrorAxis0 ? WorldAxis0 : -WorldAxis0;
		const FVector RenderWorldAxis1 = bMirrorAxis1 ? WorldAxis1 : -WorldAxis1;

		TArray<FNormalVertex> vertices;
		TArray<uint32> Indices;

		// 월드 공간 축으로 직접 메시 생성
		FGizmoGeometry::GenerateQuarterRingMesh(RenderWorldAxis0, RenderWorldAxis1,
			RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius, RotateCollisionConfig.Thickness,
			vertices, Indices);

		ID3D11Buffer* TempVB = nullptr;
		ID3D11Buffer* TempIB = nullptr;
		FGizmoGeometry::CreateTempBuffers(vertices, Indices, &TempVB, &TempIB);

		FEditorPrimitive QuarterPrim = P;
		QuarterPrim.VertexBuffer = TempVB;
		QuarterPrim.NumVertices = static_cast<uint32>(vertices.Num());
		QuarterPrim.IndexBuffer = TempIB;
		QuarterPrim.NumIndices = static_cast<uint32>(Indices.Num());
		QuarterPrim.Rotation = FQuaternion::Identity(); // 이미 월드 공간이므로 회전 불필요
		QuarterPrim.Color = ColorFor(Direction);
		Renderer.RenderEditorPrimitive(QuarterPrim, RenderState);

		TempVB->Release();
		TempIB->Release();
	}
}

void UGizmo::RenderForHitProxy(UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
	if (!TargetComponent || !InCamera)
	{
		return;
	}

	// Scale uniformly in screen space (메인 렌더링과 동일한 Location 계산)
	FVector GizmoLocation;
	if (bUseFixedLocation)
	{
		// Pilot Mode: 고정 위치 사용
		GizmoLocation = FixedLocation;
	}
	else
	{
		GizmoLocation = TargetComponent->GetWorldLocation();
	}

	const float RenderScale = FGizmoMath::CalculateScreenSpaceScale(InCamera, InViewport, GizmoLocation, 120.0f);

	URenderer& Renderer = URenderer::GetInstance();
	FHitProxyManager& HitProxyManager = FHitProxyManager::GetInstance();

	const int Mode = static_cast<int>(GizmoMode);
	auto& P = Primitives[Mode];
	P.Location = GizmoLocation;
	P.Scale = FVector(RenderScale, RenderScale, RenderScale);

	// Determine gizmo base rotation
	FQuaternion BaseRot;
	if (GizmoMode == EGizmoMode::Scale)
	{
		BaseRot = TargetComponent->GetWorldRotationAsQuaternion();
	}
	else
	{
		BaseRot = bIsWorld ? FQuaternion::Identity() : TargetComponent->GetWorldRotationAsQuaternion();
	}

	bool bIsRotateMode = (GizmoMode == EGizmoMode::Rotate);

	FQuaternion AxisRots[3];
	if (bIsRotateMode)
	{
		// Rotation mode: BaseAxis가 이미 평면을 정의하므로 AxisRotation 불필요
		AxisRots[0] = FQuaternion::Identity();
		AxisRots[1] = FQuaternion::Identity();
		AxisRots[2] = FQuaternion::Identity();
	}
	else
	{
		// Translation / Scale mode
		AxisRots[0] = FQuaternion::Identity();
		AxisRots[1] = FQuaternion::FromAxisAngle(FVector::UpVector(), FVector::GetDegreeToRadian(90.0f));  // Left-Handed
		AxisRots[2] = FQuaternion::FromAxisAngle(FVector::RightVector(), FVector::GetDegreeToRadian(-90.0f));  // Left-Handed
	}
	const bool bIsOrtho = (InCamera->GetCameraType() == ECameraType::ECT_Orthographic);

	// Orthographic rotation axis determination
	EGizmoDirection OrthoWorldAxis = EGizmoDirection::None;
	if (bIsOrtho && bIsWorld && bIsRotateMode)
	{
		const FVector CamForward = InCamera->GetForward();
		const float AbsX = abs(CamForward.X);
		const float AbsY = abs(CamForward.Y);
		const float AbsZ = abs(CamForward.Z);

		if (AbsZ > AbsX && AbsZ > AbsY)
		{
			OrthoWorldAxis = EGizmoDirection::Up;
		}
		else if (AbsY > AbsX && AbsY > AbsZ)
		{
			OrthoWorldAxis = EGizmoDirection::Right;
		}
		else
		{
			OrthoWorldAxis = EGizmoDirection::Forward;
		}
	}

	bool bShowX = !bIsDragging || !bIsRotateMode || GizmoDirection == EGizmoDirection::Forward;
	bool bShowY = !bIsDragging || !bIsRotateMode || GizmoDirection == EGizmoDirection::Right;
	bool bShowZ = !bIsDragging || !bIsRotateMode || GizmoDirection == EGizmoDirection::Up;

	if (bIsOrtho && bIsWorld && bIsRotateMode && !bIsDragging)
	{
		bShowX = (OrthoWorldAxis == EGizmoDirection::Forward);
		bShowY = (OrthoWorldAxis == EGizmoDirection::Right);
		bShowZ = (OrthoWorldAxis == EGizmoDirection::Up);
	}

	FRenderState RenderState;
	RenderState.FillMode = EFillMode::Solid;
	RenderState.CullMode = ECullMode::Back;

	// Allocate HitProxy IDs per axis
	HWidgetAxis* XAxisProxy = new HWidgetAxis(EGizmoAxisType::X, InvalidHitProxyId);
	HWidgetAxis* YAxisProxy = new HWidgetAxis(EGizmoAxisType::Y, InvalidHitProxyId);
	HWidgetAxis* ZAxisProxy = new HWidgetAxis(EGizmoAxisType::Z, InvalidHitProxyId);
	HWidgetAxis* CenterProxy = new HWidgetAxis(EGizmoAxisType::Center, InvalidHitProxyId);

	FHitProxyId XAxisId = HitProxyManager.AllocateHitProxyId(XAxisProxy);
	FHitProxyId YAxisId = HitProxyManager.AllocateHitProxyId(YAxisProxy);
	FHitProxyId ZAxisId = HitProxyManager.AllocateHitProxyId(ZAxisProxy);
	FHitProxyId CenterId = HitProxyManager.AllocateHitProxyId(CenterProxy);


	// Rotation mode requires dynamic mesh generation
	if (bIsRotateMode)
	{
		// BaseAxis 정의 (X축: Z->Y, Y축: X->Z, Z축: X->Y)
		const FVector BaseAxis0[3] = {
			FVector(0, 0, 1),  // X축: Z
			FVector(1, 0, 0),  // Y축: X
			FVector(1, 0, 0)   // Z축: X
		};
		const FVector BaseAxis1[3] = {
			FVector(0, 1, 0),  // X축: Y
			FVector(0, 0, 1),  // Y축: Z
			FVector(0, 1, 0)   // Z축: Y
		};

		const FVector GizmoLoc = P.Location;
		const FVector CameraLoc = InCamera->GetLocation();
		const FVector DirectionToWidget = (GizmoLoc - CameraLoc).GetNormalized();

		// X axis rendering (YZ 평면)
		if (bShowX)
		{
			// 월드 좌표에서 축 계산
			FVector WorldAxis0 = BaseRot.RotateVector(BaseAxis0[0]);
			FVector WorldAxis1 = BaseRot.RotateVector(BaseAxis1[0]);

			// 플립 판정
			const bool bMirrorAxis0 = (WorldAxis0.Dot(DirectionToWidget) <= 0.0f);
			const bool bMirrorAxis1 = (WorldAxis1.Dot(DirectionToWidget) <= 0.0f);
			const FVector RenderWorldAxis0 = bMirrorAxis0 ? WorldAxis0 : -WorldAxis0;
			const FVector RenderWorldAxis1 = bMirrorAxis1 ? WorldAxis1 : -WorldAxis1;

			TArray<FNormalVertex> vertices;
			TArray<uint32> indices;
			FGizmoGeometry::GenerateQuarterRingMesh(RenderWorldAxis0, RenderWorldAxis1,
				RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius,
				RotateCollisionConfig.Thickness, vertices, indices);

			if (!indices.IsEmpty())
			{
				ID3D11Buffer* vb = nullptr;
				ID3D11Buffer* ib = nullptr;
				FGizmoGeometry::CreateTempBuffers(vertices, indices, &vb, &ib);

				P.VertexBuffer = vb;
				P.NumVertices = static_cast<uint32>(vertices.Num());
				P.IndexBuffer = ib;
				P.NumIndices = static_cast<uint32>(indices.Num());
				P.Rotation = FQuaternion::Identity();
				P.Color = XAxisId.GetColor();
				Renderer.RenderEditorPrimitive(P, RenderState);

				vb->Release();
				ib->Release();
			}
		}

		// Y axis rendering (XZ 평면)
		if (bShowY)
		{
			// 월드 좌표에서 축 계산
			FVector WorldAxis0 = BaseRot.RotateVector(BaseAxis0[1]);
			FVector WorldAxis1 = BaseRot.RotateVector(BaseAxis1[1]);

			// 플립 판정
			const bool bMirrorAxis0 = (WorldAxis0.Dot(DirectionToWidget) <= 0.0f);
			const bool bMirrorAxis1 = (WorldAxis1.Dot(DirectionToWidget) <= 0.0f);
			const FVector RenderWorldAxis0 = bMirrorAxis0 ? WorldAxis0 : -WorldAxis0;
			const FVector RenderWorldAxis1 = bMirrorAxis1 ? WorldAxis1 : -WorldAxis1;

			TArray<FNormalVertex> vertices;
			TArray<uint32> indices;
			FGizmoGeometry::GenerateQuarterRingMesh(RenderWorldAxis0, RenderWorldAxis1,
				RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius,
				RotateCollisionConfig.Thickness, vertices, indices);

			if (!indices.IsEmpty())
			{
				ID3D11Buffer* vb = nullptr;
				ID3D11Buffer* ib = nullptr;
				FGizmoGeometry::CreateTempBuffers(vertices, indices, &vb, &ib);

				P.VertexBuffer = vb;
				P.NumVertices = static_cast<uint32>(vertices.Num());
				P.IndexBuffer = ib;
				P.NumIndices = static_cast<uint32>(indices.Num());
				P.Rotation = FQuaternion::Identity();
				P.Color = YAxisId.GetColor();
				Renderer.RenderEditorPrimitive(P, RenderState);

				vb->Release();
				ib->Release();
			}
		}

		// Z axis rendering (XY 평면)
		if (bShowZ)
		{
			// 월드 좌표에서 축 계산
			FVector WorldAxis0 = BaseRot.RotateVector(BaseAxis0[2]);
			FVector WorldAxis1 = BaseRot.RotateVector(BaseAxis1[2]);

			// 플립 판정
			const bool bMirrorAxis0 = (WorldAxis0.Dot(DirectionToWidget) <= 0.0f);
			const bool bMirrorAxis1 = (WorldAxis1.Dot(DirectionToWidget) <= 0.0f);
			const FVector RenderWorldAxis0 = bMirrorAxis0 ? WorldAxis0 : -WorldAxis0;
			const FVector RenderWorldAxis1 = bMirrorAxis1 ? WorldAxis1 : -WorldAxis1;

			TArray<FNormalVertex> vertices;
			TArray<uint32> indices;
			FGizmoGeometry::GenerateQuarterRingMesh(RenderWorldAxis0, RenderWorldAxis1,
				RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius,
				RotateCollisionConfig.Thickness, vertices, indices);

			if (!indices.IsEmpty())
			{
				ID3D11Buffer* vb = nullptr;
				ID3D11Buffer* ib = nullptr;
				FGizmoGeometry::CreateTempBuffers(vertices, indices, &vb, &ib);

				P.VertexBuffer = vb;
				P.NumVertices = static_cast<uint32>(vertices.Num());
				P.IndexBuffer = ib;
				P.NumIndices = static_cast<uint32>(indices.Num());
				P.Rotation = FQuaternion::Identity();
				P.Color = ZAxisId.GetColor();
				Renderer.RenderEditorPrimitive(P, RenderState);

				vb->Release();
				ib->Release();
			}
		}
	}
	else
	{
		// Translate/Scale mode uses existing Primitives
		// X axis rendering
		if (bShowX)
		{
			P.Rotation = BaseRot * AxisRots[0];
			P.Color = XAxisId.GetColor();
			Renderer.RenderEditorPrimitive(P, RenderState);
		}

		// Y axis rendering
		if (bShowY)
		{
			P.Rotation = BaseRot * AxisRots[1];
			P.Color = YAxisId.GetColor();
			Renderer.RenderEditorPrimitive(P, RenderState);
		}

		// Z axis rendering
		if (bShowZ)
		{
			P.Rotation = BaseRot * AxisRots[2];
			P.Color = ZAxisId.GetColor();
			Renderer.RenderEditorPrimitive(P, RenderState);
		}

		// Center sphere rendering for Translate/Scale modes
		{
			const float SphereRadius = 0.10f * RenderScale;
			constexpr int NumSegments = 16;
			constexpr int NumRings = 16;

			TArray<FNormalVertex> vertices;
			TArray<uint32> indices;

			// Generate sphere mesh
			for (int ring = 0; ring <= NumRings; ++ring)
			{
				const float Theta = PI * static_cast<float>(ring) / static_cast<float>(NumRings);
				const float SinTheta = sinf(Theta);
				const float CosTheta = cosf(Theta);

				for (int seg = 0; seg <= NumSegments; ++seg)
				{
					const float Phi = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(NumSegments);
					const float SinPhi = sinf(Phi);
					const float CosPhi = cosf(Phi);

					FNormalVertex v;
					v.Position.X = SphereRadius * SinTheta * CosPhi;
					v.Position.Y = SphereRadius * SinTheta * SinPhi;
					v.Position.Z = SphereRadius * CosTheta;
					v.Normal = v.Position.GetNormalized();
					v.Color = FVector4(1, 1, 1, 1);
					vertices.Add(v);
				}
			}

			// Generate indices
			for (int ring = 0; ring < NumRings; ++ring)
			{
				for (int seg = 0; seg < NumSegments; ++seg)
				{
					const int Current = ring * (NumSegments + 1) + seg;
					const int Next = Current + NumSegments + 1;

					indices.Add(Current);
					indices.Add(Next);
					indices.Add(Current + 1);

					indices.Add(Current + 1);
					indices.Add(Next);
					indices.Add(Next + 1);
				}
			}

			if (!indices.IsEmpty())
			{
				ID3D11Buffer* sphereVB = nullptr;
				ID3D11Buffer* sphereIB = nullptr;
				FGizmoGeometry::CreateTempBuffers(vertices, indices, &sphereVB, &sphereIB);

				FEditorPrimitive SpherePrim = P;
				SpherePrim.VertexBuffer = sphereVB;
				SpherePrim.NumVertices = static_cast<uint32>(vertices.Num());
				SpherePrim.IndexBuffer = sphereIB;
				SpherePrim.NumIndices = static_cast<uint32>(indices.Num());
				SpherePrim.Rotation = FQuaternion::Identity();
				SpherePrim.Scale = FVector(1.0f, 1.0f, 1.0f);
				SpherePrim.Color = CenterId.GetColor();
				Renderer.RenderEditorPrimitive(SpherePrim, RenderState);

				sphereVB->Release();
				sphereIB->Release();
			}
		}
	}
}
