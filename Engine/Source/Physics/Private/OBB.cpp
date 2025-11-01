#include "pch.h"
#include "Physics/Public/OBB.h"

#include "Physics/Public/AABB.h"
#include "Physics/Public/HitResult.h"

bool FOBB::Intersects(const FAABB& Other) const
{
    FVector Center = Other.GetCenter();
    FVector Extents = (Other.Max - Other.Min) * 0.5f;
    FOBB OBB(Center, Extents, FMatrix::Identity());
    return Intersects(OBB);
}

bool FOBB::Intersects(const FOBB& Other, FHitResult* OutHit) const
{
    const FVector AxisLhs[] = {
        FVector(ScaleRotation.Data[0][0], ScaleRotation.Data[0][1], ScaleRotation.Data[0][2]),
        FVector(ScaleRotation.Data[1][0], ScaleRotation.Data[1][1], ScaleRotation.Data[1][2]),
        FVector(ScaleRotation.Data[2][0], ScaleRotation.Data[2][1], ScaleRotation.Data[2][2])
    };

    const FVector AxisRhs[] = {
        FVector(Other.ScaleRotation.Data[0][0], Other.ScaleRotation.Data[0][1], Other.ScaleRotation.Data[0][2]),
        FVector(Other.ScaleRotation.Data[1][0], Other.ScaleRotation.Data[1][1], Other.ScaleRotation.Data[1][2]),
        FVector(Other.ScaleRotation.Data[2][0], Other.ScaleRotation.Data[2][1], Other.ScaleRotation.Data[2][2])
    };

    FVector TestAxis[15];

    size_t Count = 0;

    TestAxis[Count++] = AxisLhs[0];
    TestAxis[Count++] = AxisLhs[1];
    TestAxis[Count++] = AxisLhs[2];

    TestAxis[Count++] = AxisRhs[0];
    TestAxis[Count++] = AxisRhs[1];
    TestAxis[Count++] = AxisRhs[2];

    for (size_t i = 0; i < 3; ++i)
    {
        for (size_t j = 0; j < 3; ++j)
        {
            TestAxis[Count] = AxisRhs[j].Cross(AxisLhs[i]);
            if (TestAxis[Count].LengthSquared() > DBL_EPSILON)
            {
                ++Count;
            }
        }
    }

    const FVector Diff = Other.Center - Center;

    // MTD tracking (only if OutHit is requested)
    float MinPenetration = FLT_MAX;
    FVector MTDAxis;

    for (size_t i = 0; i < Count; ++i)
    {
        float ProjectedDist = abs(Diff.Dot(TestAxis[i]));

        float ProjectedRadiusLhs =
            Extents.X * abs(AxisLhs[0].Dot(TestAxis[i])) +
            Extents.Y * abs(AxisLhs[1].Dot(TestAxis[i])) +
            Extents.Z * abs(AxisLhs[2].Dot(TestAxis[i]));

        float ProjectedRadiusRhs =
            Other.Extents.X * abs(AxisRhs[0].Dot(TestAxis[i])) +
            Other.Extents.Y * abs(AxisRhs[1].Dot(TestAxis[i])) +
            Other.Extents.Z * abs(AxisRhs[2].Dot(TestAxis[i]));

        float TotalRadius = ProjectedRadiusLhs + ProjectedRadiusRhs;

        // Separating axis found - no collision
        if (ProjectedDist > TotalRadius)
        {
            return false;
        }

        // Track minimum penetration for MTD (only if OutHit requested)
        if (OutHit)
        {
            float Penetration = TotalRadius - ProjectedDist;
            if (Penetration < MinPenetration)
            {
                MinPenetration = Penetration;
                MTDAxis = TestAxis[i];
            }
        }
    }

    // Fill HitResult if requested
    if (OutHit)
    {
        // Ensure MTD axis points from this to Other (push Other away from this)
        if (Diff.Dot(MTDAxis) < 0)
        {
            MTDAxis = MTDAxis * -1.0f;
        }

        OutHit->Normal = MTDAxis.GetNormalized();
        OutHit->PenetrationDepth = MinPenetration;
        OutHit->bBlockingHit = true;
        OutHit->Location = Center + MTDAxis.GetNormalized() * (MinPenetration * 0.5f);
    }

    return true;
}

bool FOBB::Intersects(const FOBB& Other) const
{
    // Delegate to detailed version without hit info
    return Intersects(Other, nullptr);
}

void FOBB::Update(const FMatrix& WorldMatrix)
{
    Center = WorldMatrix.GetLocation();
    ScaleRotation = WorldMatrix;
    ScaleRotation.Data[3][0] = ScaleRotation.Data[3][1] = ScaleRotation.Data[3][2] = 0;
}

FAABB FOBB::ToWorldAABB() const
{
    FVector Corners[8];
    Corners[0] = FVector(-Extents.X, -Extents.Y, -Extents.Z);
    Corners[1] = FVector( Extents.X, -Extents.Y, -Extents.Z);
    Corners[2] = FVector(-Extents.X,  Extents.Y, -Extents.Z);
    Corners[3] = FVector( Extents.X,  Extents.Y, -Extents.Z);
    Corners[4] = FVector(-Extents.X, -Extents.Y,  Extents.Z);
    Corners[5] = FVector( Extents.X, -Extents.Y,  Extents.Z);
    Corners[6] = FVector(-Extents.X,  Extents.Y,  Extents.Z);
    Corners[7] = FVector( Extents.X,  Extents.Y,  Extents.Z);

    FMatrix ObbTransform = ScaleRotation * FMatrix::TranslationMatrix(Center);

    FVector NewMin(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector NewMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (int i = 0; i < 8; ++i)
    {
        FVector TransformedCorner = ObbTransform.TransformPosition(Corners[i]);
        NewMin.X = std::min(NewMin.X, TransformedCorner.X);
        NewMin.Y = std::min(NewMin.Y, TransformedCorner.Y);
        NewMin.Z = std::min(NewMin.Z, TransformedCorner.Z);
        NewMax.X = std::max(NewMax.X, TransformedCorner.X);
        NewMax.Y = std::max(NewMax.Y, TransformedCorner.Y);
        NewMax.Z = std::max(NewMax.Z, TransformedCorner.Z);
    }

    return FAABB(NewMin, NewMax);
}
