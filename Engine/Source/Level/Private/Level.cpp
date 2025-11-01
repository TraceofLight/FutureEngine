#include "pch.h"
#include "Actor/Public/Actor.h"
#include "Component/Public/LightComponent.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Component/Public/PointLightComponent.h"
#include "Component/Public/DirectionalLightComponent.h"
#include "Component/Public/AmbientLightComponent.h"
#include "Component/Public/SpotLightComponent.h"
#include "Core/Public/Object.h"
#include "Editor/Public/Editor.h"
#include "Global/Octree.h"
#include "Level/Public/Level.h"
#include "Manager/Config/Public/ConfigManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Utility/Public/JsonSerializer.h"
#include "Manager/UI/Public/ViewportManager.h"
#include <json.hpp>

IMPLEMENT_CLASS(ULevel, UObject)

ULevel::ULevel()
{
	StaticOctree = new FOctree(FVector(0, 0, -5), 75, 0);
}

ULevel::~ULevel()
{
	// LevelActors 배열에 남아있는 모든 액터의 메모리를 해제합니다.
	// 역순 루프를 사용하여 DestroyActor 내부의 Remove 호출로 인한 반복자 무효화 방지
	while (!LevelActors.IsEmpty())
	{
		DestroyActor(LevelActors.Last());
	}
	LevelActors.Empty();

	// 모든 액터 객체가 삭제되었으므로, 포인터를 담고 있던 컨테이너들을 비웁니다.
	SafeDelete(StaticOctree);
}

void ULevel::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// 불러오기
	if (bInIsLoading)
	{
		// NOTE: 레벨 로드 시 NextUUID를 변경하면 UUID 충돌이 발생하므로 관련 기능 구현을 보류합니다.
		uint32 NextUUID = 0;
		FJsonSerializer::ReadUint32(InOutHandle, "NextUUID", NextUUID);

		JSON ActorsJson;
		if (FJsonSerializer::ReadObject(InOutHandle, "Actors", ActorsJson))
		{
			for (auto& Pair : ActorsJson.ObjectRange())
			{
				JSON& ActorDataJson = Pair.second;

				FString TypeString;
				FJsonSerializer::ReadString(ActorDataJson, "Type", TypeString);
				
				UClass* ActorClass = UClass::FindClass(TypeString);
				SpawnActorToLevel(ActorClass, &ActorDataJson); 
			}
		}

		// 뷰포트 카메라 정보 로드
		UViewportManager::GetInstance().SerializeViewports(bInIsLoading, InOutHandle);
	}
	// 저장
	else
	{
		// NOTE: 레벨 로드 시 NextUUID를 변경하면 UUID 충돌이 발생하므로 관련 기능 구현을 보류합니다.
		InOutHandle["NextUUID"] = 0;

		JSON ActorsJson = json::Object();
		for (AActor* Actor : LevelActors)
		{
			JSON ActorJson;
			ActorJson["Type"] = Actor->GetClass()->GetName().ToString();
			Actor->Serialize(bInIsLoading, ActorJson); 

			ActorsJson[std::to_string(Actor->GetUUID())] = ActorJson;
		}
		InOutHandle["Actors"] = ActorsJson;

		// 뷰포트 카메라 정보 저장
		UViewportManager::GetInstance().SerializeViewports(bInIsLoading, InOutHandle);
	}
}

void ULevel::Init()
{
	for (AActor* Actor: LevelActors)
	{
		if (Actor)
		{
			Actor->BeginPlay();
		}
	}
}

AActor* ULevel::SpawnActorToLevel(UClass* InActorClass, JSON* ActorJsonData)
{
	if (!InActorClass)
	{
		return nullptr;
	}

	AActor* NewActor = Cast<AActor>(NewObject(InActorClass, this));
	if (NewActor)
	{
		LevelActors.Add(NewActor);
		if (ActorJsonData != nullptr)
		{
			NewActor->Serialize(true, *ActorJsonData);
		}
		else
		{
			NewActor->InitializeComponents();
		}
		NewActor->BeginPlay();
		AddLevelComponent(NewActor);
		return NewActor;
	}

	return nullptr;
}

void ULevel::RegisterComponent(UActorComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	if (!StaticOctree)
	{
		return;
	}

	if (auto PrimitiveComponent = Cast<UPrimitiveComponent>(InComponent))
	{
		// StaticOctree에 먼저 삽입 시도
		if (!(StaticOctree->Insert(PrimitiveComponent)))
		{
			// 실패하면 DynamicPrimitiveQueue 목록에 추가
			OnPrimitiveUpdated(PrimitiveComponent);
		}
	}
	else if (auto LightComponent = Cast<ULightComponent>(InComponent))
	{
		if (auto PointLightComponent = Cast<UPointLightComponent>(LightComponent))
		{
			if (auto SpotLightComponent = Cast<USpotLightComponent>(PointLightComponent))
			{
				LightComponents.Add(SpotLightComponent);
			}
			else
			{
				LightComponents.Add(PointLightComponent);
			}
		}
		if (auto DirectionalLightComponent = Cast<UDirectionalLightComponent>(LightComponent))
		{
			LightComponents.Add(DirectionalLightComponent);
		}
		if (auto AmbientLightComponent = Cast<UAmbientLightComponent>(LightComponent))
		{
			LightComponents.Add(AmbientLightComponent);
		}
		
		
	}
	UE_LOG("Level: '%s' 컴포넌트를 씬에 등록했습니다.", InComponent->GetName().ToString().data());
}

void ULevel::UnregisterComponent(UActorComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	if (!StaticOctree)
	{
		return;
	}

	if (auto PrimitiveComponent = Cast<UPrimitiveComponent>(InComponent))
	{
		// StaticOctree에서 제거 시도
		StaticOctree->Remove(PrimitiveComponent);
	
		OnPrimitiveUnregistered(PrimitiveComponent);
	}
	else if (auto LightComponent = Cast<ULightComponent>(InComponent))
	{
		LightComponents.Remove(LightComponent);
	}
	
}

void ULevel::AddActorToLevel(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}

	LevelActors.Add(InActor);
}

void ULevel::AddLevelComponent(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	for (auto& Component : Actor->GetOwnedComponents())
	{
		if (auto PrimitiveComponent = Cast<UPrimitiveComponent>(Component))
		{
			// Octree Insert 시도
			if (!(StaticOctree->Insert(PrimitiveComponent)))
			{
				// 실패하면 DynamicPrimitiveQueue에 추가
				OnPrimitiveUpdated(PrimitiveComponent);
			}
		}
		else if (auto LightComponent = Cast<ULightComponent>(Component))
		{
			if (auto PointLightComponent = Cast<UPointLightComponent>(LightComponent))
			{
				if (auto SpotLightComponent = Cast<USpotLightComponent>(PointLightComponent))
				{
					LightComponents.Add(SpotLightComponent);
				}
				else
				{
					LightComponents.Add(PointLightComponent);
				}
			}
			if (auto DirectionalLightComponent = Cast<UDirectionalLightComponent>(LightComponent))
			{
				LightComponents.Add(DirectionalLightComponent);
			}
			if (auto AmbientLightComponent = Cast<UAmbientLightComponent>(LightComponent))
			{
				LightComponents.Add(AmbientLightComponent);
			}
			
		}
	}
}

// Level에서 Actor 제거하는 함수
bool ULevel::DestroyActor(AActor* InActor)
{
	if (!InActor)
	{
		return false;
	}

	// 컴포넌트들을 옥트리에서 제거
	for (auto& Component : InActor->GetOwnedComponents())
	{
		UnregisterComponent(Component);
	}

	// LevelActors 리스트에서 제거
	LevelActors.Remove(InActor);

	// Remove Actor Selection
	UEditor* Editor = GEditor->GetEditorModule();
	if (Editor->GetSelectedActor() == InActor)
	{
		Editor->SelectActor(nullptr);
		Editor->SelectComponent(nullptr);
	}

	// Remove
	SafeDelete(InActor);

	return true;
}

void ULevel::UpdatePrimitiveInOctree(UPrimitiveComponent* InComponent)
{
	if (!StaticOctree->Remove(InComponent))
	{
		return;
	}
	OnPrimitiveUpdated(InComponent);
}

UObject* ULevel::Duplicate()
{
	ULevel* Level = Cast<ULevel>(Super::Duplicate());
	Level->ShowFlags = ShowFlags;
	return Level;
}

void ULevel::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
	ULevel* DuplicatedLevel = Cast<ULevel>(DuplicatedObject);

	for (AActor* Actor : LevelActors)
	{
		AActor* DuplicatedActor = Cast<AActor>(Actor->Duplicate());
		DuplicatedLevel->LevelActors.Add(DuplicatedActor);
		DuplicatedLevel->AddLevelComponent(DuplicatedActor);
	}
}

/*-----------------------------------------------------------------------------
	Octree Management
-----------------------------------------------------------------------------*/

void ULevel::UpdateOctree()
{
	if (!StaticOctree)
	{
		return;
	}
	
	uint32 Count = 0;
	FDynamicPrimitiveQueue NotInsertedQueue;
	
	while (!DynamicPrimitiveQueue.empty() && Count < MAX_OBJECTS_TO_INSERT_PER_FRAME)
	{
		auto [Component, TimePoint] = DynamicPrimitiveQueue.front();
		DynamicPrimitiveQueue.pop();

		auto* FoundTimestampPtr = DynamicPrimitiveMap.Find(Component);
		if (FoundTimestampPtr)
		{
			if (*FoundTimestampPtr <= TimePoint)
			{
				// 큐에 기록된 오브젝트의 마지막 변경 시간 이후로 변경이 없었다면 Octree에 재삽입한다.
				if (StaticOctree->Insert(Component))
				{
					DynamicPrimitiveMap.Remove(Component);
				}
				// 삽입이 안됐다면 다시 Queue에 들어가기 위해 저장
				else
				{
					NotInsertedQueue.push({Component, *FoundTimestampPtr});
				}
				// TODO: 오브젝트의 유일성을 보장하기 위해 StaticOctree->Remove(Component)가 필요한가?
				++Count;
			}
			else
			{
				// 큐에 기록된 오브젝트의 마지막 변경 이후 새로운 변경이 존재했다면 다시 큐에 삽입한다.
				DynamicPrimitiveQueue.push({Component, *FoundTimestampPtr});
			}
		}
	}
	
	DynamicPrimitiveQueue = NotInsertedQueue;
	if (Count != 0)
	{
		// UE_LOG("UpdateOctree: %d개의 컴포넌트가 업데이트 되었습니다.", Count);
	}
}

void ULevel::UpdateOctreeImmediate()
{
	if (!StaticOctree)
	{
		return;
	}

	uint32 TotalCount = 0;
	FDynamicPrimitiveQueue NotInsertedQueue;

	// MAX_OBJECTS_TO_INSERT_PER_FRAME 제한 없이 모든 큐 비우기
	while (!DynamicPrimitiveQueue.empty())
	{
		auto [Component, TimePoint] = DynamicPrimitiveQueue.front();
		DynamicPrimitiveQueue.pop();

		auto* FoundTimestampPtr = DynamicPrimitiveMap.Find(Component);
		if (FoundTimestampPtr)
		{
			if (*FoundTimestampPtr <= TimePoint)
			{
				// 큐에 기록된 오브젝트의 마지막 변경 시간 이후로 변경이 없었다면 Octree에 재삽입
				if (StaticOctree->Insert(Component))
				{
					DynamicPrimitiveMap.Remove(Component);
					++TotalCount;
				}
				// 삽입이 안됐다면 다시 Queue에 들어가기 위해 저장
				else
				{
					NotInsertedQueue.push({Component, *FoundTimestampPtr});
				}
			}
			else
			{
				// 큐에 기록된 오브젝트의 마지막 변경 이후 새로운 변경이 존재했다면 다시 큐에 삽입
				DynamicPrimitiveQueue.push({Component, *FoundTimestampPtr});
			}
		}
	}

	DynamicPrimitiveQueue = NotInsertedQueue;

	if (TotalCount > 0)
	{
		UE_LOG("Level: Octree 즉시 구축 완료 (%u개 컴포넌트)", TotalCount);
	}
}

void ULevel::OnPrimitiveUpdated(UPrimitiveComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	float GameTime = UTimeManager::GetInstance().GetGameTime();
	float* FoundTimePtr = DynamicPrimitiveMap.Find(InComponent);

	if (FoundTimePtr)
	{
		*FoundTimePtr = GameTime;
	}
	else
	{
		DynamicPrimitiveMap.Add(InComponent, GameTime);

		DynamicPrimitiveQueue.push({InComponent, GameTime});
	}
}

void ULevel::OnPrimitiveUnregistered(UPrimitiveComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	DynamicPrimitiveMap.Remove(InComponent);
}
