#pragma once
#include "Global/Types.h"
#include "Global/CoreTypes.h"
#include "Editor/Public/EditorPrimitive.h"
#include "Editor/Public/Grid.h"
#include "Editor/Public/BoundingBoxLines.h"

struct FVertex;
class FOctree;
class UDecalSpotLightComponent;

class UBatchLines : UObject
{
	DECLARE_CLASS(UBatchLines, UObject)
public:
	UBatchLines();
	~UBatchLines();

	// 종류별 Vertices 업데이트
	void UpdateUGridVertices(const float newCellSize);
	void UpdateBoundingBoxVertices(const IBoundingVolume* NewBoundingVolume);
	void UpdateOctreeVertices(const FOctree* InOctree);
	// Decal SpotLight용 불법 증축
	void UpdateDecalSpotLightVertices(UDecalSpotLightComponent* SpotLightComponent);
	void UpdateConeVertices(const FVector& InCenter, float InGeneratingLineLength
		, float InOuterHalfAngleRad, float InInnerHalfAngleRad, FQuaternion InRotation);
	// GPU VertexBuffer에 복사
	void UpdateVertexBuffer();

	float GetCellSize() const
	{
		return Grid.GetCellSize();
	}

	void DisableRenderBoundingBox()
	{
		UpdateBoundingBoxVertices(BoundingBoxLines.GetDisabledBoundingBox());
		bRenderSpotLight = false;
	}

	void ClearOctreeLines()
	{
		OctreeLines.clear();
		bChangedVertices = true;
	}

	//void UpdateConstant(FBoundingBox boundingBoxInfo);

	//void Update();

	/**
	 * @brief 모든 BatchLines 렌더링 (Grid, AABB, Light Lines, Octree)
	 * @note 내부에서 ShowFlags 체크 및 선택 상태에 따라 적절히 렌더링
	 */
	void Render();

private:
	void RenderGridAndLightLines();  // Grid + Light Lines
	void RenderBoundingBox();        // AABB
	void RenderOctree();             // Octree
	void SetIndices();

	void TraverseOctree(const FOctree* InNode);

	/*void AddWorldGridVerticesAndConstData();
	void AddBoundingBoxVertices();*/

	bool bChangedVertices = false;

	TArray<FVector> Vertices; // 그리드 라인 정보 + (offset 후)디폴트 바운딩 박스 라인 정보(minx, miny가 0,0에 정의된 크기가 1인 cube)
	TArray<uint32> Indices; // 월드 그리드는 그냥 정점 순서, 바운딩 박스는 실제 인덱싱

	FEditorPrimitive Primitive;

	UGrid Grid;
	UBoundingBoxLines BoundingBoxLines;
	UBoundingBoxLines SpotLightLines;
	TArray<UBoundingBoxLines> OctreeLines;

	bool bRenderBox;
	bool bRenderSpotLight = false;
};

