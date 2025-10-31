#pragma once
#include "Core/Public/Object.h"
#include "Optimization/Public/ViewVolumeCuller.h"

class UConfigManager;

enum class ECameraType
{
	ECT_Orthographic,
	ECT_Perspective
};

class UCamera : public UObject
{
public:
	// Camera Speed Constants
	static constexpr float MIN_SPEED = 1.0f;
	static constexpr float MAX_SPEED = 100.0f;
	static constexpr float DEFAULT_SPEED = 20.0f;
	static constexpr float SPEED_ADJUST_STEP = 3.0f;

	UCamera();
	~UCamera() override;

	/* *
	* @brief Update 관련 함수
	* UpdateInput 함수는 사용자 입력으로 비롯된 변화의 갱신를 담당합니다.
	* Update, UpdateMatrix 함수들은 카메라의 변환 행렬의 갱신을 담당합니다.
	*/
	FVector UpdateInput();
	void Update(const D3D11_VIEWPORT& InViewport);
	void UpdateMatrixByPers();
	void UpdateMatrixByOrth();

	float GetOrthoUnitsPerPixel(float ViewportWidth) const;

	/**
	 * @brief Setter
	 */
	void SetLocation(const FVector& InOtherPosition) { RelativeLocation = InOtherPosition; }
	void SetRotation(const FVector& InOtherRotation);
	void SetRotation(const FRotator& InRotator) { RelativeRotation = InRotator; }
	void SetRotationQuat(const FQuaternion& InQuat) { RelativeRotation = FRotator(InQuat.ToEuler().Y, InQuat.ToEuler().Z, InQuat.ToEuler().X); }
	void SetFovY(const float InOtherFovY) { FovY = InOtherFovY; }
	void SetAspect(const float InOtherAspect) { Aspect = InOtherAspect; }
	void SetNearZ(const float InOtherNearZ) { NearZ = InOtherNearZ; }
	void SetFarZ(const float InOtherFarZ) { FarZ = InOtherFarZ; }
	void SetOrthoWidth(const float InOrthoWidth) { OrthoWidth = InOrthoWidth; }
	void SetOrthoZoom(const float InOrthoZoom) { OrthoZoom = InOrthoZoom; }
	void SetCameraType(const ECameraType InCameraType) { CameraType = InCameraType; }
	void SetForward(const FVector& InForward) { Forward = InForward; }
	void SetUp(const FVector& InUp) { Up = InUp; }
	void SetRight(const FVector& InRight) { Right = InRight; }

	/**
	 * @brief Getter
	 */
	const FCameraConstants& GetFViewProjConstants() const { return CameraConstants; }
	const FCameraConstants GetFViewProjConstantsInverse() const;

	FRay ConvertToWorldRay(float NdcX, float NdcY) const;

	FVector CalculatePlaneNormal(const FVector4& Axis);
	FVector CalculatePlaneNormal(const FVector& Axis);
	const FVector& GetLocation() const { return RelativeLocation; }
	FVector GetRotation() const { return FVector(RelativeRotation.Roll, RelativeRotation.Pitch, RelativeRotation.Yaw); }
	const FRotator& GetRotationRotator() const { return RelativeRotation; }
	FQuaternion GetRotationQuat() const { return RelativeRotation.Quaternion(); }
	const FVector& GetForward() const { return Forward; }
	const FVector& GetUp() const { return Up; }
	const FVector& GetRight() const { return Right; }
	float GetFovY() const { return FovY; }
	float GetAspect() const { return Aspect; }
	float GetNearZ() const { return NearZ; }
	float GetFarZ() const { return FarZ; }
	float GetOrthoWidth() const { return OrthoWidth; }
	float GetOrthoZoom() const { return OrthoZoom; }
	ECameraType GetCameraType() const { return CameraType; }
	ViewVolumeCuller& GetViewVolumeCuller() { return ViewVolumeCuller; }

	// Input enable for main editor camera (disable when hovering other viewports)
	void SetInputEnabled(bool b) { bInputEnabled = b; }
	bool GetInputEnabled() const { return bInputEnabled; }
	
	// Camera Movement Speed Control
	float GetMoveSpeed() const { return CurrentMoveSpeed; }
	void SetMoveSpeed(float InSpeed)
	{
		CurrentMoveSpeed = clamp(InSpeed, MIN_SPEED, MAX_SPEED);
	}
	void AdjustMoveSpeed(float InDelta) { SetMoveSpeed(CurrentMoveSpeed + InDelta); }

	/* *
	 * @brief 행렬 형태로 저장된 좌표와 변환 행렬과의 연산한 결과를 반환합니다.
	 */
	inline FVector4 MultiplyPointWithMatrix(const FVector4& Point, const FMatrix& Matrix) const
	{
		FVector4 Result = Point * Matrix;
		/* *
		 * @brief 좌표가 왜곡된 공간에 남는 것을 방지합니다.
		 */
		if (Result.W != 0.f) { Result *= (1.f / Result.W); }

		return Result;
	}

private:
	FCameraConstants CameraConstants = {};
	FVector RelativeLocation = {};
	FRotator RelativeRotation = FRotator(0.0f, 0.0f, 0.0f);
	FVector Forward = { 1,0,0 };
	FVector Up = {0,0,1};
	FVector Right = {0,1,0};
	float FovY = {};
	float Aspect = {};
	float NearZ = {};
	float FarZ = {};
	float OrthoWidth = {};
	float OrthoZoom = 1000.0f; // 뷰포트 독립적 줌 레벨
	ECameraType CameraType = {};

	// 절두체 컬링을 이용한 최적화
	ViewVolumeCuller ViewVolumeCuller;

	// Whether this camera consumes input (movement/rotation). Only used by editor main camera.
	bool bInputEnabled = true;
	bool bIsMainDrraging = false;
	
	// Dynamic Movement Speed
	float CurrentMoveSpeed = DEFAULT_SPEED;
};
