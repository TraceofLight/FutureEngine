#pragma once
#include "Runtime/Core/Public/Templates/SharedPtr.h"

/**
 * @brief 약한 참조 스마트 포인터
 * TSharedPtr과 함께 사용하며, 순환 참조를 방지
 * TWeakPtr은 객체의 수명에 영향을 주지 않음
 * @tparam T 관리할 객체 타입
 * @tparam Mode 스레드 안전성 모드 (기본값: ThreadSafe)
 */
template <typename T, ESPMode Mode = ESPMode::ThreadSafe>
class TWeakPtr
{
public:
	/**
	 * @brief 기본 생성자 (nullptr 포인터)
	 */
	TWeakPtr()
		: Ptr(nullptr)
		  , RefController(nullptr)
	{
	}

	/**
	 * @brief nullptr 생성자
	 */
	TWeakPtr(decltype(nullptr))
		: Ptr(nullptr)
		  , RefController(nullptr)
	{
	}

	/**
	 * @brief TSharedPtr로부터 생성
	 */
	TWeakPtr(const TSharedPtr<T, Mode>& SharedPtr)
		: Ptr(SharedPtr.Ptr)
		  , RefController(SharedPtr.RefController)
	{
		if (RefController)
		{
			RefController->AddWeakReference();
		}
	}

	/**
	 * @brief 복사 생성자
	 */
	TWeakPtr(const TWeakPtr& Other)
		: Ptr(Other.Ptr)
		  , RefController(Other.RefController)
	{
		if (RefController)
		{
			RefController->AddWeakReference();
		}
	}

	/**
	 * @brief 타입 변환 복사 생성자
	 */
	template <typename U>
	TWeakPtr(const TWeakPtr<U, Mode>& Other)
		: Ptr(Other.Ptr)
		  , RefController(Other.RefController)
	{
		if (RefController)
		{
			RefController->AddWeakReference();
		}
	}

	/**
	 * @brief 이동 생성자
	 */
	TWeakPtr(TWeakPtr&& Other) noexcept
		: Ptr(Other.Ptr)
		  , RefController(Other.RefController)
	{
		Other.Ptr = nullptr;
		Other.RefController = nullptr;
	}

	/**
	 * @brief 소멸자
	 */
	~TWeakPtr()
	{
		if (RefController)
		{
			RefController->ReleaseWeakReference();
		}
	}

	/**
	 * @brief 복사 대입 연산자
	 * Copy-and-Swap Idiom으로 예외 안전성 보장
	 */
	TWeakPtr& operator=(const TWeakPtr& Other)
	{
		if (this != &Other)
		{
			TWeakPtr Temp(Other);
			Swap(Temp);
		}
		return *this;
	}

	/**
	 * @brief TSharedPtr 대입 연산자
	 * Copy-and-Swap Idiom으로 예외 안전성 보장
	 */
	TWeakPtr& operator=(const TSharedPtr<T, Mode>& SharedPtr)
	{
		TWeakPtr Temp(SharedPtr);
		Swap(Temp);
		return *this;
	}

	/**
	 * @brief 이동 대입 연산자
	 */
	TWeakPtr& operator=(TWeakPtr&& Other) noexcept
	{
		if (this != &Other)
		{
			// 기존 참조 해제
			if (RefController)
			{
				RefController->ReleaseWeakReference();
			}

			// 소유권 이동
			Ptr = Other.Ptr;
			RefController = Other.RefController;

			Other.Ptr = nullptr;
			Other.RefController = nullptr;
		}
		return *this;
	}

	/**
	 * @brief nullptr 대입
	 */
	TWeakPtr& operator=(decltype(nullptr)) noexcept
	{
		Reset();
		return *this;
	}

	/**
	 * @brief TSharedPtr로 승격
	 * 객체가 살아있으면 TSharedPtr을 반환하고, 파괴되었으면 빈 TSharedPtr을 반환
	 * @return 유효한 TSharedPtr 또는 nullptr
	 */
	TSharedPtr<T, Mode> Pin() const
	{
		if (IsValid())
		{
			// Shared 참조 카운트 증가
			RefController->AddSharedReference();
			return TSharedPtr<T, Mode>(Ptr, RefController);
		}
		return TSharedPtr<T, Mode>();
	}

	/**
	 * @brief 빠른 유효성 검사 (Pin()보다 가벼움)
	 * 객체가 아직 살아있는지 확인하는 함수로 Pin()과 달리 TSharedPtr을 생성하지 않기 때문에 성능이 상대적으로 우수함
	 * @return 객체가 살아있으면 true
	 */
	bool IsValid() const
	{
		return RefController && RefController->GetSharedReferenceCount() > 0;
	}

	/**
	 * @brief 객체가 파괴되었는지 확인, IsValid()의 반대 기능 함수
	 * @return 객체가 파괴되었으면 true
	 */
	bool Expired() const
	{
		return !IsValid();
	}

	/**
	 * @brief 포인터 초기화
	 */
	void Reset() noexcept
	{
		if (RefController)
		{
			RefController->ReleaseWeakReference();
		}
		Ptr = nullptr;
		RefController = nullptr;
	}

	/**
	 * @brief 두 TWeakPtr를 교환 (noexcept 보장)
	 */
	void Swap(TWeakPtr& Other) noexcept
	{
		T* TempPtr = Ptr;
		Ptr = Other.Ptr;
		Other.Ptr = TempPtr;

		FReferenceControllerBase<Mode>* TempController = RefController;
		RefController = Other.RefController;
		Other.RefController = TempController;
	}

	/**
	 * @brief 현재 참조 카운트 반환 (디버깅용)
	 */
	int32 GetSharedReferenceCount() const
	{
		return RefController ? RefController->GetSharedReferenceCount() : 0;
	}

private:
	T* Ptr;
	FReferenceControllerBase<Mode>* RefController;

	// Friend 선언
	template <typename OtherType, ESPMode OtherMode>
	friend class TWeakPtr;
};

// 비교 연산자
template <typename T, typename U, ESPMode Mode>
bool operator==(const TWeakPtr<T, Mode>& Lhs, const TWeakPtr<U, Mode>& Rhs)
{
	return Lhs.Pin() == Rhs.Pin();
}

template <typename T, typename U, ESPMode Mode>
bool operator!=(const TWeakPtr<T, Mode>& Lhs, const TWeakPtr<U, Mode>& Rhs)
{
	return !(Lhs == Rhs);
}

template <typename T, ESPMode Mode>
bool operator==(const TWeakPtr<T, Mode>& Lhs, decltype(nullptr))
{
	return Lhs.Expired();
}

template <typename T, ESPMode Mode>
bool operator==(decltype(nullptr), const TWeakPtr<T, Mode>& Rhs)
{
	return Rhs.Expired();
}

template <typename T, ESPMode Mode>
bool operator!=(const TWeakPtr<T, Mode>& Lhs, decltype(nullptr))
{
	return Lhs.IsValid();
}

template <typename T, ESPMode Mode>
bool operator!=(decltype(nullptr), const TWeakPtr<T, Mode>& Rhs)
{
	return Rhs.IsValid();
}
