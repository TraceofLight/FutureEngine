#pragma once
#include "Core/Public/Object.h"
#include "Component/Public/ActorComponent.h"
#include "Component/Public/SceneComponent.h"
#include "Core/Public/NewObject.h"
#include "Core/Public/Delegate.h"
#include "Physics/Public/HitResult.h"

class UUUIDTextComponent;

// Actor-level overlap event signatures
DECLARE_DELEGATE(FActorBeginOverlapSignature,
	AActor*,              /* OverlappedActor */
	AActor*               /* OtherActor */
);

DECLARE_DELEGATE(FActorEndOverlapSignature,
	AActor*,              /* OverlappedActor */
	AActor*               /* OtherActor */
);

DECLARE_DELEGATE(FActorHitSignature,
	AActor*,              /* HitActor */
	AActor*,              /* OtherActor */
	FVector,              /* NormalImpulse */
	const FHitResult&     /* Hit */
);

/**
 * @brief Level에서 렌더링되는 UObject 클래스
 * UWorld로부터 업데이트 함수가 호출되면 component들을 순회하며 위치, 애니메이션, 상태 처리
 */
UCLASS()

class AActor : public UObject
{
	GENERATED_BODY()
	DECLARE_CLASS(AActor, UObject)

public:
	AActor();
	AActor(UObject* InOuter);
	virtual ~AActor() override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	void SetActorLocation(const FVector& InLocation) const;
	void SetActorRotation(const FQuaternion& InRotation) const;
	void SetActorScale3D(const FVector& InScale) const;
	void SetUniformScale(bool IsUniform);
	virtual UClass* GetDefaultRootComponent();
	virtual void InitializeComponents();

	bool IsUniformScale() const;

	virtual void BeginPlay();
	virtual void EndPlay();
	virtual void Tick(float DeltaTimes);

	// Getter & Setter
	USceneComponent* GetRootComponent() const { return RootComponent; }
	TArray<UActorComponent*>& GetOwnedComponents()  { return OwnedComponents; }

	void SetRootComponent(USceneComponent* InOwnedComponents) { RootComponent = InOwnedComponents; }

	const FVector& GetActorLocation() const;
	const FQuaternion& GetActorRotation() const;
	const FVector& GetActorScale3D() const;

	// === Overlap Query API ===
	bool IsOverlappingActor(const AActor* OtherActor) const;
	void GetOverlappingComponents(const AActor* OtherActor, TArray<UPrimitiveComponent*>& OutComponents) const;

	// === Collision Event Delegates ===
	// Public so users can bind to these events
	FActorBeginOverlapSignature OnActorBeginOverlap;
	FActorEndOverlapSignature OnActorEndOverlap;
	FActorHitSignature OnActorHit;

	template<class T>
	T* CreateDefaultSubobject(const FName& InName = FName::None)
	{
		static_assert(is_base_of_v<UObject, T>, "생성할 클래스는 UObject를 반드시 상속 받아야 합니다");

		// 2. NewObject를 호출할 때도 템플릿 타입 T를 사용하여 정확한 타입의 컴포넌트를 생성합니다.
		T* NewComponent = NewObject<T>(this);
		if (!InName.IsNone()) { NewComponent->SetName(InName); }

		// 3. 컴포넌트 생성이 성공했는지 확인하고 기본 설정을 합니다.
		if (NewComponent)
		{
			NewComponent->SetOwner(this);
			OwnedComponents.push_back(NewComponent);
		}

		// 4. 정확한 타입(T*)으로 캐스팅 없이 바로 반환합니다.
		return NewComponent;
	}
	
	UActorComponent* CreateDefaultSubobject(UClass* Class)
	{
		UActorComponent* NewComponent = Cast<UActorComponent>(::NewObject(Class, this));
		if (NewComponent)
		{
			NewComponent->SetOwner(this);
			OwnedComponents.push_back(NewComponent);
		}
		
		return NewComponent;
	}

	/**
	 * @brief 런타임에 이 액터에 새로운 컴포넌트를 생성하고 등록합니다.
	 * @tparam T UActorComponent를 상속받는 컴포넌트 타입
	 * @param InName 컴포넌트의 이름
	 * @return 생성된 컴포넌트를 가리키는 포인터
	 */
	template<class T>
	T* AddComponent()
	{
		static_assert(std::is_base_of_v<UActorComponent, T>, "추가할 클래스는 UActorComponent를 반드시 상속 받아야 합니다");

		T* NewComponent = NewObject<T>(this);

		if (NewComponent)
		{
			RegisterComponent(NewComponent);
		}

		return NewComponent;
	}
	UActorComponent* AddComponent(UClass* InClass);

	void RegisterComponent(UActorComponent* InNewComponent);

	bool RemoveComponent(UActorComponent* InComponentToDelete, bool bShouldDetachChildren = false);

	bool CanTick() const { return bCanEverTick; }
	void SetCanTick(bool InbCanEverTick) { bCanEverTick = InbCanEverTick; }

	bool CanTickInEditor() const { return bTickInEditor; }
	void SetTickInEditor(bool InbTickInEditor) { bTickInEditor = InbTickInEditor; }

	bool IsPendingDestroy() const { return bIsPendingDestroy; }
	void SetIsPendingDestroy(bool bInIsPendingDestroy) { bIsPendingDestroy = bInIsPendingDestroy; }

protected:
	bool bCanEverTick = false;
	bool bTickInEditor = false;
	bool bBegunPlay = false;
	/** @brief True if the actor is marked for destruction. */  
	bool bIsPendingDestroy = false;

private:
	USceneComponent* RootComponent = nullptr;
	TArray<UActorComponent*> OwnedComponents;
	
public:
	virtual UObject* Duplicate() override;
	virtual void DuplicateSubObjects(UObject* DuplicatedObject) override;

	virtual UObject* DuplicateForEditor() override;
	virtual void DuplicateSubObjectsForEditor(UObject* DuplicatedObject) override;
};
