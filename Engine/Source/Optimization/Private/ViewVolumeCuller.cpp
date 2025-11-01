#include "pch.h"
#include "Optimization/Public/ViewVolumeCuller.h"
#include "Core/Public/Object.h"
#include "Global/Octree.h"
#include "Level/Public/Level.h"

namespace
{
	FAABB GetPrimitiveBoundingBox(UPrimitiveComponent* InPrimitive)
	{
		FVector Min, Max;
		InPrimitive->GetWorldAABB(Min, Max);

		return FAABB(Min, Max);
	}
}

void ViewVolumeCuller::Cull(FOctree* StaticOctree, TArray<UPrimitiveComponent*>& DynamicPrimitives, const FCameraConstants& ViewProjConstants)
{
	// 이전의 Cull했던 정보를 지운다.
	RenderableObjects.Empty();
	CurrentFrustum.Clear();

	// 1. 절두체 'Key' 생성 
	FMatrix VP = ViewProjConstants.View * ViewProjConstants.Projection;
	CurrentFrustum.Planes[0] = VP[3] + VP[0]; // Left
	CurrentFrustum.Planes[1] = VP[3] - VP[0]; // Right
	CurrentFrustum.Planes[2] = VP[3] + VP[1]; // Bottom
	CurrentFrustum.Planes[3] = VP[3] - VP[1]; // Top
	CurrentFrustum.Planes[4] = VP[2]; // Near
	CurrentFrustum.Planes[5] = VP[3] - VP[2]; // Far

	for (int i = 0; i < 6; i++)
	{
		const float Length = sqrt((CurrentFrustum.Planes[i].X * CurrentFrustum.Planes[i].X) +
								(CurrentFrustum.Planes[i].Y * CurrentFrustum.Planes[i].Y) +
								(CurrentFrustum.Planes[i].Z * CurrentFrustum.Planes[i].Z));

		if (Length > -MATH_EPSILON && Length < MATH_EPSILON) { return; }

		CurrentFrustum.Planes[i] /= -Length;
	}

	// 2. 옥트리를 이용해 보이는 객체만 RenderableObjects에 저장한다.
	if (StaticOctree)
	{
		CullOctree(StaticOctree);
	}

	for (UPrimitiveComponent* Primitive : DynamicPrimitives)
	{
		if (Primitive != nullptr
			&& Primitive->IsVisible()
			&& CurrentFrustum.CheckIntersection(GetPrimitiveBoundingBox(Primitive)) != EBoundCheckResult::Outside)
		{
			RenderableObjects.Add(Primitive);
		}
	}
}

const TArray<UPrimitiveComponent*>& ViewVolumeCuller::GetRenderableObjects()
{
	// Octree에 없는 DynamicPrimitives들 추가
	//TArray<UPrimitiveComponent*>& DynamicPrimitives = GWorld->GetLevel()->GetDynamicPrimitives();
	//for (UPrimitiveComponent* Primitive : DynamicPrimitives)
	//{
	//	if (Primitive != nullptr && Primitive->IsVisible())
	//	{
	//		RenderableObjects.push_back(Primitive);
	//	}
	//}
	return RenderableObjects;
}

void ViewVolumeCuller::CullOctree(FOctree* Octree)
{
	if (!Octree) { return; }

	// 0. 탐색할 노드를 추가합니다.
	TDeque<FOctree*> VisitngNodes;
	VisitngNodes.push_back(Octree);

	while (VisitngNodes.empty() == false)
	{
		FOctree* CurrentNode = VisitngNodes.back();
		VisitngNodes.pop_back();

		// 현재 옥트리 노드(자신)의 경계와 절두체의 관계를 확인합니다.
		EBoundCheckResult result = CurrentFrustum.CheckIntersection(CurrentNode->GetBoundingBox());
	
		// Case 1. 노드가 절두체 밖에 있다면, 즉시 다음 노드로 넘어갑니다. 
		if (result == EBoundCheckResult::Outside)
		{
			continue;
		}
		// Case 2. 노드가 절두체 안에 완전히 포함된다면, 전부 포함하고 다음 노드로 넘어갑니다.
		else if (result == EBoundCheckResult::Inside)
		{
			TArray<UPrimitiveComponent*> Primitives;
			CurrentNode->GetAllPrimitives(Primitives);
			for (UPrimitiveComponent* Primitive : Primitives)
			{
				if (Primitive != nullptr && Primitive->IsVisible())
				{
					RenderableObjects.Add(Primitive);
				}
			}
			continue;
		}
		// Case 3. 노드가 절두체와 부분적으로 겹쳐진다면, 개별 검사를 합니다.
		else if (result == EBoundCheckResult::Intersect)
		{
			// 노드가 겹치면, 현재 노드에 있는 프리미티브들만 개별적으로 검사합니다.
			for (UPrimitiveComponent* Primitive : CurrentNode->GetPrimitives())
			{
				if (Primitive != nullptr
					&& Primitive->IsVisible()
					&& CurrentFrustum.CheckIntersection(GetPrimitiveBoundingBox(Primitive)) != EBoundCheckResult::Outside)
				{
					RenderableObjects.Add(Primitive);
				}
			}

			// 2. 자식 노드들을 탐색 대상에 추가합니다.
			if (CurrentNode->IsLeafNode() == false)
			{
				const TArray<FOctree*>& Children = CurrentNode->GetChildren();
				for (FOctree* Child : Children)
				{
					if (Child != nullptr) { VisitngNodes.push_back(Child); }
				}
			}

		}

	}

}