#pragma once
#include "pch.h"
#include "Physics/Public/AABB.h"

class UPrimitiveComponent;
struct FStaticMesh;

struct FNode
{
	int32 ObjectIndex;
	int32 ParentIndex;
	int32 Child1;
	int32 Child2;
	bool bIsLeaf;
	FAABB Box;
	int32 TriangleBaseIndex; // �ε��� ���ۿ��� �ﰢ���� ���� �ε���
};

//  Phase Picking에 사용되는 BVH (Bounding Volume Hierarchy)
class FBVH
{
public:
	FBVH() = default;
	explicit FBVH(FStaticMesh* InMesh);

	void Build(FStaticMesh* InMesh);
	int32 GetRootIndex() const { return RootIndex; }
	int32 GetNodeCount() const { return Nodes.Num(); }
	const FNode& GetNode(uint32 Index) const;
	FNode& GetNode(uint32 Index);
	void Clear();

	/**
	* @brief 서브트리의 cost(노드가 가진 AABB의 표면적 합)을 계산.
	* @param SubTreeRootIndex: cost 계산 시작 노드 인덱스
	* @param bInternalOnly: true로 설정하면 leaf의 코스트는 포함 안시킴
	* @return cost 값
	*/
	float GetCost(int32 SubTreeRootIndex, bool bInternalOnly = false) const;

	/**
	* @brief: 트리의 유효성 검사.
	*/
	bool CheckValidity() const;

	/**
	* @brief: Ray와 BVH를 순회하여 교차하는 삼각형들의 인덱스 리스트를 반환
	* @param Ray: 교차 검사를 수행할 Ray (Local 좌표계)
	* @param OutTriangleIndices: 교차하는 삼각형들의 base index 리스트 (output)
	* @return: 교차하는 삼각형이 있으면 true, 없으면 false
	*/
	bool TraverseRay(const FRay& Ray, TArray<int32>& OutTriangleIndices) const;

	/**
	* @brief: 새 리프 노드를 특정 노드의 형제로 추가했을 때 전체 뉱업 트리의 비용 증가량 계산
	* @param CandidateIndex: 후보 형제 노드 인덱스
	* @param NewLeafAABB: 새로운 리프 노드의 AABB
	* @return: 비용 증가량
	*/
	float CalculateCostIncrease(int32 CandidateIndex, const FAABB& NewLeafAABB) const;

private:
	/**
	* @brief 새로운 leaf node를 삽입.
	* @note cost가 가장 낮아지는 최적의 sibling node를 찾아서 삽입되도록 함.
	* @return 삽입된 leaf node의 인덱스, 실패 시 -1 반환
	*/
	int32 InsertLeaf(int32 InTriangleBaseIndex);

	// --- 새 leaf node 삽입 과정 보조 메소드들 ---

	//@brief 새로운 leaf node가 추가되었을 때 cost가 가장 조금 증가하는 sibling node 탐색.
	int32 FindBestSibling(const FAABB& NewLeafAABB);
	//@brief 새로운 leaf node와 기존 sibling node를 묶는 internal node를 생성하고 트리에 삽입.
	void InsertInternalNode(int32 LeafIndex, int32 SiblingIndex);
	//@brief 주어진 노드의 '부모'부터 루트까지 올라가며 AABB Refit 수행.
	void RefitAncestors(int32 RefitStartIndex);

	FStaticMesh* Mesh = nullptr; // BVH 원본 메시
	TArray<FNode> Nodes;
	int32 RootIndex = -1;
	float Cost = 0.0f;
};

FAABB GetTriangleAABB(const FNormalVertex& V0, const FNormalVertex& V1, const FNormalVertex& V2);