#pragma once
#include "Global/Quaternion.h"

/**
 * @brief 회전을 Pitch, Yaw, Roll로 표현하는 구조체
 * - Pitch: X축 회전 (상하, 도 단위)
 * - Yaw: Z축 회전 (좌우, 도 단위)
 * - Roll: Y축 회전 (기울기, 도 단위)
 */
struct FRotator
{
	float Pitch;  // X축 회전 (도 단위)
	float Yaw;    // Z축 회전 (도 단위)
	float Roll;   // Y축 회전 (도 단위)

	FRotator() : Pitch(0.0f), Yaw(0.0f), Roll(0.0f) {}
	FRotator(float InPitch, float InYaw, float InRoll) : Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {}

	/**
	 * @brief FRotator -> FQuaternion 변환
	 * FRotator(Pitch, Yaw, Roll) -> FVector(Roll, Pitch, Yaw) -> FQuaternion
	 */
	FQuaternion Quaternion() const
	{
		// FRotator(Pitch, Yaw, Roll) -> FVector(Roll, Pitch, Yaw) for FromEuler
		return FQuaternion::FromEuler(FVector(Roll, Pitch, Yaw));
	}

	/**
	 * @brief UE 표준: 각도를 -180 ~ 180 범위로 정규화
	 * @param Angle 정규화할 각도 (도 단위)
	 * @return 정규화된 각도
	 */
	static float NormalizeAxis(float Angle)
	{
		Angle = fmodf(Angle, 360.0f);
		if (Angle > 180.0f)
		{
			Angle -= 360.0f;
		}
		else if (Angle < -180.0f)
		{
			Angle += 360.0f;
		}
		return Angle;
	}

	/**
	 * @brief 각도 정규화가 적용된 FRotator 반환
	 * @return 정규화된 FRotator
	 */
	FRotator GetNormalized() const
	{
		return {NormalizeAxis(Pitch), NormalizeAxis(Yaw), NormalizeAxis(Roll)};
	}

	/**
	 * @brief UE 표준: FRotator가 0인지 확인
	 */
	bool IsZero() const
	{
		return Pitch == 0.0f && Yaw == 0.0f && Roll == 0.0f;
	}

	/**
	 * @brief UE 표준: Zero FRotator
	 */
	static FRotator ZeroRotator() { return {0.0f, 0.0f, 0.0f}; }

	/**
	 * @brief 연산자 오버로드
	 */
	FRotator operator+(const FRotator& R) const
	{
		return {Pitch + R.Pitch, Yaw + R.Yaw, Roll + R.Roll};
	}

	FRotator operator-(const FRotator& R) const
	{
		return {Pitch - R.Pitch, Yaw - R.Yaw, Roll - R.Roll};
	}

	FRotator operator*(float Scale) const
	{
		return {Pitch * Scale, Yaw * Scale, Roll * Scale};
	}

	FRotator& operator+=(const FRotator& R)
	{
		Pitch += R.Pitch;
		Yaw += R.Yaw;
		Roll += R.Roll;
		return *this;
	}

	bool operator==(const FRotator& R) const
	{
		return Pitch == R.Pitch && Yaw == R.Yaw && Roll == R.Roll;
	}

	bool operator!=(const FRotator& R) const
	{
		return !(*this == R);
	}
};
