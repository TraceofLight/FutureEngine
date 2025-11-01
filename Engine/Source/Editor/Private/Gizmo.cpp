#include "pch.h"
#include "Editor/Public/Gizmo.h"

#include "Editor/Public/Editor.h"
#include "Editor/Public/GizmoMath.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Manager/UI/Public/ViewportManager.h"
#include "Manager/Config/Public/ConfigManager.h"
#include "Render/UI/Overlay/Public/D2DOverlayManager.h"

IMPLEMENT_CLASS(UGizmo, UObject)

void UGizmo::SetGizmoMode(EGizmoMode Mode)
{
	GizmoMode = Mode;
	// ConfigManager에 동기화 (소멸 시점 문제 방지)
	UConfigManager::GetInstance().SetCachedGizmoMode(Mode);
}

UGizmo::UGizmo()
{
	UAssetManager& ResourceManager = UAssetManager::GetInstance();
	Primitives.SetNum(3);
	GizmoColor.SetNum(3);

	/* *
	* @brief 0: Forward(x), 1: Right(y), 2: Up(z)
	*/
	GizmoColor[0] = FVector4(1, 0, 0, 1);
	GizmoColor[1] = FVector4(0, 1, 0, 1);
	GizmoColor[2] = FVector4(0, 0, 1, 1);

	/**
	 * @brief Translation Setting
	 */
	const float ScaleT = TranslateCollisionConfig.Scale;
	Primitives[0].VertexBuffer = ResourceManager.GetVertexbuffer(EPrimitiveType::Arrow);
	Primitives[0].NumVertices = ResourceManager.GetNumVertices(EPrimitiveType::Arrow);
	Primitives[0].Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	Primitives[0].Scale = FVector(ScaleT, ScaleT, ScaleT);
	Primitives[0].bShouldAlwaysVisible = true;

	/**
	 * @brief Rotation Setting
	 */
	Primitives[1].VertexBuffer = nullptr;  // 렌더링 시 동적으로 설정
	Primitives[1].NumVertices = 0;
	Primitives[1].Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	Primitives[1].Scale = FVector(ScaleT, ScaleT, ScaleT);
	Primitives[1].bShouldAlwaysVisible = true;

	/**
	 * @brief Scale Setting
	 */
	Primitives[2].VertexBuffer = ResourceManager.GetVertexbuffer(EPrimitiveType::CubeArrow);
	Primitives[2].NumVertices = ResourceManager.GetNumVertices(EPrimitiveType::CubeArrow);
	Primitives[2].Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	Primitives[2].Scale = FVector(ScaleT, ScaleT, ScaleT);
	Primitives[2].bShouldAlwaysVisible = true;

	/* *
	* @brief Render State
	*/
	RenderState.FillMode = EFillMode::Solid;
	RenderState.CullMode = ECullMode::None;
}

UGizmo::~UGizmo() = default;

void UGizmo::UpdateScale(const UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
	TargetComponent = Cast<USceneComponent>(GEditor->GetEditorModule()->GetSelectedComponent());
	if (!TargetComponent || !InCamera)
	{
		return;
	}

	// 스크린에서 균일한 사이즈를 가지도록 하기 위한 스케일 조정
	const FVector GizmoLocation = TargetComponent->GetWorldLocation();
	const float Scale = FGizmoMath::CalculateScreenSpaceScale(InCamera, InViewport, GizmoLocation, 120.0f);

	TranslateCollisionConfig.Scale = Scale;
	RotateCollisionConfig.Scale = Scale;
}

void UGizmo::ChangeGizmoMode()
{
	switch (GizmoMode)
	{
	case EGizmoMode::Translate:
		GizmoMode = EGizmoMode::Rotate; break;
	case EGizmoMode::Rotate:
		GizmoMode = EGizmoMode::Scale; break;
	case EGizmoMode::Scale:
		GizmoMode = EGizmoMode::Translate;
	}
}

void UGizmo::SetLocation(const FVector& Location) const
{
	TargetComponent->SetWorldLocation(Location);
}

bool UGizmo::IsInRadius(float Radius) const
{
	if (Radius >= RotateCollisionConfig.InnerRadius * RotateCollisionConfig.Scale && Radius <= RotateCollisionConfig.OuterRadius * RotateCollisionConfig.Scale)
		return true;
	return false;
}

void UGizmo::OnMouseDragStart(const FVector& CollisionPoint)
{
	bIsDragging = true;
	DragStartMouseLocation = CollisionPoint;
	PreviousMouseLocation = CollisionPoint;  // 누적 각도 계산을 위한 초기화

	// 스크린 공간 드래그 초기화 (뷰포트 로컬 좌표)
	POINT MousePos;
	GetCursorPos(&MousePos);
	ScreenToClient(GetActiveWindow(), &MousePos);

	const FRect& ViewportRect = UViewportManager::GetInstance().GetActiveViewportRect();
	const FVector2 CurrentScreenPos(
		static_cast<float>(MousePos.x) - static_cast<float>(ViewportRect.Left),
		static_cast<float>(MousePos.y) - static_cast<float>(ViewportRect.Top)
	);
	PreviousScreenPos = CurrentScreenPos;
	DragStartScreenPos = CurrentScreenPos;

	// 드래그 시작 방향 계산 (Arc 렌더링의 시작점으로 사용)
	FVector GizmoCenter = Primitives[static_cast<int>(GizmoMode)].Location;
	DragStartDirection = (CollisionPoint - GizmoCenter).GetNormalized();

	if (TargetComponent)
	{
		DragStartActorLocation = TargetComponent->GetWorldLocation();
		DragStartActorRotation = TargetComponent->GetWorldRotation();
		DragStartActorRotationQuat = TargetComponent->GetWorldRotationAsQuaternion();
		DragStartActorScale = TargetComponent->GetWorldScale3D();
	}
}

// 하이라이트 색상은 렌더 시점에만 계산 (상태 오염 방지)
FVector4 UGizmo::ColorFor(EGizmoDirection InAxis) const
{
	// Scale 모드에서만 Center 선택 시 모든 축 및 평면 하이라이팅
	if (GizmoMode == EGizmoMode::Scale && GizmoDirection == EGizmoDirection::Center)
	{
		if (InAxis == EGizmoDirection::Forward || InAxis == EGizmoDirection::Right || InAxis == EGizmoDirection::Up ||
		    InAxis == EGizmoDirection::XY_Plane || InAxis == EGizmoDirection::XZ_Plane || InAxis == EGizmoDirection::YZ_Plane)
		{
			if (bIsDragging)
			{
				return {0.8f, 0.8f, 0.0f, 1.0f};  // 드래그 중: 짙은 노란색
			}
			else
			{
				return {1.0f, 1.0f, 0.0f, 1.0f};  // 호버 중: 밝은 노란색
			}
		}
	}

	return FGizmoMath::CalculateColor(InAxis, GizmoDirection, bIsDragging, GizmoColor);
}

void UGizmo::CollectRotationAngleOverlay(FD2DOverlayManager& OverlayManager, UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
	if (!bIsDragging || GizmoMode != EGizmoMode::Rotate || !TargetComponent)
	{
		return;
	}

	const FVector GizmoLocation = GetGizmoLocation();
	const FVector LocalGizmoAxis = GetGizmoAxis();
	const FQuaternion StartRotQuat = GetDragStartActorRotationQuat();

	// 월드 공간 회전축
	FVector WorldRotationAxis = LocalGizmoAxis;
	if (!IsWorldMode())
	{
		WorldRotationAxis = StartRotQuat.RotateVector(LocalGizmoAxis);
	}

	// 현재 회전 각도 계산 (스냅 적용)
	float DisplayAngleRadians = GetCurrentRotationAngle();
	if (UViewportManager::GetInstance().IsRotationSnapEnabled())
	{
		const float SnapAngleDegrees = UViewportManager::GetInstance().GetRotationSnapAngle();
		const float SnapAngleRadians = FVector::GetDegreeToRadian(SnapAngleDegrees);
		DisplayAngleRadians = std::round(GetCurrentRotationAngle() / SnapAngleRadians) * SnapAngleRadians;
	}

	// RotateVector 부호 보정
	DisplayAngleRadians = -DisplayAngleRadians;

	// 각 축마다 다른 BaseAxis 사용
	FVector BaseAxis0, BaseAxis1;
	switch (GizmoDirection)
	{
	case EGizmoDirection::Forward:  // X축: Z→Y
		BaseAxis0 = FVector(0, 0, 1);
		BaseAxis1 = FVector(0, 1, 0);
		break;
	case EGizmoDirection::Right:    // Y축: X→Z
		BaseAxis0 = FVector(1, 0, 0);
		BaseAxis1 = FVector(0, 0, 1);
		break;
	case EGizmoDirection::Up:       // Z축: X→Y
		BaseAxis0 = FVector(1, 0, 0);
		BaseAxis1 = FVector(0, 1, 0);
		break;
	default:
		return;
	}

	// Local 모드면 컴포넌트 회전만 적용
	FQuaternion TotalRotation = FQuaternion::Identity();
	if (!IsWorldMode())
	{
		TotalRotation = StartRotQuat;
	}

	// Z축은 각도 반전 (언리얼 표준)
	float PositionAngleRadians = DisplayAngleRadians;
	if (GizmoDirection == EGizmoDirection::Up)
	{
		PositionAngleRadians = -DisplayAngleRadians;
	}

	// 평면에서 각도 위치의 방향 계산
	const FVector LocalDirection = BaseAxis0 * cosf(PositionAngleRadians) + BaseAxis1 * sinf(PositionAngleRadians);

	// 월드 공간으로 변환
	const FVector AngleDirection = TotalRotation.RotateVector(LocalDirection);

	// 회전 반지름 (기즈모 스케일 적용)
	const float RotateRadius = GetRotateOuterRadius();

	// 원 위의 점 (월드 공간)
	const FVector PointOnCircle = GizmoLocation + AngleDirection * RotateRadius;

	// 스크린 공간으로 투영
	const FCameraConstants& CamConst = InCamera->GetFViewProjConstants();
	const FMatrix ViewProj = CamConst.View * CamConst.Projection;

	FVector4 GizmoScreenPos4 = FVector4(GizmoLocation, 1.0f) * ViewProj;
	FVector4 PointScreenPos4 = FVector4(PointOnCircle, 1.0f) * ViewProj;

	if (GizmoScreenPos4.W <= 0.0f || PointScreenPos4.W <= 0.0f)
	{
		return;
	}

	GizmoScreenPos4 *= 1.0f / GizmoScreenPos4.W;
	PointScreenPos4 *= 1.0f / PointScreenPos4.W;

	// NDC -> 스크린 좌표
	const FVector2 GizmoScreenPos(
		(GizmoScreenPos4.X * 0.5f + 0.5f) * InViewport.Width + InViewport.TopLeftX,
		((-GizmoScreenPos4.Y) * 0.5f + 0.5f) * InViewport.Height + InViewport.TopLeftY
	);

	const FVector2 PointScreenPos(
		(PointScreenPos4.X * 0.5f + 0.5f) * InViewport.Width + InViewport.TopLeftX,
		((-PointScreenPos4.Y) * 0.5f + 0.5f) * InViewport.Height + InViewport.TopLeftY
	);

	// 기즈모 중심에서 점으로 향하는 방향
	FVector2 DirectionToPoint = PointScreenPos - GizmoScreenPos;
	const float DistToPoint = DirectionToPoint.Length();
	if (DistToPoint < 0.01f)
	{
		return;
	}
	DirectionToPoint = DirectionToPoint.GetNormalized();

	// 텍스트 위치 (뷰포트 크기 비례 원 바깥으로 추가 오프셋)
	// 스크린 공간 원 반지름에 비례하는 오프셋 계산
	constexpr float ScreenRadiusRatio = 0.3f;
	const float TextOffset = DistToPoint * ScreenRadiusRatio;
	const float TextX = PointScreenPos.X + DirectionToPoint.X * TextOffset;
	const float TextY = PointScreenPos.Y + DirectionToPoint.Y * TextOffset;

	// 텍스트 포맷 (UI와 일치하도록 각도 표시)
	float DisplayAngleDegrees = FVector::GetRadianToDegree(DisplayAngleRadians);

	// Z축만 각도 부호 반전
	if (GizmoDirection == EGizmoDirection::Up)
	{
		DisplayAngleDegrees = -DisplayAngleDegrees;
	}

	wchar_t AngleText[32];
	(void)swprintf_s(AngleText, L"%.1f°", DisplayAngleDegrees);

	// 텍스트 박스 (중앙 정렬)
	constexpr float TextBoxWidth = 60.0f;
	constexpr float TextBoxHeight = 20.0f;
	D2D1_RECT_F TextRect = D2D1::RectF(
		TextX - TextBoxWidth * 0.5f,
		TextY - TextBoxHeight * 0.5f,
		TextX + TextBoxWidth * 0.5f,
		TextY + TextBoxHeight * 0.5f
	);

	// 반투명 회색 배경
	const D2D1_COLOR_F ColorGrayBG = D2D1::ColorF(0.2f, 0.2f, 0.2f, 0.7f);
	OverlayManager.AddRectangle(TextRect, ColorGrayBG, true);

	// 노란색 텍스트
	const D2D1_COLOR_F ColorYellow = D2D1::ColorF(1.0f, 1.0f, 0.0f);
	OverlayManager.AddText(AngleText, TextRect, ColorYellow, 15.0f, true, true, L"Consolas");
}
