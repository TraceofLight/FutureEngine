#pragma once
#include <filesystem>
#include "Core/Public/Object.h"
#include "Global/Types.h"

class UEditor;
class ULevel;
class AActor;
class UClass;

namespace json { class JSON; }
using JSON = json::JSON;

// It represents the context in which the world is being used.
enum EWorldType
{
    Editor,        // A world being edited in the editor
	EditorPreview, // A preview world for an editor tool (e.g. static mesh viewer)
	PIE,           // A Play In Editor world (Cloned from Editor world)
	Game,          // The game world (Built and played by user)
};

/*
* <World>의 책임
* 1. 게임 전체의 Life Cycle 관리 (BeginPlay, EndPlay, Tick)
* 2. Tick 루프 조정 (Tick 순서 관리)
* 3. 레벨 관리 (레벨 load, save, reset) 트리거
* 4. Spawn, Destroy 시점 조정 (특히 Destroy 시점 관리. Destroy 요청 모았다가 안전한 시점에 처리할 수 있도록)
* 5. 월드 좌표계 기준 전역 쿼리(Octree, intersectoin test) 진입점
*/

// The World is the top level object representing a map or a sandbox in which Actors and Components will exist and be rendered.
UCLASS()
class UWorld final :
	public UObject
{
	GENERATED_BODY()
	DECLARE_CLASS(UWorld, UObject)

public:
	UWorld();
	UWorld(EWorldType WorldType);
	~UWorld();

	// Lifecycle & Tick
	void BeginPlay();
	bool EndPlay();
	void Tick(float DeltaTimes);
	float GetTimeSeconds() const { return WorldTimeSeconds; }

	// Level Management Triggers
	ULevel* GetLevel() const;
	void CreateNewLevel(const FName& InLevelName = FName::GetNone());
	bool LoadLevel(std::filesystem::path InLevelFilePath);
	bool SaveCurrentLevel(std::filesystem::path InLevelFilePath) const;

	// Actor Spawn & Destroy
	AActor* SpawnActor(UClass* InActorClass, JSON* ActorJsonData = nullptr);
	bool DestroyActor(AActor* InActor); // Level의 void MarkActorForDeletion(AActor * InActor) 기능을 DestroyActor가 가짐

	// TODO: World Scope Query Entrypoint
	// Editor에서 쿼리 요청시 Level에 바로 요청하지 않고 World를 통해 요청하도록 변경 

	EWorldType GetWorldType() const;
	void SetWorldType(EWorldType InWorldType);

private:
	EWorldType WorldType;
	ULevel* Level = nullptr; // Persistance Level. Sublevels are not considered in Engine.
	bool bBegunPlay = false;
	TArray<AActor*> PendingDestroyActors;
	float WorldTimeSeconds;

	void FlushPendingDestroy(); // Destroy marking 된 액터들을 실제 삭제

	void SwitchToLevel(ULevel* InNewLevel);
	
public:
	virtual UObject* Duplicate() override;

protected:
	virtual void DuplicateSubObjects(UObject* DuplicatedObject) override;

};
