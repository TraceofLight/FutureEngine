#pragma once
#include "Runtime/Core/Public/Templates/SharedPtr.h"
#include "Runtime/Core/Public/Templates/WeakPtr.h"

/**
 * @brief 자기 자신을 가리키는 TSharedPtr를 안전하게 생성할 수 있게 해주는 믹스인 클래스
 * 객체 내에서 TSharedPtr을 사용하기 위한 클래스
 * @warning MakeShared()를 통해 생성한 SharedPtr에서만 사용 가능하며, 완전히 생성된 뒤에야 AsShared 사용할 수 있음
 * @tparam T 실제 객체 타입 (Curiously Recurring Template Pattern)
 * @tparam Mode 스레드 안전성 모드
 * @param WeakThis 내부적으로 사용되는 WeakPtr (MakeShared에서 초기화됨)
 */
template <typename T, ESPMode Mode = ESPMode::ThreadSafe>
class TEnableSharedFromThis
{
public:
	virtual ~TEnableSharedFromThis() = default;

	/**
	 * @brief 자기 자신을 가리키는 TSharedPtr를 안전하게 생성하는 함수
	 * 내부적으로 저장된 WeakPtr를 통해 안전하게 SharedPtr를 생성
	 * 객체가 이미 파괴되었다면 빈 SharedPtr를 반환
	 * @return 자신을 가리키는 TSharedPtr (실패 시 빈 포인터)
	 */
	TSharedPtr<T, Mode> AsShared()
	{
		return WeakThis.Pin();
	}

	/**
	 * @brief const 버전의 AsShared
	 */
	TSharedPtr<const T, Mode> AsShared() const
	{
		TSharedPtr<T, Mode> SharedPtr = WeakThis.Pin();
		return StaticCastSharedPtr<const T, T, Mode>(SharedPtr);
	}

	/**
	 * @brief 현재 SharedPtr이 유효한지 확인
	 * AsShared()를 호출하기 전에 유효성을 미리 확인할 때 사용
	 * AsShared() 호출보다 가벼우므로 성능상 이점을 가져가기 위함
	 * @return SharedPtr 생성 가능하면 true
	 */
	bool HasValidSharedReference() const
	{
		return WeakThis.IsValid();
	}

	/**
	 * @brief 현재 Shared 참조 카운트 반환 (디버깅용)
	 */
	int32 GetSharedReferenceCount() const
	{
		return WeakThis.GetSharedReferenceCount();
	}

protected:
	mutable TWeakPtr<T, Mode> WeakThis;

	/**
	 * @brief TEnableSharedFromThis의 WeakPtr을 초기화
	 * @warning 이 함수는 MakeShared에서만 호출되어야 하며, 사용자가 직접 호출해서는 안됨
	 * FReferenceControllerWithObject::Create()에서 객체 생성 후 호출
	 * @param InSharedPtr 새로 생성된 SharedPtr
	 */
	void InitializeWeakThis(const TSharedPtr<T, Mode>& InSharedPtr) const
	{
		WeakThis = InSharedPtr;
	}

	// MakeShared가 InitializeWeakThis에 접근할 수 있도록 friend 선언
	template <typename U, ESPMode UMode, typename... Args>
	friend TSharedPtr<U, UMode> MakeShared(Args&&... InArgs);

	// 다른 타입의 TEnableSharedFromThis도 접근할 수 있도록 friend 선언 (상속 관계에서 필요)
	template <typename U, ESPMode UMode>
	friend class TEnableSharedFromThis;
};

/**
 * @brief TEnableSharedFromThis를 지원하는 타입인지 확인하는 Type Trait
 */
template <typename T, ESPMode Mode = ESPMode::ThreadSafe>
struct TIsSharedFromThisEnabled
{
	static constexpr bool Value = std::is_base_of_v<TEnableSharedFromThis<T, Mode>, T>;
};

/**
 * @brief TEnableSharedFromThis 초기화 헬퍼 (내부 사용용)
 * SFINAE를 사용하여 TEnableSharedFromThis를 상속받은 타입만 초기화를 수행
 */
template <typename T, ESPMode Mode>
void InitializeSharedFromThis(const TSharedPtr<T, Mode>& InSharedPtr,
                              std::enable_if_t<TIsSharedFromThisEnabled<T, Mode>::Value>* = nullptr)
{
	if (InSharedPtr)
	{
		// TEnableSharedFromThis<T>로 캐스팅하여 InitializeWeakThis 호출
		if (TEnableSharedFromThis<T, Mode>* EnableSharedFromThis = static_cast<TEnableSharedFromThis<T, Mode>*>(
			InSharedPtr.Get()))
		{
			EnableSharedFromThis->InitializeWeakThis(InSharedPtr);
		}
	}
}

/**
 * @brief TEnableSharedFromThis를 상속받지 않은 타입은 아무것도 하지 않음
 */
template <typename T, ESPMode Mode>
void InitializeSharedFromThis(const TSharedPtr<T, Mode>& InSharedPtr,
                              std::enable_if_t<!TIsSharedFromThisEnabled<T, Mode>::Value>* = nullptr)
{
	// TEnableSharedFromThis를 상속받지 않았으므로 아무것도 하지 않음
}
