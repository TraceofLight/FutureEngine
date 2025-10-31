#include "pch.h"
#include "Vector.h"

#include "Core/Public/Archive.h"

/**
 * @brief FVector 기본 생성자
 */
FVector::FVector()
	: X(0), Y(0), Z(0)
{
}


/**
 * @brief FVector의 멤버값을 Param으로 넘기는 생성자
 */
FVector::FVector(float InX, float InY, float InZ)
	: X(InX), Y(InY), Z(InZ)
{
}


/**
 * @brief FVector를 Param으로 넘기는 생성자
 */
FVector::FVector(const FVector& InOther)
	: X(InOther.X), Y(InOther.Y), Z(InOther.Z)
{
}

void FVector::operator=(const FVector4& InOther)
{
	*this = FVector(InOther.X, InOther.Y, InOther.Z);
}


/**
 * @brief 두 벡터를 더한 새로운 벡터를 반환하는 함수
 */
FVector FVector::operator+(const FVector& InOther) const
{
	return { X + InOther.X, Y + InOther.Y, Z + InOther.Z };
}

/**
 * @brief 두 벡터를 뺀 새로운 벡터를 반환하는 함수
 */
FVector FVector::operator-(const FVector& InOther) const
{
	return { X - InOther.X, Y - InOther.Y, Z - InOther.Z };
}

FVector FVector::operator*(const FVector& InOther) const
{
	return { X * InOther.X, Y * InOther.Y, Z * InOther.Z };
}

FVector FVector::operator/(const FVector& InOther) const
{
	return { X/InOther.X, Y/InOther.Y, Z/InOther.Z };
}

/**
 * @brief 자신의 벡터에서 배율을 곱한 백테를 반환하는 함수
 */
FVector FVector::operator*(const float InRatio) const
{
	return { X * InRatio, Y * InRatio, Z * InRatio };
}

FVector FVector::operator/(const float InRatio) const
{
	return { X / InRatio, Y / InRatio, Z / InRatio };
}

/**
 * @brief 자신의 벡터에 다른 벡터를 가산하는 함수
 */
FVector& FVector::operator+=(const FVector& InOther)
{
	X += InOther.X;
	Y += InOther.Y;
	Z += InOther.Z;
	return *this; // 연쇄적인 연산을 위해 자기 자신을 반환
}

/**
 * @brief 자신의 벡터에서 다른 벡터를 감산하는 함수
 */
FVector& FVector::operator-=(const FVector& InOther)
{
	X -= InOther.X;
	Y -= InOther.Y;
	Z -= InOther.Z;
	return *this; // 연쇄적인 연산을 위해 자기 자신을 반환
}

/**
 * @brief 자신의 벡터에서 배율을 곱한 뒤 자신을 반환
 */

FVector& FVector::operator*=(const float InRatio)
{
	X *= InRatio;
	Y *= InRatio;
	Z *= InRatio;

	return *this;
}

bool FVector::operator==(const FVector& InOther) const
{
	if (X == InOther.X && Y == InOther.Y && Z == InOther.Z)
	{
		return true;
	}
	return false;
}

bool FVector::operator!=(const FVector& InOther) const
{
	if (X != InOther.X || Y != InOther.Y || Z != InOther.Z)
	{
		return true;
	}
	return false;
}

FArchive& operator<<(FArchive& Ar, FVector& Vector)
{
	Ar << Vector.X;
	Ar << Vector.Y;
	Ar << Vector.Z;
	return Ar;
}

// FVector4 constructors moved to header as constexpr inline functions


/**
 * @brief 두 벡터를 더한 새로운 벡터를 반환하는 함수
 */
FVector4 FVector4::operator+(const FVector4& InOtherVector) const
{
	return FVector4(
		X + InOtherVector.X,
		Y + InOtherVector.Y,
		Z + InOtherVector.Z,
		W + InOtherVector.W
	);
}

FVector4 FVector4::operator*(const FMatrix& InMatrix) const
{
	__m128 VecX = _mm_shuffle_ps(V, V, _MM_SHUFFLE(0, 0, 0, 0)); // (X, X, X, X)
	__m128 VecY = _mm_shuffle_ps(V, V, _MM_SHUFFLE(1, 1, 1, 1)); // (Y, Y, Y, Y)
	__m128 VecZ = _mm_shuffle_ps(V, V, _MM_SHUFFLE(2, 2, 2, 2)); // (Z, Z, Z, Z)
	__m128 VecW = _mm_shuffle_ps(V, V, _MM_SHUFFLE(3, 3, 3, 3)); // (W, W, W, W)

	const __m128* MatrixRows = (const __m128*)InMatrix.Data;

	// R0 = Row0 * X
	// R0 = (m00*X, m01*X, m02*X, m03*X)
	__m128 R0 = _mm_mul_ps(MatrixRows[0], VecX);

	// R1 = R0 + (Row1 * Y)
	// R1 = (m00*X + m10*Y, m01*X + m11*Y, ...)
	__m128 R1 = _mm_fmadd_ps(MatrixRows[1], VecY, R0);

	// R2 = R1 + (Row2 * Z)
	__m128 R2 = _mm_fmadd_ps(MatrixRows[2], VecZ, R1);

	// Final Result = R2 + (Row3 * W)
	// 최종 결과 레지스터에는 (Result.X, Result.Y, Result.Z, Result.W)가 담깁니다.
	__m128 ResultVec = _mm_fmadd_ps(MatrixRows[3], VecW, R2);

	// 4. 결과를 FVector4 구조체에 저장합니다.
	FVector4 Result;
	_mm_store_ps((float*)&Result, ResultVec);

	return Result;
}
/**
 * @brief 두 벡터를 뺀 새로운 벡터를 반환하는 함수
 */
FVector4 FVector4::operator-(const FVector4& InOtherVector) const
{
	return FVector4(
		X - InOtherVector.X,
		Y - InOtherVector.Y,
		Z - InOtherVector.Z,
		W - InOtherVector.W
	);
}

/**
 * @brief 자신의 벡터에 배율을 곱한 값을 반환하는 함수
 */
FVector4 FVector4::operator*(const float InRatio) const
{
	return FVector4(
		X * InRatio,
		Y * InRatio,
		Z * InRatio,
		W * InRatio
	);
}

/**
 * @brief 자신의 벡터에 스칼라를 나눈 값을  반환하는 함수
 */
FVector4 FVector4::operator/(float Scalar) const
{
	// divide with zero 방지
	if (Scalar >= -0.0001f && Scalar <= 0.0001f)
		return FVector4();

	return FVector4(X / Scalar, Y / Scalar, Z / Scalar, W / Scalar);
}

/**
 * @brief 자신의 벡터에 다른 벡터를 가산하는 함수
 */
void FVector4::operator+=(const FVector4& InOtherVector)
{
	X += InOtherVector.X;
	Y += InOtherVector.Y;
	Z += InOtherVector.Z;
	W += InOtherVector.W;
}

/**
 * @brief 자신의 벡터에 다른 벡터를 감산하는 함수
 */
void FVector4::operator-=(const FVector4& InOtherVector)
{
	X -= InOtherVector.X;
	Y -= InOtherVector.Y;
	Z -= InOtherVector.Z;
	W -= InOtherVector.W;
}

/**
 * @brief 자신의 벡터에 배율을 곱하는 함수
 */
void FVector4::operator*=(const float Ratio)
{
	X *= Ratio;
	Y *= Ratio;
	Z *= Ratio;
	W *= Ratio;
}

/**
 * @brief 자신의 벡터를 스칼라로 나누는 함수
 */
void FVector4::operator/=(const float Scalar)
{
	X /= Scalar;
	Y /= Scalar;
	Z /= Scalar;
	W /= Scalar;
}

FArchive& operator<<(FArchive& Ar, FVector4& Vector)
{
	Ar << Vector.X;
	Ar << Vector.Y;
	Ar << Vector.Z;
	Ar << Vector.W;
	return Ar;
}

/**
 * @brief FVector2 기본 생성자
 */
FVector2::FVector2()
	: X(0), Y(0)
{
}

/**
 * @brief FVector2의 멤버값을 Param으로 넘기는 생성자
 */
FVector2::FVector2(float InX, float InY)
	: X(InX), Y(InY)
{
}

/**
 * @brief FVector2를 Param으로 넘기는 생성자
 */
FVector2::FVector2(const FVector2& InOther)
	: X(InOther.X), Y(InOther.Y)
{
}

/**
 * @brief 두 벡터를 더한 새로운 벡터를 반환하는 함수
 */
FVector2 FVector2::operator+(const FVector2& InOther) const
{
	return { X + InOther.X, Y + InOther.Y };
}

/**
 * @brief 두 벡터를 뺀 새로운 벡터를 반환하는 함수
 */
FVector2 FVector2::operator-(const FVector2& InOther) const
{
	return { X - InOther.X, Y - InOther.Y };
}

FVector2 FVector2::operator-() const
{
	return {-X, -Y};
}

/**
 * @brief 자신의 벡터에서 배율을 곱한 백터를 반환하는 함수
 */
FVector2 FVector2::operator*(const float Ratio) const
{
	return { X * Ratio, Y * Ratio };
}

FArchive& operator<<(FArchive& Ar, FVector2& Vector)
{
	Ar << Vector.X;
	Ar << Vector.Y;
	return Ar;
}

