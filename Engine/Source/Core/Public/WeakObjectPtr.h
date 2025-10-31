#pragma once
#include "ObjectHandle.h"
#include "Object.h"

/**
 * @brief UObject에 대한 약한 참조(Weak Reference) 템플릿 클래스
 *
 * TWeakObjectPtr는 UObject를 참조하되, 객체의 생명주기에 영향을 주지 않습니다.
 * 객체가 삭제되면 자동으로 nullptr을 반환하여 안전하게 사용할 수 있습니다.
 *
 * 사용 예시:
 * @code
 * TWeakObjectPtr<AActor> WeakActor = MyActor;
 * if (AActor* Actor = WeakActor.Get())
 * {
 *     // 안전하게 사용
 * }
 * @endcode
 *
 * @tparam T UObject를 상속받는 타입
 */
template<typename T = UObject>
class TWeakObjectPtr
{
private:
	FObjectHandle Handle;

	// Helper to validate type at compile-time when methods are actually used
	// This allows forward declarations to work, with validation deferred until instantiation
	static constexpr void ValidateType()
	{
		static_assert(std::is_base_of_v<UObject, T>, "T must inherit from UObject");
	}

public:
	/**
	 * @brief 기본 생성자 - 무효한 약한 참조 생성
	 */
	TWeakObjectPtr()
		: Handle()
	{
	}

	/**
	 * @brief 객체로부터 약한 참조 생성
	 * @param Object 참조할 객체 (nullptr 가능)
	 */
	TWeakObjectPtr(T* Object)
		: Handle(Object)
	{
	}

	/**
	 * @brief 복사 생성자
	 */
	TWeakObjectPtr(const TWeakObjectPtr& Other) = default;

	/**
	 * @brief 대입 연산자
	 */
	TWeakObjectPtr& operator=(const TWeakObjectPtr& Other) = default;

	/**
	 * @brief 객체 대입
	 */
	TWeakObjectPtr& operator=(T* Object)
	{
		Handle = FObjectHandle(Object);
		return *this;
	}

	/**
	 * @brief 참조하는 객체를 가져옴
	 * @return 유효한 객체 포인터 또는 nullptr (객체가 삭제된 경우)
	 */
	T* Get() const
	{
		ValidateType();
		UObject* Object = Handle.Get();
		return Cast<T>(Object);
	}

	/**
	 * @brief 약한 참조가 유효한지 확인
	 * @return 객체가 살아있으면 true
	 */
	bool IsValid() const
	{
		return Get() != nullptr;
	}

	/**
	 * @brief 약한 참조를 무효화
	 */
	void Reset()
	{
		Handle.Reset();
	}

	/**
	 * @brief 포인터 역참조 연산자
	 * @warning 객체가 유효한지 먼저 확인해야 합니다!
	 */
	T* operator->() const
	{
		return Get();
	}

	/**
	 * @brief 참조 역참조 연산자
	 * @warning 객체가 유효한지 먼저 확인해야 합니다!
	 */
	T& operator*() const
	{
		return *Get();
	}

	/**
	 * @brief bool 변환 연산자 - IsValid()와 동일
	 */
	explicit operator bool() const
	{
		return IsValid();
	}

	/**
	 * @brief 비교 연산자 - 동일한 객체를 가리키는지 확인
	 */
	bool operator==(const TWeakObjectPtr& Other) const
	{
		return Handle == Other.Handle;
	}

	bool operator!=(const TWeakObjectPtr& Other) const
	{
		return Handle != Other.Handle;
	}

	/**
	 * @brief nullptr과의 비교
	 */
	bool operator==(std::nullptr_t) const
	{
		return !IsValid();
	}

	bool operator!=(std::nullptr_t) const
	{
		return IsValid();
	}

	/**
	 * @brief 내부 핸들 가져오기 (고급 사용)
	 */
	const FObjectHandle& GetHandle() const
	{
		return Handle;
	}
};

/**
 * @brief TWeakObjectPtr에 대한 해시 함수 (TMap/TSet에서 사용)
 */
template<typename T>
struct std::hash<TWeakObjectPtr<T>>
{
	size_t operator()(const TWeakObjectPtr<T>& WeakPtr) const
	{
		return std::hash<FObjectHandle>()(WeakPtr.GetHandle());
	}
};
