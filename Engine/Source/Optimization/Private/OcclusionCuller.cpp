#include "pch.h"
#include "Optimization/Public/OcclusionCuller.h"
#include "Component/Public/PrimitiveComponent.h"

#include "Level/Public/Level.h"
#include "Global/Octree.h"

COcclusionCuller::COcclusionCuller()
{ 
    CPU_ZBuffer.SetNum(Z_BUFFER_SIZE);
}

void COcclusionCuller::InitializeCuller(const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix)
{
    fill(CPU_ZBuffer.begin(), CPU_ZBuffer.end(), 1.0f);
    CurrentViewProj = ViewMatrix * ProjectionMatrix;
}

TArray<UPrimitiveComponent*> COcclusionCuller::PerformCulling(const TArray<UPrimitiveComponent*>& AllPrimitives, const FVector& CameraPos)
{    
    Frame++;
    // 0. Primitive AABB 데이터 채우기
    CachedAABBs.Empty();
    for (int32 i = 0; i < AllPrimitives.Num(); ++i)
    {
        UPrimitiveComponent* PrimitiveComp = AllPrimitives[i];
        if (!PrimitiveComp) continue;

        FWorldAABBData Data;
        Data.Prim = PrimitiveComp;
        PrimitiveComp->GetWorldAABB(Data.Min, Data.Max);
        Data.Center = (Data.Min + Data.Max) * 0.5f;
        CachedAABBs.Add(Data);

        // 인덱스를 컴포넌트에 직접 캐시
        PrimitiveComp->CachedAABBIndex = i;
        PrimitiveComp->CachedFrame = Frame;
    }


    // 1. 오클루더 동적 선택
    ULevel* CurrentLevel = GWorld->GetLevel();
    TArray<UPrimitiveComponent*> OccluderCandidates = CurrentLevel->GetStaticOctree()->FindNearestPrimitives(CameraPos, static_cast<uint32>(AllPrimitives.Num()) / 10);
    TArray<UPrimitiveComponent*> SelectedOccluders = SelectOccluders(OccluderCandidates, CameraPos);

    // 2. CPU Z-Buffer 구성
    RasterizeOccluders(SelectedOccluders, CameraPos);

    // 3. 가시성 테스트
    VisibleMeshComponents.Empty();
    for (auto& AABBData : CachedAABBs)
    {
        if (IsMeshVisible(AABBData))
        {
            VisibleMeshComponents.Add(AABBData.Prim);
        }
    }

    return VisibleMeshComponents;
}

TArray<UPrimitiveComponent*> COcclusionCuller::SelectOccluders(const TArray<UPrimitiveComponent*>& Candidates, const FVector& CameraPos)
{
    FilteredOccluders.Empty();

    for (UPrimitiveComponent* Occluder : Candidates)
    {
        if (Occluder->CachedFrame != Frame)
        {
            continue;
        }
        FWorldAABBData& Data = CachedAABBs[Occluder->CachedAABBIndex];

        float AABB_Diagonal_LengthSq = FVector::DistSquared(Data.Min, Data.Max);
        float DistanceToOccluderSq = FVector::DistSquared(CameraPos, Data.Center);

        if (DistanceToOccluderSq < AABB_Diagonal_LengthSq)
        {
            continue;
        }

        FilteredOccluders.Add(Occluder);
    }
    return FilteredOccluders;
}

void COcclusionCuller::RasterizeOccluders(const TArray<UPrimitiveComponent*>& SelectedOccluders, const FVector& CameraPos)
{
    for (UPrimitiveComponent* OccluderComp : SelectedOccluders)
    {
        if (OccluderComp->CachedFrame != Frame)
        {
            continue;
        }
        // 1. AABB를 12개 삼각형의 월드 정점 리스트로 변환
        TArray<FVector> BoxTriangles = ConvertAABBToTriangles(OccluderComp);

        // 2. CPU 래스터라이징
        for (uint32 Idx = 0; Idx < BoxTriangles.Num(); Idx += 3)
        {
            // 삼각형의 세 정점 (월드 좌표)
            const FVector& P1_World = BoxTriangles[Idx];
            const FVector& P2_World = BoxTriangles[Idx + 1];
            const FVector& P3_World = BoxTriangles[Idx + 2];

            // 정점을 화면 좌표로 투영
            FVector P1_Screen = Project(P1_World);
            FVector P2_Screen = Project(P2_World);
            FVector P3_Screen = Project(P3_World);

            // Backface Culling
            FVector2 V1(P2_Screen.X - P1_Screen.X, P2_Screen.Y - P1_Screen.Y);
            FVector2 V2(P3_Screen.X - P1_Screen.X, P3_Screen.Y - P1_Screen.Y);

            // 2D 외적 (Z 성분만)
            float CrossZ = V1.X * V2.Y - V1.Y * V2.X;
            if (CrossZ < 0.0f)
            {
                continue;
            }

            // Z-Buffer에 깊이 쓰기
            RasterizeTriangle(P1_Screen, P2_Screen, P3_Screen, CPU_ZBuffer);
        }
    }
}

FVector COcclusionCuller::Project(const FVector& WorldPos) const
{
    FVector4 WorldPos4(WorldPos.X, WorldPos.Y, WorldPos.Z, 1.0f);
    FVector4 ClipPos = WorldPos4 * CurrentViewProj;

    if (ClipPos.W != 0.0f)
    {
        ClipPos.X /= ClipPos.W;
        ClipPos.Y /= ClipPos.W;
        ClipPos.Z /= ClipPos.W;
    }

    // NDC to Screen
    float ScreenX = (ClipPos.X + 1.0f) * 0.5f * Z_BUFFER_WIDTH;
    float ScreenY = (1.0f - ClipPos.Y) * 0.5f * Z_BUFFER_HEIGHT;
    float ScreenZ = ClipPos.Z;

    return {ScreenX, ScreenY, ScreenZ};
}

bool COcclusionCuller::IsMeshVisible(const FWorldAABBData& AABBData)
{
    const FVector& WorldMin = AABBData.Min;
    const FVector& WorldMax = AABBData.Max;
    const FVector& WorldCenter = AABBData.Center;

    constexpr int32 NumSamples = 19;
    FVector OriginalSamplesArray[NumSamples];
    int32 Index = 0; // 배열 인덱스 카운터

    // 1개 중심점
    OriginalSamplesArray[Index++] = WorldCenter;

    // 6개 면 중심
    OriginalSamplesArray[Index++] = FVector(WorldCenter.X, WorldMin.Y, WorldCenter.Z);
    OriginalSamplesArray[Index++] = FVector(WorldCenter.X, WorldMax.Y, WorldCenter.Z);
    OriginalSamplesArray[Index++] = FVector(WorldMin.X, WorldCenter.Y, WorldCenter.Z);
    OriginalSamplesArray[Index++] = FVector(WorldMax.X, WorldCenter.Y, WorldCenter.Z);
    OriginalSamplesArray[Index++] = FVector(WorldCenter.X, WorldCenter.Y, WorldMin.Z);
    OriginalSamplesArray[Index++] = FVector(WorldCenter.X, WorldCenter.Y, WorldMax.Z);

    // 12개 엣지 중심
    // X 엣지 (4개)
    OriginalSamplesArray[Index++] = FVector(WorldMin.X, WorldCenter.Y, WorldMin.Z);
    OriginalSamplesArray[Index++] = FVector(WorldMax.X, WorldCenter.Y, WorldMin.Z);
    OriginalSamplesArray[Index++] = FVector(WorldMin.X, WorldCenter.Y, WorldMax.Z);
    OriginalSamplesArray[Index++] = FVector(WorldMax.X, WorldCenter.Y, WorldMax.Z);

    // Y 엣지 (4개)
    OriginalSamplesArray[Index++] = FVector(WorldCenter.X, WorldMin.Y, WorldMin.Z);
    OriginalSamplesArray[Index++] = FVector(WorldCenter.X, WorldMax.Y, WorldMin.Z);
    OriginalSamplesArray[Index++] = FVector(WorldCenter.X, WorldMin.Y, WorldMax.Z);
    OriginalSamplesArray[Index++] = FVector(WorldCenter.X, WorldMax.Y, WorldMax.Z);

    // Z 엣지 (4개)
    OriginalSamplesArray[Index++] = FVector(WorldMin.X, WorldMin.Y, WorldCenter.Z);
    OriginalSamplesArray[Index++] = FVector(WorldMax.X, WorldMin.Y, WorldCenter.Z);
    OriginalSamplesArray[Index++] = FVector(WorldMin.X, WorldMax.Y, WorldCenter.Z);
    OriginalSamplesArray[Index++] = FVector(WorldMax.X, WorldMax.Y, WorldCenter.Z);

    // SIMD 배치 처리를 위해 20개로 패딩
    const int32 BatchCount = (NumSamples + 3) / 4;

    for (int32 i = 0; i < 19; ++i)
    {
        SampleCoords[0][i] = OriginalSamplesArray[i].X;  // X
        SampleCoords[1][i] = OriginalSamplesArray[i].Y;  // Y
        SampleCoords[2][i] = OriginalSamplesArray[i].Z;  // Z
    }

    // 패딩
    for (int32 i = 19; i < 20; ++i)
    {
        SampleCoords[0][i] = 1e30f;
        SampleCoords[1][i] = 1e30f;
        SampleCoords[2][i] = 1e30f;
    }

    constexpr float Z_TOLERANCE = 0.001f;
    for (int32 BatchIdx = 0; BatchIdx < BatchCount; ++BatchIdx)
    {
        BatchProjectionInput Input;
        Input.WorldX = _mm_load_ps(&SampleCoords[0][BatchIdx * 4]);
        Input.WorldY = _mm_load_ps(&SampleCoords[1][BatchIdx * 4]);
        Input.WorldZ = _mm_load_ps(&SampleCoords[2][BatchIdx * 4]);

        BatchProjectionResult ProjectionResult = BatchProject4(Input);

        // 멤버 배열에 저장
        _mm_store_si128((__m128i*)TempPixelCoords[0], ProjectionResult.PixelX);
        _mm_store_si128((__m128i*)TempPixelCoords[1], ProjectionResult.PixelY);
        _mm_store_ps(TempScreenZ, ProjectionResult.ScreenZ);

        for (int32 i = 0; i < 4; ++i)
        {
            int32 SampleIdx = BatchIdx * 4 + i;
            if (SampleIdx >= 19) break;

            int32 X = TempPixelCoords[0][i];  // PixelX
            int32 Y = TempPixelCoords[1][i];  // PixelY

            if (X >= 0 && X < Z_BUFFER_WIDTH && Y >= 0 && Y < Z_BUFFER_HEIGHT)
            {
                int32 Index = Y * Z_BUFFER_WIDTH + X;
                if (TempScreenZ[i] < CPU_ZBuffer[Index] + Z_TOLERANCE)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

BatchProjectionResult COcclusionCuller::BatchProject4(const BatchProjectionInput& Input) const
{    
    // 1. 4개 월드 좌표를 동질 좌표로 변환 (W = 1.0f)
    __m128 Ones = _mm_set1_ps(1.0f);

    // 2. ViewProjection 행렬 접근
    const float* MatrixData = (const float*)&CurrentViewProj;

    __m128 Row0 = _mm_load_ps(&MatrixData[0]);
    __m128 Row1 = _mm_load_ps(&MatrixData[4]);
    __m128 Row2 = _mm_load_ps(&MatrixData[8]);
    __m128 Row3 = _mm_load_ps(&MatrixData[12]);

    __m128 ClipX = _mm_add_ps(_mm_add_ps(_mm_add_ps(
        _mm_mul_ps(Input.WorldX, _mm_shuffle_ps(Row0, Row0, 0x00)),
        _mm_mul_ps(Input.WorldY, _mm_shuffle_ps(Row1, Row1, 0x00))),
        _mm_mul_ps(Input.WorldZ, _mm_shuffle_ps(Row2, Row2, 0x00))),
        _mm_mul_ps(Ones, _mm_shuffle_ps(Row3, Row3, 0x00)));

    __m128 ClipY = _mm_add_ps(_mm_add_ps(_mm_add_ps(
        _mm_mul_ps(Input.WorldX, _mm_shuffle_ps(Row0, Row0, 0x55)),
        _mm_mul_ps(Input.WorldY, _mm_shuffle_ps(Row1, Row1, 0x55))),
        _mm_mul_ps(Input.WorldZ, _mm_shuffle_ps(Row2, Row2, 0x55))),
        _mm_mul_ps(Ones, _mm_shuffle_ps(Row3, Row3, 0x55)));

    __m128 ClipZ = _mm_add_ps(_mm_add_ps(_mm_add_ps(
        _mm_mul_ps(Input.WorldX, _mm_shuffle_ps(Row0, Row0, 0xAA)),
        _mm_mul_ps(Input.WorldY, _mm_shuffle_ps(Row1, Row1, 0xAA))),
        _mm_mul_ps(Input.WorldZ, _mm_shuffle_ps(Row2, Row2, 0xAA))),
        _mm_mul_ps(Ones, _mm_shuffle_ps(Row3, Row3, 0xAA)));

    __m128 ClipW = _mm_add_ps(_mm_add_ps(_mm_add_ps(
        _mm_mul_ps(Input.WorldX, _mm_shuffle_ps(Row0, Row0, 0xFF)),
        _mm_mul_ps(Input.WorldY, _mm_shuffle_ps(Row1, Row1, 0xFF))),
        _mm_mul_ps(Input.WorldZ, _mm_shuffle_ps(Row2, Row2, 0xFF))),
        _mm_mul_ps(Ones, _mm_shuffle_ps(Row3, Row3, 0xFF)));

    // -----------------------------------------------------------
    // 3. Perspective divide 안전성 체크 및 수행
    // -----------------------------------------------------------

    // 3-1. W > 0 (프러스텀 앞)인 항목만 유효하게 만듦
    __m128 W_is_positive = _mm_cmpgt_ps(ClipW, _mm_set1_ps(1e-6f)); // W > 0.000001f

    // 3-2. 역수 (InvW) 계산
    __m128 SafeW = _mm_or_ps(ClipW, _mm_andnot_ps(W_is_positive, Ones));
    // W > 0이면 ClipW, 아니면 1.0f (0으로 나누는 것을 방지)

    __m128 InvW = _mm_div_ps(Ones, SafeW);

    // 3-3. W <= 0 (Near Plane 뒤)인 항목은 InvW를 0으로 마스킹
    InvW = _mm_and_ps(InvW, W_is_positive);

    // 역투영
    ClipX = _mm_mul_ps(ClipX, InvW);
    ClipY = _mm_mul_ps(ClipY, InvW);
    ClipZ = _mm_mul_ps(ClipZ, InvW);
    // W <= 0인 좌표는 ClipX/Y/Z = 0이 되므로, NDC 밖(-1 또는 1)으로 튀어나가지 않음

    // -----------------------------------------------------------
    // 4. NDC to Screen coordinates (4개 동시)
    // -----------------------------------------------------------
    __m128 HalfWidth = _mm_set1_ps(Z_BUFFER_WIDTH * 0.5f);
    __m128 HalfHeight = _mm_set1_ps(Z_BUFFER_HEIGHT * 0.5f);

    BatchProjectionResult Result;
    // X: (ClipX + 1.0) * 0.5 * Width
    Result.ScreenX = _mm_mul_ps(_mm_add_ps(ClipX, Ones), HalfWidth);
    // Y: (1.0 - ClipY) * 0.5 * Height
    Result.ScreenY = _mm_mul_ps(_mm_sub_ps(Ones, ClipY), HalfHeight);
    Result.ScreenZ = ClipZ; // ScreenZ (NDC Z)

    // 5. 정수형 픽셀 좌표로 변환
    // _mm_cvtps_epi32는 기본적으로 TRUNCATE (버림)을 수행
    Result.PixelX = _mm_cvtps_epi32(Result.ScreenX);
    Result.PixelY = _mm_cvtps_epi32(Result.ScreenY);

    return Result;
}

void COcclusionCuller::RasterizeTriangle(const FVector& P1, const FVector& P2, const FVector& P3, TArray<float>& ZBuffer)
{        
    // 1. 바운딩 박스 계산
    int32 MinX = max(0, (int32)min({ P1.X, P2.X, P3.X }));
    int32 MaxX = min(Z_BUFFER_WIDTH - 1, (int32)max({ P1.X, P2.X, P3.X }));
    int32 MinY = max(0, (int32)min({ P1.Y, P2.Y, P3.Y }));
    int32 MaxY = min(Z_BUFFER_HEIGHT - 1, (int32)max({ P1.Y, P2.Y, P3.Y }));

    // 3. 삼각형 면적 계산 (degenerate triangle 체크용)
    float Area = 0.5f * abs((P2.X - P1.X) * (P3.Y - P1.Y) - (P3.X - P1.X) * (P2.Y - P1.Y));
    if (Area < 0.001f) { return; }

    // 4. Barycentric 좌표 계산을 위한 분모 미리 계산 (최적화)
    float Denom1 = (P2.X - P3.X) * (P1.Y - P3.Y) - (P2.Y - P3.Y) * (P1.X - P3.X);
    float Denom2 = (P3.X - P1.X) * (P2.Y - P1.Y) - (P3.Y - P1.Y) * (P2.X - P1.X);

    // 5. 분모가 0에 가까우면 degenerate triangle
    if (abs(Denom1) < 0.0001f || abs(Denom2) < 0.0001f) { return; }

    // 6. 역수 미리 계산
    float InvDenom1 = 1.0f / Denom1;
    float InvDenom2 = 1.0f / Denom2;

    // 7. 바운딩 박스 내의 모든 픽셀에 대해 검사
    for (int32 Y = MinY; Y <= MaxY; ++Y)
    {
        for (int32 X = MinX; X <= MaxX; ++X)
        {
            // 픽셀 중심점
            float PixelX = X + 0.5f;
            float PixelY = Y + 0.5f;

            // Barycentric coordinates 계산 (최적화된 버전)
            float W1 = ((P2.X - P3.X) * (PixelY - P3.Y) - (P2.Y - P3.Y) * (PixelX - P3.X)) * InvDenom1;
            float W2 = ((P3.X - P1.X) * (PixelY - P1.Y) - (P3.Y - P1.Y) * (PixelX - P1.X)) * InvDenom2;
            float W3 = 1.0f - W1 - W2;

            const float EPSILON = -0.0001f; // 경계 케이스 처리
            if (W1 >= EPSILON && W2 >= EPSILON && W3 >= EPSILON)
            {
                // 깊이값 보간
                float InterpolatedZ = W1 * P1.Z + W2 * P2.Z + W3 * P3.Z;

                int32 Index = Y * Z_BUFFER_WIDTH + X;

                // 깊이 테스트 및 업데이트
                if (InterpolatedZ < ZBuffer[Index])
                {
                    ZBuffer[Index] = InterpolatedZ;
                }
            }
        }
    }
}

TArray<FVector> COcclusionCuller::ConvertAABBToTriangles(UPrimitiveComponent* Prim)
{
    Triangles.Empty();

    if (!Prim) { return Triangles; }

    FWorldAABBData& Data = CachedAABBs[Prim->CachedAABBIndex];
    const FVector& WorldCenter = Data.Center;
    FVector Extent = (Data.Min - Data.Max) * 0.5f;

    constexpr float OccluderScale = 0.5f;

    FVector WorldMin = WorldCenter - Extent * OccluderScale;
    FVector WorldMax = WorldCenter + Extent * OccluderScale;

    // AABB의 8개 정점 계산
    FVector Vertices[8];
    Vertices[0] = FVector(WorldMin.X, WorldMin.Y, WorldMin.Z); // 000
    Vertices[1] = FVector(WorldMax.X, WorldMin.Y, WorldMin.Z); // 100
    Vertices[2] = FVector(WorldMax.X, WorldMax.Y, WorldMin.Z); // 110
    Vertices[3] = FVector(WorldMin.X, WorldMax.Y, WorldMin.Z); // 010
    Vertices[4] = FVector(WorldMin.X, WorldMin.Y, WorldMax.Z); // 001
    Vertices[5] = FVector(WorldMax.X, WorldMin.Y, WorldMax.Z); // 101
    Vertices[6] = FVector(WorldMax.X, WorldMax.Y, WorldMax.Z); // 111
    Vertices[7] = FVector(WorldMin.X, WorldMax.Y, WorldMax.Z); // 011

    // Front face (Z = Min)
    Triangles.Add(Vertices[0]); Triangles.Add(Vertices[1]); Triangles.Add(Vertices[2]);
    Triangles.Add(Vertices[0]); Triangles.Add(Vertices[2]); Triangles.Add(Vertices[3]);

    // Back face (Z = Max)
    Triangles.Add(Vertices[5]); Triangles.Add(Vertices[4]); Triangles.Add(Vertices[7]);
    Triangles.Add(Vertices[5]); Triangles.Add(Vertices[7]); Triangles.Add(Vertices[6]);

    // Left face (X = Min)
    Triangles.Add(Vertices[4]); Triangles.Add(Vertices[0]); Triangles.Add(Vertices[3]);
    Triangles.Add(Vertices[4]); Triangles.Add(Vertices[3]); Triangles.Add(Vertices[7]);

    // Right face (X = Max)
    Triangles.Add(Vertices[1]); Triangles.Add(Vertices[5]); Triangles.Add(Vertices[6]);
    Triangles.Add(Vertices[1]); Triangles.Add(Vertices[6]); Triangles.Add(Vertices[2]);

    // Bottom face (Y = Min)
    Triangles.Add(Vertices[4]); Triangles.Add(Vertices[5]); Triangles.Add(Vertices[1]);
    Triangles.Add(Vertices[4]); Triangles.Add(Vertices[1]); Triangles.Add(Vertices[0]);

    // Top face (Y = Max)
    Triangles.Add(Vertices[3]); Triangles.Add(Vertices[2]); Triangles.Add(Vertices[6]);
    Triangles.Add(Vertices[3]); Triangles.Add(Vertices[6]); Triangles.Add(Vertices[7]);

    return Triangles;
}
