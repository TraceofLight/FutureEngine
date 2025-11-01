#pragma once

class UStaticMeshComponent;

/**
 * @brief 축 정렬 경계 상자 (Axis-Aligned Bounding Box)
 */
struct FPSMBoundingBox
{
	FVector MinPt = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector MaxPt = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	FPSMBoundingBox() = default;
	FPSMBoundingBox(const FPSMBoundingBox& Other) : MinPt(Other.MinPt), MaxPt(Other.MaxPt) {}

	/**
	 * @brief 점 배열로부터 AABB 생성
	 */
	explicit FPSMBoundingBox(const TArray<FVector>& Points)
	{
		for (const auto& Pt : Points)
		{
			Merge(Pt);
		}
	}

	/**
	 * @brief 여러 AABB를 하나로 병합
	 */
	explicit FPSMBoundingBox(const TArray<FPSMBoundingBox>& Boxes)
	{
		for (const auto& Box : Boxes)
		{
			Merge(Box.MinPt);
			Merge(Box.MaxPt);
		}
	}

	/**
	 * @brief AABB의 중심점 반환
	 */
	FVector GetCenter() const
	{
		return (MinPt + MaxPt) * 0.5f;
	}

	/**
	 * @brief 점을 AABB에 병합
	 */
	void Merge(const FVector& Point)
	{
		MinPt.X = std::min(MinPt.X, Point.X);
		MinPt.Y = std::min(MinPt.Y, Point.Y);
		MinPt.Z = std::min(MinPt.Z, Point.Z);

		MaxPt.X = std::max(MaxPt.X, Point.X);
		MaxPt.Y = std::max(MaxPt.Y, Point.Y);
		MaxPt.Z = std::max(MaxPt.Z, Point.Z);
	}

	/**
	 * @brief AABB의 i번째 모서리 반환 (0-7)
	 */
	FVector GetCorner(int Index) const
	{
		return {
			(Index & 1) ? MaxPt.X : MinPt.X,
			(Index & 2) ? MaxPt.Y : MinPt.Y,
			(Index & 4) ? MaxPt.Z : MinPt.Z
		};
	}

	/**
	 * @brief 광선-박스 교차 테스트
	 * @param OutHitDist 광선을 따라 교차점까지의 거리
	 * @param Origin 광선 원점
	 * @param Direction 광선 방향 (정규화되어야 함)
	 * @return 교차가 존재하면 true
	 */
	bool Intersect(float& OutHitDist, const FVector& Origin, const FVector& Direction) const;

	/**
	 * @brief AABB가 유효한지 확인 (비어있지 않은지)
	 */
	bool IsValid() const
	{
		return MinPt.X <= MaxPt.X && MinPt.Y <= MaxPt.Y && MinPt.Z <= MaxPt.Z;
	}
};

/**
 * @brief 경계 구 (Bounding Sphere)
 */
struct FPSMBoundingSphere
{
	FVector Center = FVector::ZeroVector();
	float Radius = 0.0f;

	FPSMBoundingSphere() = default;
	FPSMBoundingSphere(const FPSMBoundingSphere& Other) : Center(Other.Center), Radius(Other.Radius) {}

	/**
	 * @brief AABB로부터 경계 구 생성
	 */
	explicit FPSMBoundingSphere(const FPSMBoundingBox& Box)
	{
		Center = Box.GetCenter();
		FVector RadiusVec = Box.MaxPt - Center;
		Radius = RadiusVec.Length();
	}

	/**
	 * @brief 점들로부터 최소 경계 구 생성
	 */
	explicit FPSMBoundingSphere(const TArray<FVector>& Points);
};

/**
 * @brief 컬링 테스트용 뷰 프러스텀
 */
struct FPSMFrustum
{
	FVector4 Planes[6];  // Left, Right, Bottom, Top, Near, Far (DirectX 뷰 공간)
	FVector Corners[8];  // 프러스텀 8개 모서리
	int VertexLUT[6];    // 각 평면에 대한 가장 가까운/먼 정점 인덱스

	FPSMFrustum() = default;

	/**
	 * @brief 뷰-투영 행렬로부터 프러스텀 생성
	 * @param ViewProj 결합된 뷰-투영 행렬
	 */
	explicit FPSMFrustum(const FMatrix& ViewProj);

	/**
	 * @brief 구가 프러스텀 내부에 있는지 테스트
	 */
	bool TestSphere(const FPSMBoundingSphere& Sphere) const;

	/**
	 * @brief AABB가 프러스텀 내부/교차하는지 테스트
	 * @return 0 = 외부, 1 = 완전히 내부, 2 = 교차
	 */
	int TestBox(const FPSMBoundingBox& Box) const;

	/**
	 * @brief Swept Sphere(방향으로 확장된 구)가 프러스텀과 교차하는지 테스트
	 * 그림자 캐스터 컬링에 사용됨
	 */
	bool TestSweptSphere(const FPSMBoundingSphere& Sphere, const FVector& SweepDir) const;
};

/**
 * @brief 경계 원뿔 (Bounding Cone) - PSM에서 타이트한 FOV 계산에 사용
 */
struct FPSMBoundingCone
{
	FVector Apex = FVector::ZeroVector();
	FVector Direction = FVector(1, 0, 0);  // FutureEngine에서는 X-Forward
	float FovX = 0.0f;  // 라디안 단위 반각
	float FovY = 0.0f;  // 라디안 단위 반각
	float Near = 0.001f;
	float Far = 1.0f;
	FMatrix LookAtMatrix = FMatrix::Identity();

	FPSMBoundingCone() = default;

	/**
	 * @brief AABB들과 꼭짓점으로부터 경계 원뿔 생성
	 * 최적의 뷰 방향과 FOV를 자동으로 계산
	 *
	 * @param Boxes 포스트-프로젝티브 공간의 AABB들
	 * @param Projection 포스트-프로젝티브 공간으로의 변환
	 * @param InApex 원뿔 꼭짓점 (포스트-프로젝티브 공간의 빛 위치)
	 */
	FPSMBoundingCone(
		const TArray<FPSMBoundingBox>& Boxes,
		const FMatrix& Projection,
		const FVector& InApex
	);

	/**
	 * @brief 미리 정의된 방향으로 경계 원뿔 생성
	 *
	 * @param Boxes 포스트-프로젝티브 공간의 AABB들
	 * @param Projection 포스트-프로젝티브 공간으로의 변환
	 * @param InApex 원뿔 꼭짓점
	 * @param InDirection 원뿔 방향 (정규화됨)
	 */
	FPSMBoundingCone(
		const TArray<FPSMBoundingBox>& Boxes,
		const FMatrix& Projection,
		const FVector& InApex,
		const FVector& InDirection
	);
};

/**
 * @brief 행렬로 AABB 변환
 * @param Result 출력 변환된 AABB
 * @param Source 입력 AABB
 * @param Transform 변환 행렬
 */
void TransformBoundingBox(FPSMBoundingBox& Result, const FPSMBoundingBox& Source, const FMatrix& Transform);

/**
 * @brief 스태틱 메시 컴포넌트의 월드 공간 AABB 가져오기
 */
void GetMeshWorldBoundingBox(FPSMBoundingBox& OutBox, UStaticMeshComponent* Mesh);

/**
 * @brief Swept Sphere-평면 교차 테스트 (그림자 캐스터 컬링용)
 */
bool SweptSpherePlaneIntersect(
	float& OutT0,
	float& OutT1,
	const FVector4& Plane,
	const FPSMBoundingSphere& Sphere,
	const FVector& SweepDir
);
