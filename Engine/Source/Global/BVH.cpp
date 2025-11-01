#include "pch.h"
#include "Global/BVH.h"

#include "Component/Mesh/Public/StaticMesh.h"

FBVH::FBVH(FStaticMesh* InMesh)
{
	Build(InMesh);
}

const FNode& FBVH::GetNode(uint32 Index) const
{
	assert(Index < Nodes.Num());
	return Nodes[Index];
}

FNode& FBVH::GetNode(uint32 Index)
{
	assert(Index < Nodes.Num());
	return Nodes[Index];
}

void FBVH::Clear()
{
	Mesh = nullptr;
	Nodes.Empty();
	RootIndex = -1;
	Cost = 0.0f;
}

int32 FBVH::InsertLeaf(int32 InTriangleBaseIndex)
{
	if(!Mesh)
	{
		std::cerr << "FBVH::InsertLeaf: Mesh is null. Cannot insert leaf node." << std::endl;
		return -1;
	}

	if(InTriangleBaseIndex < 0 || InTriangleBaseIndex + 2 >= Mesh->Indices.Num() || InTriangleBaseIndex % 3 != 0)
	{
		std::cerr << "FBVH::InsertLeaf: Invalid triangle base index." << std::endl;
		return -1;
	}

	// 1. 새 Leaf node의 AABB 계산
	FNormalVertex V0 = Mesh->Vertices[Mesh->Indices[InTriangleBaseIndex]];
	FNormalVertex V1 = Mesh->Vertices[Mesh->Indices[InTriangleBaseIndex + 1]];
	FNormalVertex V2 = Mesh->Vertices[Mesh->Indices[InTriangleBaseIndex + 2]];
	FAABB LeafAABB = GetTriangleAABB(V0, V1, V2);

	// 2. 새 Leaf node 생성
	FNode NewNode;
	NewNode.ObjectIndex = Nodes.Num();
	NewNode.ParentIndex = -1; // Parent는 InsertInternalNode에서 설정
	NewNode.Child1 = -1;
	NewNode.Child2 = -1;
	NewNode.bIsLeaf = true;
	NewNode.Box = LeafAABB;
	NewNode.TriangleBaseIndex = InTriangleBaseIndex;
	// 빈 트리인 경우 새 노드를 루트로 설정하고 종료
	if (NewNode.ObjectIndex == 0)
	{
		Nodes.Add(NewNode);
		RootIndex = 0;
		Cost = GetCost(RootIndex);
		return 0;
	}

	// 3. 새 Leaf node를 삽입할 최적의 sibling node 찾기;
	int32 SiblingIndex = FindBestSibling(NewNode.Box);

	// 4. 새 leaf node와 sibling의 새로운 부모 node 생성
	Nodes.Add(NewNode);
	InsertInternalNode(NewNode.ObjectIndex, SiblingIndex);

	// 5. 새 node가 추가되었으므로 부모 거슬러올라가며 AABB 리피팅
	// NewNode의 부모가 InsertInternalNode에서 설정했고, 부모의 AABB도 끝내둠.
	// 리핏은 ParentIndex부터 시작
	RefitAncestors(Nodes[SiblingIndex].ParentIndex);

	return NewNode.ObjectIndex;
}

float FBVH::GetCost(int32 SubTreeRootIndex, bool bInternalOnly) const
{
	// 인덱스 벗어난 경우 0 반환
	if (SubTreeRootIndex >= Nodes.Num() || SubTreeRootIndex < 0)
	{
		return 0.0f;
	}

	const FNode& SubTreeRoot = Nodes[SubTreeRootIndex];
	if (SubTreeRoot.bIsLeaf)
	{
		// InternalOnly면 leaf node의 cost는 0으로 계산
		// 새 노드 삽입시 최적 위치 계산하는 경우는 leaf node의 cost가 상수이기 때문
		if (bInternalOnly)
		{
			return 0.0f;
		}

		return SubTreeRoot.Box.GetSurfaceArea();
	}

	return SubTreeRoot.Box.GetSurfaceArea() + GetCost(SubTreeRoot.Child1, bInternalOnly) + GetCost(SubTreeRoot.Child2, bInternalOnly);
}

int32 FBVH::FindBestSibling(const FAABB& NewLeafAABB)
{
	int32 BestSiblingIndex = -1;
	float MinCostIncrease = FLT_MAX;

	// Branch and Bound 최적화를 위한 스택 기반 순회
	// 루트부터 시작하여 더 나은 후보가 있을 수 있는 노드들만 방문
	TArray<int32> CandidateStack;
	CandidateStack.Add(RootIndex);

	while (!CandidateStack.IsEmpty())
	{
		int32 CurrentIndex = CandidateStack.Last();
		CandidateStack.Pop();

		if (CurrentIndex < 0 || CurrentIndex >= Nodes.Num())
		{
			continue;
		}

		const FNode& CurrentNode = Nodes[CurrentIndex];
		FAABB CombinedAABB = Union(NewLeafAABB, CurrentNode.Box);

		// 하한 비용 계산: 이 서브트리의 어떤 리프를 선택하더라도 최소한 발생하는 '증가' 비용
		float DirectIncrease = CombinedAABB.GetSurfaceArea() - CurrentNode.Box.GetSurfaceArea();
		float InheritedIncrease = 0.0f;
		int32 AncestorIndex = CurrentNode.ParentIndex;
		int32 CurrentChildIndex = CurrentIndex;
		FAABB CurrentAABB = CombinedAABB;
		while(AncestorIndex != -1)
		{
			const FNode& AncestorNode = Nodes[AncestorIndex];
			int32 SiblingOfCurrentChildIndex = (AncestorNode.Child1 == CurrentChildIndex)
				? AncestorNode.Child2
				: AncestorNode.Child1;
			const FNode& SiblingOfCurrentChild = Nodes[SiblingOfCurrentChildIndex];

			FAABB AncestorNewAABB = Union(SiblingOfCurrentChild.Box, CurrentAABB);
			InheritedIncrease += AncestorNewAABB.GetSurfaceArea() - AncestorNode.Box.GetSurfaceArea();
			AncestorIndex = AncestorNode.ParentIndex;
			CurrentChildIndex = AncestorNode.ObjectIndex;
			CurrentAABB = AncestorNewAABB;
		}
		float LowerBoundIncrease = DirectIncrease + InheritedIncrease;

		// 하한 비용이 현재 최적 증가 비용 이상이면 이 서브트리에서는 더 나은 해를 찾을 수 없으므로 탐색 중단
		if (LowerBoundIncrease >= MinCostIncrease)
		{
			continue; // 조기 포기
		}

		if (CurrentNode.bIsLeaf)
		{
			// 리프 노드인 경우: 전체 비용 증가 계산
			float TotalCostIncrease = CalculateCostIncrease(CurrentIndex, NewLeafAABB);

			if (TotalCostIncrease < MinCostIncrease)
			{
				MinCostIncrease = TotalCostIncrease;
				BestSiblingIndex = CurrentIndex;
			}
		}
		else
		{
			// 내부 노드인 경우: 자식들을 스택에 추가
			// 하지만 Lower Bound가 현재 최적해보다 좋을 때만

			if (LowerBoundIncrease < MinCostIncrease)
			{
				// 자식 노드들을 후보에 추가
				if (CurrentNode.Child1 >= 0 && CurrentNode.Child1 < Nodes.Num())
				{
					CandidateStack.Add(CurrentNode.Child1);
				}
				if (CurrentNode.Child2 >= 0 && CurrentNode.Child2 < Nodes.Num())
				{
					CandidateStack.Add(CurrentNode.Child2);
				}
			}
			// else: 이 서브트리는 더 이상 탐색하지 않음 (Bound)
		}
	}

	return BestSiblingIndex;
}

float FBVH::CalculateCostIncrease(int32 CandidateIndex, const FAABB& NewLeafAABB) const
{
	if (CandidateIndex < 0 || CandidateIndex >= Nodes.Num())
	{
		return FLT_MAX;
	}

	const FNode& CandidateNode = Nodes[CandidateIndex];
	const FAABB& SiblingAABB = CandidateNode.Box;

	// 새 리프와 sibling을 묶는 새 internal node의 AABB 계산
	FAABB CombinedAABB = Union(NewLeafAABB, SiblingAABB);

	// Cost 증가 계산: 새 internal node의 surface area
	// (기존 sibling의 cost는 leaf이므로 변화 없음)
	float CostIncrease = CombinedAABB.GetSurfaceArea();

	// Sibling이 가지고 있던 부모 노드가 있다면
	// 부모부터 루트까지 올라가며 추가 cost 계산
	int32 AncestorIndex = CandidateNode.ParentIndex;
	int32 CurrentChildIndex = CandidateIndex;
	FAABB CurrentAABB = CombinedAABB;
	while (AncestorIndex != -1 && AncestorIndex < Nodes.Num())
	{
		const FNode& AncestorNode = Nodes[AncestorIndex];

		// 형제 노드의 AABB와 합쳐서 새로운 조상 AABB 계산
		int32 SiblingOfCurrentChildIndex = (AncestorNode.Child1 == CurrentChildIndex)
			? AncestorNode.Child2
			: AncestorNode.Child1;

		if (SiblingOfCurrentChildIndex >= 0 && SiblingOfCurrentChildIndex < Nodes.Num())
		{
			const FAABB& SiblingOfCurrentChild = Nodes[SiblingOfCurrentChildIndex].Box;
			FAABB NewAncestorAABB = Union(CurrentAABB, SiblingOfCurrentChild);
			CostIncrease += NewAncestorAABB.GetSurfaceArea() - AncestorNode.Box.GetSurfaceArea();
			CurrentAABB = NewAncestorAABB;
		}
		
		CurrentChildIndex = AncestorIndex;
		AncestorIndex = AncestorNode.ParentIndex;
	}

	return CostIncrease;
}

void FBVH::InsertInternalNode(int32 NewLeafIndex, int32 SiblingIndex)
{
	// 새 부모(Internal) 노드 구성
	FNode NewParent;
	int32 OldParentIndex = Nodes[SiblingIndex].ParentIndex;
	NewParent.ParentIndex = OldParentIndex;
	NewParent.Child1 = SiblingIndex;
	NewParent.Child2 = NewLeafIndex;
	NewParent.bIsLeaf = false;
	NewParent.TriangleBaseIndex = -1; // Internal 노드는 삼각형 인덱스 없음

	// 부모 AABB는 두 자식의 합집합
	FAABB NewLeafAABB = Nodes[NewLeafIndex].Box;
	FAABB SiblingAABB = Nodes[SiblingIndex].Box;
	NewParent.Box = Union(NewLeafAABB, SiblingAABB);

	// 새 부모 노드를 push하고 인덱스 확정
	int32 NewParentIndex = Nodes.Num();
	NewParent.ObjectIndex = NewParentIndex;
	Nodes.Add(NewParent);

	// 자식들의 부모 갱신
	Nodes[SiblingIndex].ParentIndex = NewParentIndex;
	Nodes[NewLeafIndex].ParentIndex = NewParentIndex;

	// 이전 부모가 있었다면 그 부모의 자식 포인터를 새 부모로 교체
	if (OldParentIndex != -1)
	{
		FNode& OldParent = Nodes[OldParentIndex];
		if (OldParent.Child1 == SiblingIndex)
		{
			OldParent.Child1 = NewParentIndex;
		}
		else if (OldParent.Child2 == SiblingIndex)
		{
			OldParent.Child2 = NewParentIndex;
		}
		else
		{
			// 혹시 모를 오류 (OldParent가 SiblingIndex를 자식으로 가지고 있지 않는 경우) 대비
			throw std::runtime_error("OldParent does not reference SiblingIndex as a child.");
		}
	}
	// sibling이 루트였던 경우, 새 부모가 루트가 됨
	else
	{
		RootIndex = NewParentIndex;
	}
}

void FBVH::RefitAncestors(int32 RefitStartIndex)
{
	// 시작점의 부모부터 루트까지 올라가며 InternalBox를 리핏
	if (RefitStartIndex < 0 || RefitStartIndex >= Nodes.Num())
	{
		return;
	}

	int32 CurrentIndex = Nodes[RefitStartIndex].ParentIndex; // 부모부터 시작
	while (CurrentIndex != -1)
	{
		FNode& Current = Nodes[CurrentIndex];
		// 두 자식의 AABB 합집합으로 현재 InternalBox 갱신
		FAABB Child1AABB = Nodes[Current.Child1].Box;
		FAABB Child2AABB = Nodes[Current.Child2].Box;
		Current.Box = Union(Child1AABB, Child2AABB);

		CurrentIndex = Current.ParentIndex;
	}

	// 전체 비용 갱신
	Cost = GetCost(RootIndex);
}

bool FBVH::CheckValidity() const
{
	// 1. 루트의 인덱스가 유효한지 확인
	if ((RootIndex < 0 && !Nodes.IsEmpty()) || RootIndex >= static_cast<int32>(Nodes.Num()))
	{
		return false;
	}

	// 2. 비어있는 트리의 경우 값이 정상적인지 확인
	if (Nodes.IsEmpty())
	{
		if (RootIndex != -1 || Cost != 0.0f)
		{
			return false;
		}
	}

	// 3. 각 노드의 부모-자식 관계가 일관적인지 확인
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		const FNode& Node = Nodes[i];
		if (Node.bIsLeaf) // Leaf node 인 경우 자식이 없어야 함
		{
			if (Node.Child1 != -1 || Node.Child2 != -1)
			{
				return false;
			}
			if (Node.TriangleBaseIndex < 0 || Node.TriangleBaseIndex % 3 != 0)
			{
				return false; // 리프 노드는 반드시 유효한 인덱스 배열의 인덱스를 가져야 함
			}
		}
		else // Internal Node 인 경우 자식이 있어야 함
		{
			if (Node.Child1 == -1 || Node.Child2 == -1)
			{
				return false;
			}
			if (Node.TriangleBaseIndex != -1)
			{
				return false; // 내부 노드는 인덱스를 가져선 안됨
			}
			// 자식 노드들이 올바른 부모 인덱스를 가리키는지 확인
			if (Node.Child1 < 0 || Node.Child1 >= Nodes.Num() ||
				Node.Child2 < 0 || Node.Child2 >= Nodes.Num())
			{
				return false;
			}
			const FNode& Child1 = Nodes[Node.Child1];
			const FNode& Child2 = Nodes[Node.Child2];
			if (Child1.ParentIndex != i || Child2.ParentIndex != i)
			{
				return false;
			}
		}

		// 부모 인덱스가 유효한지 확인 (루트는 제외)
		if (Node.ParentIndex != -1)
		{
			if (Node.ParentIndex < 0 || Node.ParentIndex >= Nodes.Num())
			{
				return false;
			}
		}
		else if (i != RootIndex) // 루트가 아닌데 부모가 없는 경우
		{
			return false;
		}
	}
	return true;
}

FAABB GetTriangleAABB(const FNormalVertex& V0, const FNormalVertex& V1, const FNormalVertex& V2)
{
	FVector Min{
		std::min({V0.Position.X, V1.Position.X, V2.Position.X}),
		std::min({V0.Position.Y, V1.Position.Y, V2.Position.Y}),
		std::min({V0.Position.Z, V1.Position.Z, V2.Position.Z})
	};
	FVector Max{
		std::max({V0.Position.X, V1.Position.X, V2.Position.X}),
		std::max({V0.Position.Y, V1.Position.Y, V2.Position.Y}),
		std::max({V0.Position.Z, V1.Position.Z, V2.Position.Z})
	};
	return FAABB(Min, Max);
}

bool FBVH::TraverseRay(const FRay& Ray, TArray<int32>& OutTriangleIndices) const
{
	OutTriangleIndices.Empty();
	
	// 빈 트리이거나 루트가 유효하지 않은 경우
	if (RootIndex < 0 || RootIndex >= Nodes.Num())
	{
		return false; // Traverse failed
	}
	
	// 스택을 사용한 반복적 순회로 구현 (재귀보다 성능상 유리)
	TArray<int32> NodeStack;
	NodeStack.Add(RootIndex);
	
	while (!NodeStack.IsEmpty())
	{
		int32 CurrentNodeIndex = NodeStack.Last();
		NodeStack.Pop();
		
		const FNode& CurrentNode = Nodes[CurrentNodeIndex];
		
		// Ray와 현재 노드의 AABB 교차 검사
		if (!CheckIntersectionRayBox(Ray, CurrentNode.Box))
		{
			continue; // AABB와 교차하지 않으면 이 노드의 자식들도 건너뜀
		}
		
		if (CurrentNode.bIsLeaf)
		{
			// 리프 노드인 경우 삼각형 인덱스 추가
			// ------------------------------------------------------------------------------------
			// 주의: BVH 내부에서는 삼각형 인덱스 = 인덱스 배열에서 삼각형을 이루는 첫 인덱스의 위치이지만(Index buffer offset),
			// BVH 외부에서는 삼각형 인덱스 = 인덱스 버퍼를 3개 단위로 묶었을 때의 삼각형 번호를 의미하므로(Triangle ordinal)
			// 의미 통일을 위해 외부 반환시 3으로 나누어 사용
			// ------------------------------------------------------------------------------------
			OutTriangleIndices.Add(CurrentNode.TriangleBaseIndex / 3);
		}
		else
		{
			// 내부 노드인 경우 자식들을 스택에 추가
			// 자식 인덱스가 유효한지 확인
			if (CurrentNode.Child1 >= 0 && CurrentNode.Child1 < Nodes.Num())
			{
				NodeStack.Add(CurrentNode.Child1);
			}
			if (CurrentNode.Child2 >= 0 && CurrentNode.Child2 < Nodes.Num())
			{
				NodeStack.Add(CurrentNode.Child2);
			}
		}
	}
	
	return true; // Traverse successful
}

void FBVH::Build(FStaticMesh* InMesh)
{
	if (!InMesh)
	{
		std::cerr << "FBVH::Build: Input mesh is null." << '\n';
		return;
	}
	Clear();
	Mesh = InMesh;
	// 모든 삼각형에 대해 Leaf 노드 삽입
	int32 TriangleCount = Mesh->Indices.Num() / 3;
	for (int32 i = 0; i < TriangleCount; ++i)
	{
		int32 TriangleBaseIndex = i * 3;
		InsertLeaf(TriangleBaseIndex);
	}
	// 전체 비용 계산
	Cost = GetCost(RootIndex);
	// 유효성 검사
	if (!CheckValidity())
	{
		std::cerr << "FBVH::Build: BVH structure is invalid after build." << '\n';
	}
}

