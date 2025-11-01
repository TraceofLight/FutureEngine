#include "pch.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Global/OverlapInfo.h"

#include "Manager/Asset/Public/AssetManager.h"
#include "Physics/Public/AABB.h"
#include "Physics/Public/OBB.h"
#include "Physics/Public/BoundingSphere.h"
#include "Physics/Public/Capsule.h"
#include "Physics/Public/Bounds.h"
#include "Physics/Public/CollisionHelper.h"
#include "Utility/Public/JsonSerializer.h"
#include "Actor/Public/Actor.h"
#include "Level/Public/Level.h"
#include "Global/Octree.h"
#include <unordered_set>

IMPLEMENT_ABSTRACT_CLASS(UPrimitiveComponent, USceneComponent)

UPrimitiveComponent::UPrimitiveComponent()
{
	bCanEverTick = true;
}

void UPrimitiveComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
}
void UPrimitiveComponent::OnSelected()
{
	SetColor({ 1.f, 0.8f, 0.2f, 0.4f });
}

void UPrimitiveComponent::OnDeselected()
{
	SetColor({ 0.f, 0.f, 0.f, 0.f });
}

void USceneComponent::SetUniformScale(bool bIsUniform)
{
	bIsUniformScale = bIsUniform;
}

bool USceneComponent::IsUniformScale() const
{
	return bIsUniformScale;
}

const TArray<FNormalVertex>* UPrimitiveComponent::GetVerticesData() const
{
	return Vertices;
}

const TArray<uint32>* UPrimitiveComponent::GetIndicesData() const
{
	return Indices;
}

ID3D11Buffer* UPrimitiveComponent::GetVertexBuffer() const
{
	return VertexBuffer;
}

ID3D11Buffer* UPrimitiveComponent::GetIndexBuffer() const
{
	return IndexBuffer;
}

uint32 UPrimitiveComponent::GetNumVertices() const
{
	return NumVertices;
}

uint32 UPrimitiveComponent::GetNumIndices() const
{
	return NumIndices;
}

void UPrimitiveComponent::SetTopology(D3D11_PRIMITIVE_TOPOLOGY InTopology)
{
	Topology = InTopology;
}

D3D11_PRIMITIVE_TOPOLOGY UPrimitiveComponent::GetTopology() const
{
	return Topology;
}

// === Collision System (SOLID Separation) ===

FBounds UPrimitiveComponent::CalcBounds() const
{
	// Default: Use GetWorldAABB (subclasses should override for efficiency)
	FVector WorldMin, WorldMax;
	const_cast<UPrimitiveComponent*>(this)->GetWorldAABB(WorldMin, WorldMax);
	return FBounds(WorldMin, WorldMax);
}

const IBoundingVolume* UPrimitiveComponent::GetCollisionShape() const
{
	// Update collision shape to world space before returning
	if (BoundingBox)
	{
		BoundingBox->Update(GetWorldTransformMatrix());
	}
	return BoundingBox;
}

const IBoundingVolume* UPrimitiveComponent::GetBoundingBox()
{
	// Legacy method - kept for backward compatibility with Octree, culling, etc.
	if (BoundingBox)
	{
		BoundingBox->Update(GetWorldTransformMatrix());
	}
	return BoundingBox;
}

void UPrimitiveComponent::GetWorldAABB(FVector& OutMin, FVector& OutMax)
{
	// Keep existing implementation for Octree and other systems
	if (!BoundingBox)
	{
		OutMin = FVector(); OutMax = FVector();
		return;
	}

	if (bIsAABBCacheDirty)
	{
		if (BoundingBox->GetType() == EBoundingVolumeType::AABB)
		{
			const FAABB* LocalAABB = static_cast<const FAABB*>(BoundingBox);
			FVector LocalCorners[8] =
			{
				FVector(LocalAABB->Min.X, LocalAABB->Min.Y, LocalAABB->Min.Z), FVector(LocalAABB->Max.X, LocalAABB->Min.Y, LocalAABB->Min.Z),
				FVector(LocalAABB->Min.X, LocalAABB->Max.Y, LocalAABB->Min.Z), FVector(LocalAABB->Max.X, LocalAABB->Max.Y, LocalAABB->Min.Z),
				FVector(LocalAABB->Min.X, LocalAABB->Min.Y, LocalAABB->Max.Z), FVector(LocalAABB->Max.X, LocalAABB->Min.Y, LocalAABB->Max.Z),
				FVector(LocalAABB->Min.X, LocalAABB->Max.Y, LocalAABB->Max.Z), FVector(LocalAABB->Max.X, LocalAABB->Max.Y, LocalAABB->Max.Z)
			};

			const FMatrix& WorldTransform = GetWorldTransformMatrix();
			FVector WorldMin(+FLT_MAX, +FLT_MAX, +FLT_MAX);
			FVector WorldMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

			for (int32 Idx = 0; Idx < 8; Idx++)
			{
				FVector4 WorldCorner = FVector4(LocalCorners[Idx].X, LocalCorners[Idx].Y, LocalCorners[Idx].Z, 1.0f) * WorldTransform;
				WorldMin.X = min(WorldMin.X, WorldCorner.X);
				WorldMin.Y = min(WorldMin.Y, WorldCorner.Y);
				WorldMin.Z = min(WorldMin.Z, WorldCorner.Z);
				WorldMax.X = max(WorldMax.X, WorldCorner.X);
				WorldMax.Y = max(WorldMax.Y, WorldCorner.Y);
				WorldMax.Z = max(WorldMax.Z, WorldCorner.Z);
			}

			CachedWorldMin = WorldMin;
			CachedWorldMax = WorldMax;
		}
		else if (BoundingBox->GetType() == EBoundingVolumeType::OBB ||
			BoundingBox->GetType() == EBoundingVolumeType::SpotLight)
		{
			const FOBB* OBB = static_cast<const FOBB*>(BoundingBox);
			FAABB AABB = OBB->ToWorldAABB();

			CachedWorldMin = AABB.Min;
			CachedWorldMax = AABB.Max;
		}
		else
		{
			// For other types (Sphere, Capsule, etc), use CalcBounds()
			// These are primarily shape components used for collision, not spatial partitioning
			FBounds Bounds = CalcBounds();
			CachedWorldMin = Bounds.Min;
			CachedWorldMax = Bounds.Max;
		}

		bIsAABBCacheDirty = false;
	}

	OutMin = CachedWorldMin;
	OutMax = CachedWorldMax;
}

void UPrimitiveComponent::MarkAsDirty()
{
	bIsAABBCacheDirty = true;
	Super::MarkAsDirty();

	// Update octree position first, then check for overlaps
	// This ensures that when this component queries the octree, it has the latest positions
	AActor* Owner = GetOwner();
	if (Owner && Owner->GetOuter())
	{
		ULevel* Level = Cast<ULevel>(Owner->GetOuter());
		if (Level)
		{
			Level->UpdatePrimitiveInOctree(this);
		}
	}

	// Now update overlaps with the latest octree data
	UpdateOverlaps();
}


UObject* UPrimitiveComponent::Duplicate()
{
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Super::Duplicate());
	
	PrimitiveComponent->Color = Color;
	PrimitiveComponent->Topology = Topology;
	PrimitiveComponent->RenderState = RenderState;
	PrimitiveComponent->bVisible = bVisible;
	PrimitiveComponent->bReceivesDecals = bReceivesDecals;

	PrimitiveComponent->Vertices = Vertices;
	PrimitiveComponent->Indices = Indices;
	PrimitiveComponent->VertexBuffer = VertexBuffer;
	PrimitiveComponent->IndexBuffer = IndexBuffer;
	PrimitiveComponent->NumVertices = NumVertices;
	PrimitiveComponent->NumIndices = NumIndices;

	if (!bOwnsBoundingBox)
	{
		PrimitiveComponent->BoundingBox = BoundingBox;
	}
	
	return PrimitiveComponent;
}

void UPrimitiveComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);

}
void UPrimitiveComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FString VisibleString;
		FJsonSerializer::ReadString(InOutHandle, "bVisible", VisibleString, "true");
		SetVisibility(VisibleString == "true");
	}
	else
	{
		InOutHandle["bVisible"] = bVisible ? "true" : "false";
	}

}

// === Overlap Query Implementation ===

bool UPrimitiveComponent::IsOverlappingComponent(const UPrimitiveComponent* OtherComp) const
{
	if (!OtherComp)
		return false;

	for (const FOverlapInfo& Info : OverlappingComponents)
	{
		if (Info.OverlapComponent.Get() == OtherComp)
			return true;
	}
	return false;
}

bool UPrimitiveComponent::IsOverlappingActor(const AActor* OtherActor) const
{
	if (!OtherActor)
		return false;

	for (const FOverlapInfo& Info : OverlappingComponents)
	{
		if (Info.IsValid() && Info.GetActor() == OtherActor)
			return true;
	}
	return false;
}

void UPrimitiveComponent::UpdateOverlaps()
{
	AActor* MyOwner = GetOwner();
	if (!MyOwner)
		return;

	ULevel* Level = Cast<ULevel>(MyOwner->GetOuter());
	if (!Level || !Level->GetStaticOctree())
		return;

	// === PHASE 1: Broad Phase (Spatial Query) ===
	// Use FBounds for fast AABB-based octree query
	FBounds MyBounds = CalcBounds();
	FAABB MyAABB(MyBounds.Min, MyBounds.Max);

	TArray<UPrimitiveComponent*> Candidates;
	Level->GetStaticOctree()->QueryAABB(MyAABB, Candidates);

	// Store previous overlaps for comparison
	TArray<FOverlapInfo> PreviousOverlaps = OverlappingComponents;

	// === PHASE 2: Narrow Phase (Precise Collision) ===
	// Use actual collision shapes for accurate overlap testing
	TArray<FOverlapInfo> NewOverlaps;
	const IBoundingVolume* MyShape = GetCollisionShape();

	for (UPrimitiveComponent* Candidate : Candidates)
	{
		// Skip self
		if (Candidate == this)
			continue;

		// Skip same actor components
		if (Candidate->GetOwner() == MyOwner)
			continue;

		// Broad phase AABB rejection (per-primitive level)
		// Note: Octree::QueryAABB() returns all primitives in overlapping nodes,
		// so we need to test individual primitive AABBs here
		FBounds CandidateBounds = Candidate->CalcBounds();
		if (!MyBounds.Overlaps(CandidateBounds))
			continue;

		// Narrow phase: Precise shape-to-shape test
		const IBoundingVolume* OtherShape = Candidate->GetCollisionShape();
		if (FCollisionHelper::TestOverlap(MyShape, OtherShape))
		{
			NewOverlaps.Add(FOverlapInfo(Candidate));
		}
	}

	// 5. Detect Begin Overlaps (in NewOverlaps but not in PreviousOverlaps)
	// Optimize: Use unordered_set for O(1) lookup instead of O(N) linear search
	std::unordered_set<FOverlapInfo> PreviousSet(PreviousOverlaps.begin(), PreviousOverlaps.end());

	for (const FOverlapInfo& NewInfo : NewOverlaps)
	{
		// This is a new overlap - fire BeginOverlap event
		if (PreviousSet.find(NewInfo) == PreviousSet.end() && NewInfo.IsValid())
		{
			FHitResult HitResult;
			HitResult.Actor = NewInfo.GetActor();
			HitResult.Component = NewInfo.OverlapComponent.Get();
			// Note: For simple overlap detection, we don't have detailed hit info
			// In a physics engine with continuous collision detection, you'd fill this in

			NotifyComponentBeginOverlap(NewInfo.OverlapComponent.Get(), HitResult);
		}
	}

	// 6. Detect End Overlaps (in PreviousOverlaps but not in NewOverlaps)
	// Optimize: Use unordered_set for O(1) lookup instead of O(N) linear search
	std::unordered_set<FOverlapInfo> NewSet(NewOverlaps.begin(), NewOverlaps.end());

	for (const FOverlapInfo& PrevInfo : PreviousOverlaps)
	{
		// Overlap ended - fire EndOverlap event
		if (NewSet.find(PrevInfo) == NewSet.end() && PrevInfo.IsValid())
		{
			NotifyComponentEndOverlap(PrevInfo.OverlapComponent.Get());
		}
	}

	// 7. Update overlap list
	OverlappingComponents = NewOverlaps;
}

// === Event Notification Helpers ===

void UPrimitiveComponent::NotifyComponentBeginOverlap(UPrimitiveComponent* OtherComp, const FHitResult& SweepResult)
{
	if (!OtherComp)
		return;

	AActor* MyOwner = GetOwner();
	AActor* OtherOwner = OtherComp->GetOwner();

	// 1. Broadcast component-level events (bidirectional)
	// Log and broadcast for THIS component
	UE_LOG("BeginOverlap: [%s]%s overlaps with [%s]%s",
		MyOwner ? MyOwner->GetName().ToString().c_str() : "None",
		GetName().ToString().c_str(),
		OtherOwner ? OtherOwner->GetName().ToString().c_str() : "None",
		OtherComp->GetName().ToString().c_str());
	OnComponentBeginOverlap.Broadcast(this, OtherOwner, OtherComp, SweepResult);

	// Log and broadcast for OTHER component
	UE_LOG("BeginOverlap: [%s]%s overlaps with [%s]%s",
		OtherOwner ? OtherOwner->GetName().ToString().c_str() : "None",
		OtherComp->GetName().ToString().c_str(),
		MyOwner ? MyOwner->GetName().ToString().c_str() : "None",
		GetName().ToString().c_str());
	OtherComp->OnComponentBeginOverlap.Broadcast(OtherComp, MyOwner, this, SweepResult);

	// 2. Broadcast actor-level events (bidirectional)
	if (MyOwner)
	{
		MyOwner->OnActorBeginOverlap.Broadcast(MyOwner, OtherOwner);
	}
	if (OtherOwner)
	{
		OtherOwner->OnActorBeginOverlap.Broadcast(OtherOwner, MyOwner);
	}
}

void UPrimitiveComponent::NotifyComponentEndOverlap(UPrimitiveComponent* OtherComp)
{
	if (!OtherComp)
		return;

	AActor* MyOwner = GetOwner();
	AActor* OtherOwner = OtherComp->GetOwner();

	// 1. Broadcast component-level events (bidirectional)
	// Log and broadcast for THIS component
	UE_LOG("EndOverlap: [%s]%s stopped overlapping with [%s]%s",
		MyOwner ? MyOwner->GetName().ToString().c_str() : "None",
		GetName().ToString().c_str(),
		OtherOwner ? OtherOwner->GetName().ToString().c_str() : "None",
		OtherComp->GetName().ToString().c_str());
	OnComponentEndOverlap.Broadcast(this, OtherOwner, OtherComp);

	// Log and broadcast for OTHER component
	UE_LOG("EndOverlap: [%s]%s stopped overlapping with [%s]%s",
		OtherOwner ? OtherOwner->GetName().ToString().c_str() : "None",
		OtherComp->GetName().ToString().c_str(),
		MyOwner ? MyOwner->GetName().ToString().c_str() : "None",
		GetName().ToString().c_str());
	OtherComp->OnComponentEndOverlap.Broadcast(OtherComp, MyOwner, this);

	// 2. Broadcast actor-level events (bidirectional)
	if (MyOwner)
	{
		MyOwner->OnActorEndOverlap.Broadcast(MyOwner, OtherOwner);
	}
	if (OtherOwner)
	{
		OtherOwner->OnActorEndOverlap.Broadcast(OtherOwner, MyOwner);
	}
}

void UPrimitiveComponent::NotifyComponentHit(UPrimitiveComponent* OtherComp, const FVector& NormalImpulse, const FHitResult& Hit)
{
	if (!OtherComp)
		return;

	AActor* MyOwner = GetOwner();
	AActor* OtherOwner = OtherComp->GetOwner();

	// 1. Broadcast component-level events (bidirectional)
	// Note: NormalImpulse is negated for the other component
	OnComponentHit.Broadcast(this, OtherOwner, OtherComp, NormalImpulse, Hit);
	OtherComp->OnComponentHit.Broadcast(OtherComp, MyOwner, this, -NormalImpulse, Hit);

	// 2. Broadcast actor-level events (bidirectional)
	if (MyOwner)
	{
		MyOwner->OnActorHit.Broadcast(MyOwner, OtherOwner, NormalImpulse, Hit);
	}
	if (OtherOwner)
	{
		OtherOwner->OnActorHit.Broadcast(OtherOwner, MyOwner, -NormalImpulse, Hit);
	}
}