#include "pch.h"
#include "Render/Shadow/Public/PSMCalculator.h"
#include "Editor/Public/Camera.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include <algorithm>
#include <cmath>

#define W_EPSILON 0.001f
#define Z_EPSILON 0.0001f
#define ZNEAR_MIN 0.1f       // LSPSM용 최소 near plane
#define ZFAR_MAX 1000.0f     // LSPSM용 최대 far plane

static const FVector ZUp(0, 0, 1);  // FutureEngine Z-Up
static const FVector XForward(1, 0, 0);  // FutureEngine X-Forward

//-----------------------------------------------------------------------------
// 메인 진입점
//-----------------------------------------------------------------------------

void FPSMCalculator::CalculateShadowProjection(
	EShadowProjectionMode Mode,
	FMatrix& OutViewMatrix,
	FMatrix& OutProjectionMatrix,
	const FVector& LightDirection,
	UCamera* Camera,
	const TArray<UStaticMeshComponent*>& Meshes,
	FPSMParameters& InOutParams)
{
	// 그림자 캐스터와 리시버 분류
	TArray<FPSMBoundingBox> ShadowCasters, ShadowReceivers;
	ComputeVirtualCameraParameters(
		LightDirection, Camera, Meshes,
		ShadowCasters, ShadowReceivers, InOutParams
	);

	// 모드에 따라 투영 행렬 생성
	switch (Mode)
	{
	case EShadowProjectionMode::Uniform:
		BuildUniformShadowMap(OutViewMatrix, OutProjectionMatrix, LightDirection, Camera,
			ShadowCasters, ShadowReceivers, InOutParams);
		break;

	case EShadowProjectionMode::PSM:
		BuildPSMProjection(OutViewMatrix, OutProjectionMatrix, LightDirection, Camera,
			ShadowCasters, ShadowReceivers, InOutParams);
		break;

	case EShadowProjectionMode::LSPSM:
		// LiSPSM은 world space 데이터가 필요하므로 Meshes를 직접 전달
		BuildLSPSMProjection(OutViewMatrix, OutProjectionMatrix, LightDirection, Camera,
			Meshes, InOutParams);
		break;

	case EShadowProjectionMode::TSM:
		BuildTSMProjection(OutViewMatrix, OutProjectionMatrix, LightDirection, Camera,
			ShadowCasters, ShadowReceivers, InOutParams);
		break;

	default:
		BuildUniformShadowMap(OutViewMatrix, OutProjectionMatrix, LightDirection, Camera,
			ShadowCasters, ShadowReceivers, InOutParams);
		break;
	}
}

//-----------------------------------------------------------------------------
// 그림자 캐스터/리시버 분류
//-----------------------------------------------------------------------------

void FPSMCalculator::ComputeVirtualCameraParameters(
	const FVector& LightDirection,
	UCamera* Camera,
	const TArray<UStaticMeshComponent*>& Meshes,
	TArray<FPSMBoundingBox>& OutShadowCasters,
	TArray<FPSMBoundingBox>& OutShadowReceivers,
	FPSMParameters& InOutParams)
{
	OutShadowCasters.Empty();
	OutShadowReceivers.Empty();

	if (!Camera)
	{
		return;
	}

	// 카메라 행렬 가져오기
	const FCameraConstants& CamConstants = Camera->GetFViewProjConstants();
	FMatrix ViewMatrix = CamConstants.View;
	FMatrix ProjMatrix = CamConstants.Projection;
	FMatrix ViewProj = ViewMatrix * ProjMatrix;

	// 컬링용 프러스텀 생성
	FPSMFrustum SceneFrustum(ViewProj);

	// 빛 스윕 방향 (빛 방향의 반대)
	FVector SweepDir = -LightDirection.GetNormalized();

	// 각 메시 테스트
	for (auto* Mesh : Meshes)
	{
		if (!Mesh || !Mesh->IsVisible())
			continue;

		// 메시 월드 AABB 가져오기
		FPSMBoundingBox MeshBox;
		GetMeshWorldBoundingBox(MeshBox, Mesh);

		if (!MeshBox.IsValid())
			continue;

		// 프러스텀에 대해 테스트
		int FrustumTest = SceneFrustum.TestBox(MeshBox);

		// 저장을 위해 뷰 공간으로 변환
		FPSMBoundingBox ViewSpaceBox;
		TransformBoundingBox(ViewSpaceBox, MeshBox, ViewMatrix);

		switch (FrustumTest)
		{
		case 0:  // 프러스텀 밖 - 그림자 투사를 위해 swept sphere 테스트
		{
			FPSMBoundingSphere MeshSphere(MeshBox);
			if (SceneFrustum.TestSweptSphere(MeshSphere, SweepDir))
			{
				OutShadowCasters.Add(ViewSpaceBox);
			}
			break;
		}

		case 1:  // 완전히 내부 - 캐스터이자 리시버
			OutShadowCasters.Add(ViewSpaceBox);
			OutShadowReceivers.Add(ViewSpaceBox);
			break;

		case 2:  // 교차 - 캐스터이자 리시버
			OutShadowCasters.Add(ViewSpaceBox);
			OutShadowReceivers.Add(ViewSpaceBox);
			break;
		}
	}

	// 리시버로부터 near/far 계산
	if (!OutShadowReceivers.IsEmpty())
	{
		float MinZ = FLT_MAX;
		float MaxZ = -FLT_MAX;

		for (const auto& Box : OutShadowReceivers)
		{
			MinZ = std::min(MinZ, Box.MinPt.Z);
			MaxZ = std::max(MaxZ, Box.MaxPt.Z);
		}

		InOutParams.ZNear = std::max(0.1f, MinZ);
		InOutParams.ZFar = std::min(10000.0f, MaxZ);
	}
	else
	{
		InOutParams.ZNear = 0.1f;
		InOutParams.ZFar = 10000.0f;
	}

	InOutParams.SlideBack = 0.0f;

	// 감마 계산 (빛과 뷰 방향 사이의 각도)
	FVector ViewDir(ViewMatrix.Data[0][2], ViewMatrix.Data[1][2], ViewMatrix.Data[2][2]);  // 뷰 전방 (뷰 공간의 Z)
	InOutParams.CosGamma = LightDirection.GetNormalized().Dot(ViewDir);
}

//-----------------------------------------------------------------------------
// Uniform 그림자 맵 (직교 투영)
//-----------------------------------------------------------------------------

void FPSMCalculator::BuildUniformShadowMap(
	FMatrix& OutView,
	FMatrix& OutProj,
	const FVector& LightDirection,
	UCamera* Camera,
	const TArray<FPSMBoundingBox>& ShadowCasters,
	const TArray<FPSMBoundingBox>& ShadowReceivers,
	FPSMParameters& Params)
{
	const FCameraConstants& CamConstants = Camera->GetFViewProjConstants();
	FMatrix CameraView = CamConstants.View;

	// 월드 공간 빛 방향
	FVector WorldLightDir = LightDirection.GetNormalized();

	// 씬 AABB 계산
	FPSMBoundingBox SceneBox;
	if (Params.bUnitCubeClip)
	{
		SceneBox = FPSMBoundingBox(ShadowReceivers);
	}
	else
	{
		// 카메라 프러스텀 범위 사용
		float Aspect = Camera->GetAspect();
		float FOV = Camera->GetFovY();
		float FarDist = Camera->GetFarZ();
		float Height = std::tan(FOV * 0.5f) * FarDist;
		float Width = Height * Aspect;

		SceneBox.MinPt = FVector(-Width, -Height, 0.1f);
		SceneBox.MaxPt = FVector(Width, Height, FarDist);
	}

	// Directional Light View Matrix를 Camera / Cascade와 동일한 방식으로 생성
	// X-forward Z-up LH 좌표계에 맞춰 Right, Up, Forward 계산
	FVector Forward = WorldLightDir;
	Forward.Normalize();

	// WorldUp 벡터 선택 (빛이 거의 수직이면 다른 축 사용)
	FVector WorldUp = ZUp;
	if (std::abs(Forward.Z) > 0.99f)
	{
		WorldUp = XForward;
	}

	// Right = WorldUp × Forward (LH 외적)
	FVector Right = WorldUp.Cross(Forward);
	Right.Normalize();

	// Up = Forward × Right
	FVector Up = Forward.Cross(Right);
	Up.Normalize();

	// View Matrix = [Right | Up | Forward]^T (Camera/Cascade와 동일 구조)
	FMatrix ViewRot = FMatrix(Right, Up, Forward);
	OutView = ViewRot.Transpose();

	// 씬 AABB를 라이트 공간으로 변환
	FPSMBoundingBox LightSpaceBox;
	TransformBoundingBox(LightSpaceBox, SceneBox, OutView);

	// 그림자 캐스터도 고려
	if (!ShadowCasters.IsEmpty())
	{
		FPSMBoundingBox CasterBox(ShadowCasters);
		FPSMBoundingBox LightSpaceCasterBox;
		TransformBoundingBox(LightSpaceCasterBox, CasterBox, OutView);

		// 캐스터를 포함하도록 박스 확장
		LightSpaceBox.Merge(LightSpaceCasterBox.MinPt);
		LightSpaceBox.Merge(LightSpaceCasterBox.MaxPt);
	}

	// 직교 투영 생성
	OutProj = FMatrix::CreateOrthoOffCenterLH(
		LightSpaceBox.MinPt.X, LightSpaceBox.MaxPt.X,
		LightSpaceBox.MinPt.Y, LightSpaceBox.MaxPt.Y,
		LightSpaceBox.MinPt.Z, LightSpaceBox.MaxPt.Z
	);

	// 카메라 뷰와 결합
	OutView = CameraView * OutView;

	Params.PPNear = LightSpaceBox.MinPt.Z;
	Params.PPFar = LightSpaceBox.MaxPt.Z;
}

//-----------------------------------------------------------------------------
// PSM (원근 그림자 맵)
//-----------------------------------------------------------------------------

void FPSMCalculator::BuildPSMProjection(
	FMatrix& OutView,
	FMatrix& OutProj,
	const FVector& LightDirection,
	UCamera* Camera,
	const TArray<FPSMBoundingBox>& ShadowCasters,
	const TArray<FPSMBoundingBox>& ShadowReceivers,
	FPSMParameters& Params)
{
	const FCameraConstants& CamConstants = Camera->GetFViewProjConstants();
	FMatrix CameraView = CamConstants.View;
	FVector ViewLightDir = CameraView.TransformVector(LightDirection.GetNormalized());

	// 단계 1: 슬라이드 백이 적용된 가상 카메라 설정
	FMatrix VirtualCameraView = FMatrix::Identity();
	FMatrix VirtualCameraProj;

	float ZNear = Params.ZNear;
	float ZFar = Params.ZFar;
	float SlideBack = 0.0f;

	if (Params.bSlideBackEnabled)
	{
		// 무한 평면 거리 계산
		float Infinity = ZFar / (ZFar - ZNear);
		float MinInfZ = Params.MinInfinityZ;

		if (Infinity <= MinInfZ)
		{
			float AdditionalSlide = MinInfZ * (ZFar - ZNear) - ZFar + Z_EPSILON;
			SlideBack = AdditionalSlide;
			ZFar += AdditionalSlide;
			ZNear += AdditionalSlide;
		}

		VirtualCameraView = FMatrix::CreateTranslation(FVector(0, 0, SlideBack));

		if (Params.bUnitCubeClip && !ShadowReceivers.IsEmpty())
		{
			// 경계 원뿔을 사용하여 타이트한 FOV 계산
			FVector EyePos = FVector::ZeroVector();
			FVector EyeDir = FVector(0, 0, 1);  // 뷰 공간의 Z-Forward (DirectX 표준)
			FPSMBoundingCone BC(ShadowReceivers, VirtualCameraView, EyePos, EyeDir);

			float Width = 2.0f * std::tan(BC.FovX) * ZNear;
			float Height = 2.0f * std::tan(BC.FovY) * ZNear;
			VirtualCameraProj = FMatrix::CreatePerspectiveLH(Width, Height, ZNear, ZFar);
		}
		else
		{
			// 슬라이드 백 조정이 적용된 카메라 FOV 사용
			float FOV = Camera->GetFovY();
			float Aspect = Camera->GetAspect();
			float ViewHeight = std::tan(FOV * 0.5f) * Camera->GetFarZ();
			float ViewWidth = ViewHeight * Aspect;
			float FarDist = Camera->GetFarZ();

			float HalfFovY = std::atan(ViewHeight / (FarDist + SlideBack));
			float HalfFovX = std::atan(ViewWidth / (FarDist + SlideBack));

			float Width = 2.0f * std::tan(HalfFovX) * ZNear;
			float Height = 2.0f * std::tan(HalfFovY) * ZNear;
			VirtualCameraProj = FMatrix::CreatePerspectiveLH(Width, Height, ZNear, ZFar);
		}
	}
	else
	{
		float FOV = Camera->GetFovY();
		float Aspect = Camera->GetAspect();
		VirtualCameraProj = FMatrix::CreatePerspectiveFovLH(FOV, Aspect, ZNear, ZFar);
	}

	FMatrix VirtualCameraViewProj = CameraView * VirtualCameraView * VirtualCameraProj;
	FMatrix EyeToPostProjective = VirtualCameraView * VirtualCameraProj;

	// 단계 2: 빛 방향을 포스트-프로젝티브 공간으로 변환
	FVector4 LightDirW(ViewLightDir.X, ViewLightDir.Y, ViewLightDir.Z, 0.0f);
	FVector4 PPLight = VirtualCameraProj.TransformVector4(LightDirW);

	Params.bShadowTestInverted = (PPLight.W < 0.0f);

	// 단계 3: 투영 타입 결정
	if (std::abs(PPLight.W) <= W_EPSILON)
	{
		// 빛이 무한대에 있음 - 직교 투영 사용
		BuildUniformShadowMap(OutView, OutProj, LightDirection, Camera,
			ShadowCasters, ShadowReceivers, Params);
		return;
	}

	// 단계 4: 원근 PSM
	FVector PPLightPos(
		PPLight.X / PPLight.W,
		PPLight.Y / PPLight.W,
		PPLight.Z / PPLight.W
	);

	FMatrix LightView, LightProj;

	const FVector PPCubeCenter(0.0f, 0.0f, 0.5f);

	if (Params.bShadowTestInverted)
	{
		// 역 투영 (빛이 카메라 뒤에 있음)
		FPSMBoundingCone ViewCone;
		if (!Params.bUnitCubeClip)
		{
			// 전체 유닛 큐브 투영
			TArray<FPSMBoundingBox> UnitBox;
			FPSMBoundingBox Cube;
			Cube.MinPt = FVector(-1, -1, 0);
			Cube.MaxPt = FVector(1, 1, 1);
			UnitBox.Add(Cube);

			ViewCone = FPSMBoundingCone(UnitBox, FMatrix::Identity(), PPLightPos);
		}
		else
		{
			ViewCone = FPSMBoundingCone(ShadowReceivers, EyeToPostProjective, PPLightPos);
		}

		ViewCone.Near = std::max(0.001f, ViewCone.Near * 0.3f);
		Params.PPNear = -ViewCone.Near;
		Params.PPFar = ViewCone.Near;

		LightView = ViewCone.LookAtMatrix;

		float Width = 2.0f * std::tan(ViewCone.FovX) * Params.PPNear;
		float Height = 2.0f * std::tan(ViewCone.FovY) * Params.PPNear;
		LightProj = FMatrix::CreatePerspectiveLH(Width, Height, Params.PPNear, Params.PPFar);
	}
	else
	{
		// 일반 투영
		FVector LookAt = PPCubeCenter - PPLightPos;
		float Distance = LookAt.Length();
		LookAt = LookAt * (1.0f / Distance);

		FVector Up = ZUp;
		if (std::abs(ZUp.Dot(LookAt)) > 0.99f)
			Up = XForward;

		if (!Params.bUnitCubeClip)
		{
			// 간단한 구 기반 FOV
			const float PPCubeRadius = 1.5f;

			LightView = FMatrix::CreateLookAtLH(PPLightPos, PPCubeCenter, Up);

			float Fovy = 2.0f * std::atan(PPCubeRadius / Distance);
			float Aspect = 1.0f;
			float FNear = std::max(0.001f, Distance - 2.0f * PPCubeRadius);
			float FFar = Distance + 2.0f * PPCubeRadius;

			Params.PPNear = FNear;
			Params.PPFar = FFar;

			LightProj = FMatrix::CreatePerspectiveFovLH(Fovy, Aspect, FNear, FFar);
		}
		else
		{
			// 유닛 큐브 클리핑
			FPSMBoundingCone BC(ShadowReceivers, EyeToPostProjective, PPLightPos);

			LightView = BC.LookAtMatrix;

			float Fovy = 2.0f * BC.FovY;
			float Aspect = BC.FovX / BC.FovY;
			float FNear = BC.Near * 0.6f;  // 약간의 조정
			float FFar = BC.Far;

			FNear = std::max(0.001f, FNear);

			Params.PPNear = FNear;
			Params.PPFar = FFar;

			LightProj = FMatrix::CreatePerspectiveFovLH(Fovy, Aspect, FNear, FFar);
		}
	}

	// 단계 5: 행렬 결합
	FMatrix LightViewProj = LightView * LightProj;
	OutView = VirtualCameraViewProj;
	OutProj = LightViewProj;

	// 참고: 실제 렌더링에서는 다음과 같이 결합됨: FinalMatrix = View * VirtualView * VirtualProj * LightView * LightProj
	// 하지만 ShadowMapPass를 위해 최종 결합 행렬을 ViewProj로 반환
	OutView = CameraView;
	OutProj = VirtualCameraView * VirtualCameraProj * LightView * LightProj;

	Params.SlideBack = SlideBack;
}

//-----------------------------------------------------------------------------
// LSPSM Helper Functions
//-----------------------------------------------------------------------------

/**
 * @brief 카메라 frustum의 8개 코너를 view space에서 계산
 * @param Camera 씬 카메라
 * @return 8개 코너 (near plane 4개 + far plane 4개)
 */
static std::vector<FVector> GetCameraFrustumCornersInViewSpace(UCamera* Camera)
{
	std::vector<FVector> Corners;
	Corners.reserve(8);

	const FCameraConstants& CamConstants = Camera->GetFViewProjConstants();
	float NearZ = Camera->GetNearZ();
	float FarZ = Camera->GetFarZ();
	float FovY = Camera->GetFovY();
	float Aspect = Camera->GetAspect();

	// View space에서 frustum 크기 계산
	float NearHeight = 2.0f * std::tan(FovY * 0.5f) * NearZ;
	float NearWidth = NearHeight * Aspect;
	float FarHeight = 2.0f * std::tan(FovY * 0.5f) * FarZ;
	float FarWidth = FarHeight * Aspect;

	// View space (Z-forward, X-right, Y-up in camera local)
	// FutureEngine: X-forward, Y-right, Z-up in world
	// View space는 카메라 로컬 공간이므로 표준 convention 사용
	// Forward = (0, 0, Z), Right = (X, 0, 0), Up = (0, Y, 0)

	// Near plane 4 corners (counter-clockwise from top-left)
	Corners.push_back(FVector(-NearWidth * 0.5f, NearHeight * 0.5f, NearZ));   // Top-left
	Corners.push_back(FVector(NearWidth * 0.5f, NearHeight * 0.5f, NearZ));    // Top-right
	Corners.push_back(FVector(NearWidth * 0.5f, -NearHeight * 0.5f, NearZ));   // Bottom-right
	Corners.push_back(FVector(-NearWidth * 0.5f, -NearHeight * 0.5f, NearZ));  // Bottom-left

	// Far plane 4 corners
	Corners.push_back(FVector(-FarWidth * 0.5f, FarHeight * 0.5f, FarZ));      // Top-left
	Corners.push_back(FVector(FarWidth * 0.5f, FarHeight * 0.5f, FarZ));       // Top-right
	Corners.push_back(FVector(FarWidth * 0.5f, -FarHeight * 0.5f, FarZ));      // Bottom-right
	Corners.push_back(FVector(-FarWidth * 0.5f, -FarHeight * 0.5f, FarZ));     // Bottom-left

	return Corners;
}

/**
 * @brief Light-Space basis 행렬 생성
 * @param ViewLightDir View space에서의 빛 방향 (씬에서 빛을 향하는 방향)
 * @param ViewDir View space에서의 뷰 방향 (보통 (0, 0, -1) for Z-forward)
 * @return Light-Space basis 회전 행렬 (3x3 rotation part만)
 */
static FMatrix CreateLightSpaceBasis(const FVector& ViewLightDir, const FVector& ViewDir)
{
	// PracticalPSM 알고리즘:
	// upVector = lightDir (씬에서 빛을 향하는 방향)
	// leftVector = cross(upVector, eyeVector)
	// viewVector = cross(upVector, leftVector)

	FVector UpVector = ViewLightDir.GetNormalized();
	FVector EyeVector = ViewDir.GetNormalized();

	// Left vector = cross(up, eye)
	FVector LeftVector = Cross(UpVector, EyeVector);
	LeftVector.Normalize();

	// View vector = cross(up, left)
	FVector ViewVector = Cross(UpVector, LeftVector);
	ViewVector.Normalize();

	// Build rotation matrix (basis vectors as rows for DirectX left-handed)
	FMatrix Basis = FMatrix::Identity();

	Basis.Data[0][0] = LeftVector.X;  Basis.Data[0][1] = UpVector.X;  Basis.Data[0][2] = ViewVector.X;
	Basis.Data[1][0] = LeftVector.Y;  Basis.Data[1][1] = UpVector.Y;  Basis.Data[1][2] = ViewVector.Y;
	Basis.Data[2][0] = LeftVector.Z;  Basis.Data[2][1] = UpVector.Z;  Basis.Data[2][2] = ViewVector.Z;

	Basis.Data[3][0] = 0.0f;  Basis.Data[3][1] = 0.0f;  Basis.Data[3][2] = 0.0f;  Basis.Data[3][3] = 1.0f;

	return Basis;
}

//-----------------------------------------------------------------------------
// LSPSM & TSM - 플레이스홀더 구현
// 이들은 복잡하며, 필요시 나중에 추가될 수 있습니다
//-----------------------------------------------------------------------------

void FPSMCalculator::BuildLSPSMProjection(
	FMatrix& OutView,
	FMatrix& OutProj,
	const FVector& LightDirection,
	UCamera* Camera,
	const TArray<UStaticMeshComponent*>& Meshes,
	FPSMParameters& Params)
{
	// Sample LiSPSM 알고리즘 정확한 재구현 (line 536-607)
	// World space 데이터 사용
	const FCameraConstants& CamConstants = Camera->GetFViewProjConstants();
	FMatrix CameraView = CamConstants.View;
	FMatrix CameraProj = CamConstants.Projection;

	// Get camera world position
	FMatrix InvCameraView = CameraView.Inverse();
	FVector EyePos(InvCameraView.Data[3][0], InvCameraView.Data[3][1], InvCameraView.Data[3][2]);

	// Sample line 536: viewdir = Vector3(-view._13, -view._23, -view._33)
	// Column 2 of view matrix
	FVector ViewDir = FVector(-CameraView.Data[0][2], -CameraView.Data[1][2], -CameraView.Data[2][2]);
	ViewDir.Normalize();

	// Sample line 537: lightdir = Vector3(-lightview._13, -lightview._23, -lightview._33)
	FVector LightDir = -LightDirection.GetNormalized();

	// Check degenerate case
	float CosGamma = ViewDir.Dot(LightDir);
	Params.CosGamma = CosGamma;

	if (std::abs(CosGamma) >= 0.999f)
	{
		// Fallback to uniform
		// TODO: Call proper uniform implementation
		OutView = FMatrix::Identity();
		OutProj = FMatrix::Identity();
		return;
	}

	// Sample line 554-558: Build Light-Space basis
	FVector LSLeft = Cross(LightDir, ViewDir);
	LSLeft.Normalize();

	FVector LSUp = Cross(LSLeft, LightDir);
	LSUp.Normalize();

	// Sample line 560-566: Light-Space view matrix (world space)
	FMatrix LSLightView = FMatrix::Identity();
	LSLightView.Data[0][0] = LSLeft.X;   LSLightView.Data[0][1] = LSUp.X;   LSLightView.Data[0][2] = LightDir.X;
	LSLightView.Data[1][0] = LSLeft.Y;   LSLightView.Data[1][1] = LSUp.Y;   LSLightView.Data[1][2] = LightDir.Y;
	LSLightView.Data[2][0] = LSLeft.Z;   LSLightView.Data[2][1] = LSUp.Z;   LSLightView.Data[2][2] = LightDir.Z;
	LSLightView.Data[3][0] = -LSLeft.Dot(EyePos);
	LSLightView.Data[3][1] = -LSUp.Dot(EyePos);
	LSLightView.Data[3][2] = -LightDir.Dot(EyePos);

	// Sample line 542-545: Body B = frustum + scene intersection points (WORLD SPACE!)
	// 간소화: Mesh AABB만 사용 (frustum intersection은 복잡하므로 생략)
	TArray<FVector> BodyB;
	BodyB.Reserve(Meshes.Num() * 8);

	// Mesh AABB corners (WORLD SPACE directly!) - 카메라 독립적!
	for (auto* Mesh : Meshes)
	{
		if (!Mesh || !Mesh->IsVisible())
			continue;

		FVector MinPt, MaxPt;
		Mesh->GetWorldAABB(MinPt, MaxPt);

		// 8 corners of AABB
		BodyB.Add(MinPt);
		BodyB.Add(FVector(MaxPt.X, MinPt.Y, MinPt.Z));
		BodyB.Add(FVector(MinPt.X, MaxPt.Y, MinPt.Z));
		BodyB.Add(FVector(MaxPt.X, MaxPt.Y, MinPt.Z));
		BodyB.Add(FVector(MinPt.X, MinPt.Y, MaxPt.Z));
		BodyB.Add(FVector(MaxPt.X, MinPt.Y, MaxPt.Z));
		BodyB.Add(FVector(MinPt.X, MaxPt.Y, MaxPt.Z));
		BodyB.Add(MaxPt);
	}

	// Sample line 572: Transform Body B to Light Space, calculate AABB
	FPSMBoundingBox LSBody;
	for (const auto& Pt : BodyB)
	{
		FVector LSPt = LSLightView.TransformPosition(Pt);
		LSBody.Merge(LSPt);
	}

	// Check if LSBody is valid
	if (!LSBody.IsValid() || BodyB.IsEmpty())
	{
		// Fallback to uniform
		OutView = FMatrix::Identity();
		OutProj = FMatrix::Identity();
		return;
	}

	// Sample line 574-581: Calculate LSPSM parameters
	// ViewNear should be camera's actual near plane
	float ViewNear = Camera->GetNearZ();
	float SinGamma = std::sqrt(1.0f - CosGamma * CosGamma);
	SinGamma = std::max(SinGamma, 0.01f);

	float ZNear = ViewNear / SinGamma;
	float D = std::abs(LSBody.MaxPt.Y - LSBody.MinPt.Y);
	D = std::max(D, 0.1f);  // Minimum D to avoid degenerate cases
	float ZFar = ZNear + D * SinGamma;
	float N = (ZNear + std::sqrt(ZFar * ZNear)) / SinGamma;
	float F = N + D;

	// Safety check: F must be greater than N
	if (F - N < 0.01f)
	{
		// Fallback to uniform
		OutView = FMatrix::Identity();
		OutProj = FMatrix::Identity();
		return;
	}

	// Sample line 583-586: LiSPSM projection matrix (Y-axis warp only)
	FMatrix LiSPSMProj = FMatrix::Identity();
	LiSPSMProj.Data[1][1] = (F + N) / (F - N);
	LiSPSMProj.Data[3][1] = -2.0f * F * N / (F - N);
	LiSPSMProj.Data[1][3] = 1.0f;
	LiSPSMProj.Data[3][3] = 0.0f;

	// Sample line 592-596: Correct eye position
	FVector Corrected = EyePos - LSUp * (N - ViewNear);
	LSLightView.Data[3][0] = -LSLeft.Dot(Corrected);
	LSLightView.Data[3][1] = -LSUp.Dot(Corrected);
	LSLightView.Data[3][2] = -LightDir.Dot(Corrected);

	// Sample line 598-599: Apply projection, recalculate AABB
	FMatrix LSLightViewProj = LSLightView * LiSPSMProj;

	LSBody = FPSMBoundingBox();
	for (const auto& Pt : BodyB)
	{
		FVector4 Pt4D(Pt.X, Pt.Y, Pt.Z, 1.0f);
		FVector4 TransformedPt4D = Pt4D * LSLightViewProj;

		// Perspective divide (Sample line 1052-1054)
		if (std::abs(TransformedPt4D.W) > 1e-6f)
		{
			FVector TransformedPt(
				TransformedPt4D.X / TransformedPt4D.W,
				TransformedPt4D.Y / TransformedPt4D.W,
				TransformedPt4D.Z / TransformedPt4D.W);

			LSBody.Merge(TransformedPt);
		}
	}

	// Sample line 601-605: Fit to unit cube
	FMatrix FitToUnitCube = FMatrix::CreateOrthoLH(
		LSBody.MinPt.X, LSBody.MaxPt.X,
		LSBody.MinPt.Y, LSBody.MaxPt.Y,
		LSBody.MinPt.Z, LSBody.MaxPt.Z);

	// Sample line 607: Final matrix
	FMatrix FinalProj = LSLightViewProj * FitToUnitCube;

	// Return as View * Proj format
	OutView = FMatrix::Identity();
	OutProj = FinalProj;
}

void FPSMCalculator::BuildTSMProjection(
	FMatrix& OutView,
	FMatrix& OutProj,
	const FVector& LightDirection,
	UCamera* Camera,
	const TArray<FPSMBoundingBox>& ShadowCasters,
	const TArray<FPSMBoundingBox>& ShadowReceivers,
	FPSMParameters& Params)
{
	// 현재는 uniform으로 대체
	BuildUniformShadowMap(OutView, OutProj, LightDirection, Camera, ShadowCasters, ShadowReceivers, Params);
}
