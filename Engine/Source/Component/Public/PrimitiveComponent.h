#pragma once
#include "Component/Public/SceneComponent.h"
#include "Physics/Public/BoundingVolume.h"
#include "Global/OverlapInfo.h"
#include "Core/Public/Delegate.h"
#include "Physics/Public/HitResult.h"

// Component-level overlap event signatures
DECLARE_DELEGATE(FComponentBeginOverlapSignature,
	UPrimitiveComponent*, /* OverlappedComponent */
	AActor*,              /* OtherActor */
	UPrimitiveComponent*, /* OtherComp */
	const FHitResult&     /* OverlapInfo */
);

DECLARE_DELEGATE(FComponentEndOverlapSignature,
	UPrimitiveComponent*, /* OverlappedComponent */
	AActor*,              /* OtherActor */
	UPrimitiveComponent*  /* OtherComp */
);

DECLARE_DELEGATE(FComponentHitSignature,
	UPrimitiveComponent*, /* HitComponent */
	AActor*,              /* OtherActor */
	UPrimitiveComponent*, /* OtherComp */
	FVector,              /* NormalImpulse */
	const FHitResult&     /* Hit */
);

UCLASS()
class UPrimitiveComponent : public USceneComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

public:
	UPrimitiveComponent();

	void TickComponent(float DeltaTime) override;
	virtual void OnSelected() override;
	virtual void OnDeselected() override;

	const TArray<FNormalVertex>* GetVerticesData() const;
	const TArray<uint32>* GetIndicesData() const;
	ID3D11Buffer* GetVertexBuffer() const;
	ID3D11Buffer* GetIndexBuffer() const;
	uint32 GetNumVertices() const;
	uint32 GetNumIndices() const;

	const FRenderState& GetRenderState() const { return RenderState; }

	void SetTopology(D3D11_PRIMITIVE_TOPOLOGY InTopology);
	D3D11_PRIMITIVE_TOPOLOGY GetTopology() const;
	//void Render(const URenderer& Renderer) const override;

	bool IsVisible() const { return bVisible; }
	void SetVisibility(bool bVisibility) { bVisible = bVisibility; }
	
	bool CanPick() const { return bCanPick; }
	void SetCanPick(bool bInCanPick) { bCanPick = bInCanPick; }
	

	FVector4 GetColor() const { return Color; }
	void SetColor(const FVector4& InColor) { Color = InColor; }

	// === Collision System (SOLID Separation) ===

	// Broad phase: Calculate world-space AABB for spatial queries (Octree, culling)
	virtual struct FBounds CalcBounds() const;

	// Narrow phase: Get collision shape for precise overlap testing
	// Note: Returns existing BoundingBox pointer (do not delete)
	virtual const IBoundingVolume* GetCollisionShape() const;

	// Legacy methods (kept for backward compatibility)
	virtual const IBoundingVolume* GetBoundingBox();
	void GetWorldAABB(FVector& OutMin, FVector& OutMax);

	// === Collision Event Delegates ===
	// Public so users can bind to these events
	FComponentBeginOverlapSignature OnComponentBeginOverlap;
	FComponentEndOverlapSignature OnComponentEndOverlap;
	FComponentHitSignature OnComponentHit;

	// === Collision Event Flags ===
	// TODO: Replace with ECollisionResponse enum (Ignore/Overlap/Block) for Unreal-style system
	bool bGenerateHitEvents = false;  // true = blocking collision (fires Hit), false = overlap only

	// === Overlap Query API ===
	const TArray<FOverlapInfo>& GetOverlapInfos() const { return OverlappingComponents; }
	bool IsOverlappingComponent(const UPrimitiveComponent* OtherComp) const;
	bool IsOverlappingActor(const AActor* OtherActor) const;

	// Update overlaps (called by Level or when component moves)
	void UpdateOverlaps();

	virtual void MarkAsDirty() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	// 데칼에 덮일 수 있는가
	bool bReceivesDecals = true;

	// 다른 곳에서 사용할 인덱스
	mutable int32 CachedAABBIndex = -1;
	mutable uint32 CachedFrame = 0;

protected:
	const TArray<FNormalVertex>* Vertices = nullptr;
	const TArray<uint32>* Indices = nullptr;

	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer = nullptr;

	uint32 NumVertices = 0;
	uint32 NumIndices = 0;

	FVector4 Color = FVector4{ 0.f,0.f,0.f,0.f };

	D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	FRenderState RenderState = {};

	bool bVisible = true;
	bool bCanPick = true;

	mutable IBoundingVolume* BoundingBox = nullptr;
	bool bOwnsBoundingBox = false;
	
	mutable FVector CachedWorldMin;
	mutable FVector CachedWorldMax;
	mutable bool bIsAABBCacheDirty = true;

	// Overlap tracking
	TArray<FOverlapInfo> OverlappingComponents;

	// === Collision Calculation Helpers ===
	// Calculate detailed hit info (Normal, PenetrationDepth) for blocking collisions
	// Returns true if detailed info was calculated, false otherwise
	// TODO: Generalize for all shape types (currently BoxToBox only)
	bool CalculateDetailedHitInfo(UPrimitiveComponent* OtherComp, FHitResult& OutHit);

	// === Event Notification Helpers ===
	void NotifyComponentBeginOverlap(UPrimitiveComponent* OtherComp, const FHitResult& SweepResult);
	void NotifyComponentEndOverlap(UPrimitiveComponent* OtherComp);
	void NotifyComponentHit(UPrimitiveComponent* OtherComp, const FVector& NormalImpulse, const FHitResult& Hit);

public:
	virtual UObject* Duplicate() override;

protected:
	virtual void DuplicateSubObjects(UObject* DuplicatedObject) override;
};
