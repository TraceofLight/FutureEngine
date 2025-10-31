#pragma once
#include "Class.h"
#include "Name.h"

namespace json { class JSON; }
using JSON = json::JSON;

// Forward declaration for FUObjectItem
class UObject;

/**
 * @brief Object 배열의 각 항목을 나타내는 구조체
 * Object 포인터와 세대(generation) 번호를 함께 저장하여
 * 삭제된 객체의 슬롯 재사용을 안전하게 추적합니다.
 */
struct FUObjectItem
{
	UObject* Object;
	uint32 SerialNumber;  // 슬롯이 재사용될 때마다 증가

	FUObjectItem() : Object(nullptr), SerialNumber(0) {}
	FUObjectItem(UObject* InObject, uint32 InSerialNumber)
		: Object(InObject), SerialNumber(InSerialNumber) {}
};

UCLASS()
class UObject
{
	GENERATED_BODY()
	DECLARE_CLASS(UObject, UObject)

public:
	// 생성자 및 소멸자
	UObject();
	virtual ~UObject();

	// 가상 함수 (인터페이스)
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle);

	// Public 멤버 함수
	bool IsA(UClass* InClass) const;
	bool IsExactly(UClass* InClass) const;
	void AddMemoryUsage(uint64 InBytes, uint32 InCount);
	void RemoveMemoryUsage(uint64 InBytes, uint32 InCount);

	// Getter & Setter
	const FName& GetName() const { return Name; }
	UObject* GetOuter() const { return Outer; }
	uint64 GetAllocatedBytes() const { return AllocatedBytes; }
	uint32 GetAllocatedCount() const { return AllocatedCounts; }
	uint32 GetUUID() const { return UUID; }
	uint32 GetObjectIndex() const { return InternalIndex; }
	uint32 GetSerialNumber() const;

	FName GetName() { return Name; }
	void SetName(const FName& InName) { Name = InName; }
	void SetOuter(UObject* InObject);

	template<typename T>
	T* GetTypedOuter() const;

	virtual UObject* Duplicate();
	virtual UObject* DuplicateForEditor();

protected:
	virtual void DuplicateSubObjects(UObject* DuplicatedObject);

	/**
	 * @brief Editor 전용 하위 오브젝트 복제 (EditorOnly 포함)
	 * @param DuplicatedObject 복제된 객체
	 */
	virtual void DuplicateSubObjectsForEditor(UObject* DuplicatedObject);

private:
	// Private 멤버 변수
	uint32 UUID;
	uint32 InternalIndex;
	FName Name;
	UObject* Outer;
	uint64 AllocatedBytes = 0;
	uint32 AllocatedCounts = 0;

	// Private 멤버 함수
	void PropagateMemoryChange(uint64 InBytesDelta, uint32 InCountDelta);
};

/**
 * @brief 안전한 타입 캐스팅 함수 (원시 포인터용)
 * UClass::IsChildOf를 사용한 런타임 타입 체크
 * @tparam T 캐스팅할 대상 타입
 * @param InObject 캐스팅할 원시 포인터
 * @return 캐스팅 성공시 T*, 실패시 nullptr
 */
template <typename T>
T* Cast(UObject* InObject)
{
	static_assert(std::is_base_of_v<UObject, T>, "Cast<T>: T는 UObject를 상속받아야 합니다");

	if (!InObject)
	{
		return nullptr;
	}

	// 런타임 타입 체크: InObject가 T 타입인가?
	if (InObject->IsA(T::StaticClass()))
	{
		return static_cast<T*>(InObject);
	}

	return nullptr;
}

/**
 * @brief 안전한 타입 캐스팅 함수 (const 원시 포인터용)
 * @tparam T 캐스팅할 대상 타입
 * @param InObject 캐스팅할 const 원시 포인터
 * @return 캐스팅 성공시 const T*, 실패시 nullptr
 */
template <typename T>
const T* Cast(const UObject* InObject)
{
	static_assert(std::is_base_of_v<UObject, T>, "Cast<T>: T는 UObject를 상속받아야 합니다");

	if (!InObject)
	{
		return nullptr;
	}

	// 런타임 타입 체크: InObject가 T 타입인가?
	if (InObject->IsA(T::StaticClass()))
	{
		return static_cast<const T*>(InObject);
	}

	return nullptr;
}

/**
 * @brief Outer 체인을 타고 올라가며 특정 타입의 Outer 탐색하는 함수
 * @tparam T 찾을 Outer 타입 (UObject 상속)
 * @return 찾은 Outer 또는 nullptr
 */
template<typename T>
T* UObject::GetTypedOuter() const
{
	static_assert(std::is_base_of_v<UObject, T>, "GetTypedOuter<T>: T must inherit from UObject");

	UObject* CurrentOuter = Outer;
	while (CurrentOuter)
	{
		if (T* TypedOuter = Cast<T>(CurrentOuter))
		{
			return TypedOuter;
		}
		CurrentOuter = CurrentOuter->GetOuter();
	}
	return nullptr;
}

/**
 * @brief 빠른 캐스팅 함수 (런타임 체크 없음, 디버그에서만 체크)
 * 성능이 중요한 상황에서 사용, 타입 안전성을 개발자가 보장해야 함
 * @tparam T 캐스팅할 대상 타입
 * @param InObject 캐스팅할 원시 포인터
 * @return T* (런타임 체크 없이 static_cast)
 */
template <typename T>
T* CastChecked(UObject* InObject)
{
	static_assert(std::is_base_of_v<UObject, T>, "CastChecked<T>: T는 UObject를 상속받아야 합니다");

#ifdef _DEBUG
	// 디버그 빌드에서는 안전성 체크
	if (InObject && !InObject->IsA(T::StaticClass()))
	{
		// 로그나 assert 추가 가능
		return nullptr;
	}
#endif

	return static_cast<T*>(InObject);
}

/**
 * @brief 타입 체크 함수 (캐스팅하지 않고 체크만)
 * @tparam T 체크할 타입
 * @param InObject 체크할 객체
 * @return InObject가 T 타입이면 true
 */
template <typename T>
bool IsA(const UObject* InObject)
{
	static_assert(std::is_base_of_v<UObject, T>, "IsA<T>: T는 UObject를 상속받아야 합니다");

	if (!InObject)
	{
		return false;
	}

	return InObject->IsA(T::StaticClass());
}

/**
 * @brief 유효성과 타입을 동시에 체크하는 함수
 * @tparam T 체크할 타입
 * @param InObject 체크할 객체
 * @return 객체가 유효하고 T 타입이면 true
 */
template <typename T>
bool IsValid(const UObject* InObject)
{
	return InObject && IsA<T>(InObject);
}

/**
 * @brief 전역 UObject 배열을 반환하는 함수
 * FUObjectItem 구조체로 객체 포인터와 세대 번호를 함께 관리합니다.
 * @return GUObjectArray 참조
 */
TArray<FUObjectItem>& GetUObjectArray();
