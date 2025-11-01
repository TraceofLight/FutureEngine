#pragma once
#include "Core/Public/Object.h"
#include "Editor/Public/Camera.h"
#include "Global/Enum.h"

namespace json { class JSON; }
using JSON = json::JSON;

class UWorld;
class AActor;
class UPrimitiveComponent;
class UPointLightComponent;
class ULightComponent;
class FOctree;

UCLASS()
class ULevel : public UObject
{
	GENERATED_BODY()
	DECLARE_CLASS(ULevel, UObject)
public:
	ULevel();
	~ULevel() override;

	virtual void Init();

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	const TArray<AActor*>& GetLevelActors() const { return LevelActors; }

	void AddActorToLevel(AActor* InActor);

	void AddLevelComponent(AActor* Actor);

	void RegisterComponent(UActorComponent* InComponent);
	void UnregisterComponent(UActorComponent* InComponent);
	bool DestroyActor(AActor* InActor);

	uint64 GetShowFlags() const { return ShowFlags; }
	void SetShowFlags(uint64 InShowFlags) { ShowFlags = InShowFlags; }

	void UpdatePrimitiveInOctree(UPrimitiveComponent* InComponent);

	FOctree* GetStaticOctree() { return StaticOctree; }

	/** @todo: 효율 개선을 위해 DirtyFlag와 캐시 도입 가능 */
	TArray<UPrimitiveComponent*>& GetDynamicPrimitives()
	{
		DynamicPrimitives.clear();

		for (auto [Component, TimePoint] : DynamicPrimitiveMap)
		{
			DynamicPrimitives.push_back(Component);
		}
		return DynamicPrimitives;
	}

	friend class UWorld;
public:
	virtual UObject* Duplicate() override;

protected:
	virtual void DuplicateSubObjects(UObject* DuplicatedObject) override;

private:
	AActor* SpawnActorToLevel(UClass* InActorClass, JSON* ActorJsonData = nullptr);

	TArray<AActor*> LevelActors;	// 레벨이 보유하고 있는 모든 Actor를 배열로 저장합니다.

	// 지연 삭제를 위한 리스트
	TArray<AActor*> ActorsToDelete;

	uint64 ShowFlags =
		static_cast<uint64>(EEngineShowFlags::SF_Billboard) |
		static_cast<uint64>(EEngineShowFlags::SF_StaticMesh) |
		static_cast<uint64>(EEngineShowFlags::SF_Text) |
		static_cast<uint64>(EEngineShowFlags::SF_Decal) |
		static_cast<uint64>(EEngineShowFlags::SF_Fog) |
		static_cast<uint64>(EEngineShowFlags::SF_Collision);
	
	/*-----------------------------------------------------------------------------
		Octree Management
	-----------------------------------------------------------------------------*/
public:
	void UpdateOctree();
	void UpdateOctreeImmediate();

private:

	void OnPrimitiveUpdated(UPrimitiveComponent* InComponent);

	void OnPrimitiveUnregistered(UPrimitiveComponent* InComponent);

	/** @brief 한 프레임에 Octree에 삽입할 오브젝트의 최대 크기를 결정해서 부하를 여러 프레임에 분산함. */
	static constexpr uint32 MAX_OBJECTS_TO_INSERT_PER_FRAME = 256;
	
	/** @brief 가장 오래전에 움직인 UPrimitiveComponent를 Octree에 삽입하기 위해 필요한 구조체. */
	struct FDynamicPrimitiveData
	{
		UPrimitiveComponent* Primitive;
		float LastMoveTimePoint;

		bool operator>(const FDynamicPrimitiveData& Other) const
		{
			return LastMoveTimePoint > Other.LastMoveTimePoint;
		}
	};
	
	using FDynamicPrimitiveQueue = TQueue<FDynamicPrimitiveData>;
	
	FOctree* StaticOctree = nullptr;

	/** @deprecated 기존 코드와의 호환성을 위해 유지, 직접 사용하거나 업데이트하는 것을 금지함 */
	TArray<UPrimitiveComponent*> DynamicPrimitives;

	/** @brief 가장 오래전에 움직인 UPrimitiveComponent부터 순서대로 Octree에 삽입할 수 있도록 보관 */
	FDynamicPrimitiveQueue DynamicPrimitiveQueue;

	/** @brief 각 UPrimitiveComponent가 움직인 가장 마지막 시간을 기록 */
	TMap<UPrimitiveComponent*, float> DynamicPrimitiveMap;
	
	/*-----------------------------------------------------------------------------
		Lighting Management
	-----------------------------------------------------------------------------*/
public:
	const TArray<ULightComponent*>& GetLightComponents() const { return LightComponents; } 

private:
	TArray<ULightComponent*> LightComponents;
};
