#pragma once

#include "PSMBounding.h"
#include "Global/Vector.h"
#include "Global/Matrix.h"
#include "Global/Types.h"
#include <vector>

class UCamera;
class UStaticMeshComponent;

/**
 * @brief 그림자 매핑 투영 타입
 */
enum class EShadowProjectionMode : uint8
{
	Uniform = 0,        // 표준 직교 그림자 맵
	PSM = 1,            // 원근 그림자 맵 (Perspective Shadow Map)
	LSPSM = 2,          // 라이트 공간 원근 그림자 맵 (Light-Space Perspective Shadow Map)
	TSM = 3             // 사다리꼴 그림자 맵 (Trapezoidal Shadow Map)
};

/**
 * @brief PSM 알고리즘 파라미터
 */
struct FPSMParameters
{
	// 가상 카메라 파라미터
	float ZNear = 0.1f;
	float ZFar = 1000.0f;
	float SlideBack = 0.0f;

	// PSM 전용
	float MinInfinityZ = 1.5f;         // 무한 평면까지의 최소 거리
	bool bUnitCubeClip = true;         // 실제 리시버에 맞게 그림자 맵 최적화
	bool bSlideBackEnabled = true;     // 슬라이드 백 최적화 활성화

	// LSPSM 전용
	float LSPSMNoptWeight = 1.0f;      // Nopt 계산 가중치 (0-1)

	// TSM 전용
	float TSMDelta = 0.52f;            // 사다리꼴 포커스 파라미터 (0-1)

	// 계산된 값 (출력)
	float PPNear = 0.0f;               // 포스트-프로젝티브 근평면
	float PPFar = 1.0f;                // 포스트-프로젝티브 원평면
	float CosGamma = 0.0f;             // 빛과 뷰 방향 사이의 각도
	bool bShadowTestInverted = false;  // 빛이 카메라 뒤에 있음
};

/**
 * @brief PSM 투영 행렬 계산기
 *
 * PracticalPSM (NVIDIA 샘플)의 원근 그림자 매핑 알고리즘을
 * FutureEngine 좌표계에 맞게 구현
 */
class FPSMCalculator
{
public:
	/**
	 * @brief 지정된 알고리즘을 사용하여 라이트 뷰-투영 행렬 계산
	 *
	 * @param Mode 그림자 투영 모드 (Uniform, PSM, LSPSM, TSM)
	 * @param OutViewMatrix 출력 라이트 뷰 행렬
	 * @param OutProjectionMatrix 출력 라이트 투영 행렬
	 * @param LightDirection 디렉셔널 라이트 방향 (월드 공간)
	 * @param Camera 씬 카메라
	 * @param Meshes AABB 계산용 씬 메시들
	 * @param InOutParams PSM 파라미터 (입력/출력)
	 */
	static void CalculateShadowProjection(
		EShadowProjectionMode Mode,
		FMatrix& OutViewMatrix,
		FMatrix& OutProjectionMatrix,
		const FVector& LightDirection,
		UCamera* Camera,
		const TArray<UStaticMeshComponent*>& Meshes,
		FPSMParameters& InOutParams
	);

private:
	/**
	 * @brief 그림자 캐스터/리시버 분류 계산
	 * 어떤 메시가 그림자를 드리우고 어떤 메시가 받는지 결정
	 */
	static void ComputeVirtualCameraParameters(
		const FVector& LightDirection,
		UCamera* Camera,
		const TArray<UStaticMeshComponent*>& Meshes,
		TArray<FPSMBoundingBox>& OutShadowCasters,
		TArray<FPSMBoundingBox>& OutShadowReceivers,
		FPSMParameters& InOutParams
	);

	/**
	 * @brief Uniform 그림자 맵 생성 (직교 투영)
	 */
	static void BuildUniformShadowMap(
		FMatrix& OutView,
		FMatrix& OutProj,
		const FVector& LightDirection,
		UCamera* Camera,
		const TArray<FPSMBoundingBox>& ShadowCasters,
		const TArray<FPSMBoundingBox>& ShadowReceivers,
		FPSMParameters& Params
	);

	/**
	 * @brief 원근 그림자 맵 (PSM) 생성
	 */
	static void BuildPSMProjection(
		FMatrix& OutView,
		FMatrix& OutProj,
		const FVector& LightDirection,
		UCamera* Camera,
		const TArray<FPSMBoundingBox>& ShadowCasters,
		const TArray<FPSMBoundingBox>& ShadowReceivers,
		FPSMParameters& Params
	);

	/**
	 * @brief 라이트 공간 원근 그림자 맵 (LSPSM) 생성
	 */
	static void BuildLSPSMProjection(
		FMatrix& OutView,
		FMatrix& OutProj,
		const FVector& LightDirection,
		UCamera* Camera,
		const TArray<UStaticMeshComponent*>& Meshes,
		FPSMParameters& Params
	);

	/**
	 * @brief 사다리꼴 그림자 맵 (TSM) 생성
	 */
	static void BuildTSMProjection(
		FMatrix& OutView,
		FMatrix& OutProj,
		const FVector& LightDirection,
		UCamera* Camera,
		const TArray<FPSMBoundingBox>& ShadowCasters,
		const TArray<FPSMBoundingBox>& ShadowReceivers,
		FPSMParameters& Params
	);
};
