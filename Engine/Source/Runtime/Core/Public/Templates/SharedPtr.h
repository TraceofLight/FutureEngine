#pragma once
#include "Runtime/Core/Public/Templates/SharedPointerInternals.h"

// Forward declarations
template <typename T, ESPMode Mode>
class TWeakPtr;

/**
 * @brief 참조 카운팅 기반 공유 소유권 스마트 포인터
 *
 * std::shared_ptr와 유사하며, 여러 TSharedPtr이 하나의 객체를 공유할 수 있음
 * 마지막 TSharedPtr이 소멸되면 객체도 자동으로 파괴
 * UObject 계열에는 사용 금지 (TObjectPtr 사용, 언리얼에서는 객체 수명 및 메모리 관리와 겹치는 면이 있어서 배제됨)
 * UPROPERTY로 선언 불가능 (현재 미구현이나, 마찬가지로 관리 시스템에서 충돌되기 때문)
 * 성능을 위해 MakeShared<T>() 사용을 권장
 * @tparam T 관리할 객체 타입
 * @tparam Mode 스레드 안전성 모드 (기본값: ThreadSafe)
 */
template <typename T, ESPMode Mode = ESPMode::ThreadSafe>
class TSharedPtr
{
public:
	/**
	 * @brief 기본 생성자 (nullptr 포인터)
	 */
	TSharedPtr()
		: Ptr(nullptr)
		  , RefController(nullptr)
	{
	}

	/**
	 * @brief nullptr 생성자
	 */
	TSharedPtr(decltype(nullptr))
		: Ptr(nullptr)
		  , RefController(nullptr)
	{
	}

	/**
	 * @brief 일반 포인터로부터 생성 (2회 할당)
	 *
	 * 주의: 성능을 위해 MakeShared<T>() 사용을 권장합니다!
	 *
	 * @param InPtr 관리할 포인터
	 */
	explicit TSharedPtr(T* InPtr)
		: Ptr(InPtr)
		  , RefController(InPtr ? new FReferenceController<T, Mode>(InPtr) : nullptr)
	{
	}

	/**
	 * @brief 복사 생성자
	 */
	TSharedPtr(const TSharedPtr& Other)
		: Ptr(Other.Ptr)
		  , RefController(Other.RefController)
	{
		if (RefController)
		{
			RefController->AddSharedReference();
		}
	}

	/**
	 * @brief 타입 변환 복사 생성자 (업캐스팅)
	 */
	template <typename U>
	TSharedPtr(const TSharedPtr<U, Mode>& Other)
		: Ptr(Other.Ptr)
		  , RefController(Other.RefController)
	{
		if (RefController)
		{
			RefController->AddSharedReference();
		}
	}

	/**
	 * @brief 이동 생성자
	 */
	TSharedPtr(TSharedPtr&& Other) noexcept
		: Ptr(Other.Ptr)
		  , RefController(Other.RefController)
	{
		Other.Ptr = nullptr;
		Other.RefController = nullptr;
	}

	/**
	 * @brief 타입 변환 이동 생성자
	 */
	template <typename U>
	TSharedPtr(TSharedPtr<U, Mode>&& Other) noexcept
		: Ptr(Other.Ptr)
		  , RefController(Other.RefController)
	{
		Other.Ptr = nullptr;
		Other.RefController = nullptr;
	}

	/**
	 * @brief 소멸자
	 */
	~TSharedPtr()
	{
		if (RefController)
		{
			RefController->ReleaseSharedReference();
		}
	}

	/**
	 * @brief 복사 대입 연산자 (Copy-and-Swap Idiom으로 예외 안전성 보장)
	 */
	TSharedPtr& operator=(const TSharedPtr& Other)
	{
		if (this != &Other)
		{
			TSharedPtr Temp(Other); // 복사 생성
			Swap(Temp); // noexcept swap
		}
		return *this;
	}

	/**
	 * @brief 이동 대입 연산자
	 */
	TSharedPtr& operator=(TSharedPtr&& Other) noexcept
	{
		if (this != &Other)
		{
			// 기존 참조 해제
			if (RefController)
			{
				RefController->ReleaseSharedReference();
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
	TSharedPtr& operator=(decltype(nullptr)) noexcept
	{
		Reset();
		return *this;
	}

	/**
	 * @brief 포인터 역참조 연산자
	 *
	 * 언리얼 엔진의 "Fail-fast" 원칙에 따라 nullptr 역참조 시 즉시 크래시 발생
	 */
	T* operator->() const
	{
		// Null pointer 역참조 방지 - 언리얼 엔진 스타일의 Fail-fast 원칙
		assert(Ptr != nullptr && "TSharedPtr::operator->() called on null pointer! Use IsValid() to check first.");
		return Ptr;
	}

	/**
	 * @brief 참조 연산자
	 *
	 * 언리얼 엔진의 "Fail-fast" 원칙에 따라 nullptr 역참조 시 즉시 크래시 발생
	 */
	T& operator*() const
	{
		// Null pointer 역참조 방지 - 언리얼 엔진 스타일의 Fail-fast 원칙
		assert(Ptr != nullptr && "TSharedPtr::operator*() called on null pointer! Use IsValid() to check first.");
		return *Ptr;
	}

	/**
	 * @brief bool 변환 연산자
	 */
	explicit operator bool() const
	{
		return IsValid();
	}

	/**
	 * @brief 포인터 초기화
	 */
	void Reset() noexcept
	{
		if (RefController)
		{
			RefController->ReleaseSharedReference();
		}
		Ptr = nullptr;
		RefController = nullptr;
	}

	/**
	 * @brief 두 TSharedPtr를 교환 (noexcept 보장)
	 */
	void Swap(TSharedPtr& Other) noexcept
	{
		T* TempPtr = Ptr;
		Ptr = Other.Ptr;
		Other.Ptr = TempPtr;

		FReferenceControllerBase* TempController = RefController;
		RefController = Other.RefController;
		Other.RefController = TempController;
	}

	/**
	 * @brief 관리 중인 포인터 반환
	 */
	T* Get() const
	{
		return Ptr;
	}

	/**
	 * @brief 유효성 검사
	 */
	bool IsValid() const
	{
		return Ptr != nullptr;
	}

	/**
	 * @brief 현재 Shared 참조 카운트 반환
	 */
	int32 GetSharedReferenceCount() const
	{
		return RefController ? RefController->GetSharedReferenceCount() : 0;
	}

	/**
	 * @brief 고유한 소유자인지 확인 (참조 카운트가 1인지)
	 */
	bool IsUnique() const
	{
		return GetSharedReferenceCount() == 1;
	}

private:
	T* Ptr;
	FReferenceControllerBase<Mode>* RefController;

	// MakeShared를 위한 특수 생성자 (private)
	TSharedPtr(T* InPtr, FReferenceControllerBase<Mode>* InRefController)
		: Ptr(InPtr)
		  , RefController(InRefController)
	{
	}

	// Friend 선언
	template <typename U, ESPMode UMode, typename... Args>
	friend TSharedPtr<U, UMode> MakeShared(Args&&... InArgs);

	template <typename U, ESPMode UMode>
	friend class TSharedPtr;

	template <typename U, ESPMode UMode>
	friend class TWeakPtr;

	template <typename To, typename From, ESPMode CastMode>
	friend TSharedPtr<To, CastMode> StaticCastSharedPtr(const TSharedPtr<From, CastMode>&);

	template <typename To, typename From, ESPMode CastMode>
	friend TSharedPtr<To, CastMode> DynamicCastSharedPtr(const TSharedPtr<From, CastMode>&);

	template <typename To, typename From, ESPMode CastMode>
	friend TSharedPtr<To, CastMode> ConstCastSharedPtr(const TSharedPtr<From, CastMode>&);
};

/**
 * @brief 최적화된 TSharedPtr 생성 함수 (단일 메모리 할당)
 *
 * 제어 블록과 객체를 연속된 메모리에 함께 할당하여 성능을 최적화합니다.
 * - 할당 횟수: 1회 (일반 생성자는 2회)
 * - 캐시 친화성 향상
 * - 메모리 파편화 감소
 * - 올바른 메모리 정렬 보장
 *
 * @tparam T 관리할 객체 타입
 * @tparam Mode 스레드 안전성 모드 (기본값: ThreadSafe)
 * @param InArgs 객체 생성자에 전달할 인자들
 * @return 생성된 TSharedPtr
 */
template <typename T, ESPMode Mode = ESPMode::ThreadSafe, typename... Args>
TSharedPtr<T, Mode> MakeShared(Args&&... InArgs)
{
	// 제어 블록과 객체를 한 번에 할당하고 생성
	FReferenceControllerWithObject<T, Mode>* Controller =
		FReferenceControllerWithObject<T, Mode>::Create(Forward<Args>(InArgs)...);

	// 객체 포인터는 올바르게 정렬된 위치에 존재
	T* ObjectPtr = Controller->GetObjectPtr();

	// TSharedPtr 생성 (특수 생성자 사용)
	TSharedPtr<T, Mode> SharedPtr(ObjectPtr, Controller);

	// TEnableSharedFromThis 지원: 객체가 TEnableSharedFromThis를 상속받았다면 WeakThis 초기화
	// 이 부분은 EnableSharedFromThis.h에서 include 해야 함

	return SharedPtr;
}

/**
 * @brief Static Cast
 * 기존 제어 블록을 공유하는 새로운 TSharedPtr를 생성
 * 참조 카운트를 올바르게 관리하여 메모리 누수를 방지
 */
template <typename To, typename From, ESPMode Mode = ESPMode::ThreadSafe>
TSharedPtr<To, Mode> StaticCastSharedPtr(const TSharedPtr<From, Mode>& InSharedPtr)
{
	if (!InSharedPtr.IsValid())
	{
		return TSharedPtr<To, Mode>();
	}

	To* CastedPtr = static_cast<To*>(InSharedPtr.Ptr);

	// 복사 생성을 통해 참조 카운트를 올바르게 관리
	TSharedPtr<To, Mode> Result(CastedPtr, InSharedPtr.RefController);
	if (InSharedPtr.RefController)
	{
		InSharedPtr.RefController->AddSharedReference();
	}

	return Result;
}

/**
 * @brief Dynamic Cast
 * 런타임에 타입 체크를 수행하는 함수
 * 캐스팅이 성공하면 기존 제어 블록을 공유하는 새로운 TSharedPtr를 반환, 실패하면 빈 TSharedPtr를 반환
 */
template <typename To, typename From, ESPMode Mode = ESPMode::ThreadSafe>
TSharedPtr<To, Mode> DynamicCastSharedPtr(const TSharedPtr<From, Mode>& InSharedPtr)
{
	if (!InSharedPtr.IsValid())
	{
		return TSharedPtr<To, Mode>();
	}

	To* CastedPtr = dynamic_cast<To*>(InSharedPtr.Ptr);

	if (CastedPtr)
	{
		// 캐스팅 성공: 복사 생성을 통해 참조 카운트를 올바르게 관리
		TSharedPtr<To, Mode> Result(CastedPtr, InSharedPtr.RefController);
		if (InSharedPtr.RefController)
		{
			InSharedPtr.RefController->AddSharedReference();
		}
		return Result;
	}

	// 캐스팅 실패: 빈 포인터 반환
	return TSharedPtr<To, Mode>();
}

/**
 * @brief Const Cast
 * const 한정자를 제거/추가하고, 기존 제어 블록을 공유하는 새로운 TSharedPtr를 생성
 * 참조 카운트를 올바르게 관리하여 메모리 누수를 방지
 */
template <typename To, typename From, ESPMode Mode = ESPMode::ThreadSafe>
TSharedPtr<To, Mode> ConstCastSharedPtr(const TSharedPtr<From, Mode>& InSharedPtr)
{
	if (!InSharedPtr.IsValid())
	{
		return TSharedPtr<To, Mode>();
	}

	To* CastedPtr = const_cast<To*>(InSharedPtr.Ptr);

	// 복사 생성을 통해 참조 카운트를 올바르게 관리
	TSharedPtr<To, Mode> Result(CastedPtr, InSharedPtr.RefController);
	if (InSharedPtr.RefController)
	{
		InSharedPtr.RefController->AddSharedReference();
	}

	return Result;
}

// 비교 연산자
template <typename T, typename U, ESPMode Mode>
bool operator==(const TSharedPtr<T, Mode>& Lhs, const TSharedPtr<U, Mode>& Rhs)
{
	return Lhs.Get() == Rhs.Get();
}

template <typename T, typename U, ESPMode Mode>
bool operator!=(const TSharedPtr<T, Mode>& Lhs, const TSharedPtr<U, Mode>& Rhs)
{
	return !(Lhs == Rhs);
}

template <typename T, ESPMode Mode>
bool operator==(const TSharedPtr<T, Mode>& Lhs, decltype(nullptr))
{
	return !Lhs.IsValid();
}

template <typename T, ESPMode Mode>
bool operator==(decltype(nullptr), const TSharedPtr<T, Mode>& Rhs)
{
	return !Rhs.IsValid();
}

template <typename T, ESPMode Mode>
bool operator!=(const TSharedPtr<T, Mode>& Lhs, decltype(nullptr))
{
	return Lhs.IsValid();
}

template <typename T, ESPMode Mode>
bool operator!=(decltype(nullptr), const TSharedPtr<T, Mode>& Rhs)
{
	return Rhs.IsValid();
}
