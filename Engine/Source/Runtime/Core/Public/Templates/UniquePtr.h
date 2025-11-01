#pragma once
#include "TemplateUtilities.h"

/**
 * @brief 기본 삭제자 (단일 객체)
 */
template<typename T>
struct TDefaultDelete
{
	void operator()(T* Ptr) const
	{
		delete Ptr;
	}
};

/**
 * @brief 기본 삭제자 (배열 특수화)
 */
template<typename T>
struct TDefaultDelete<T[]>
{
	void operator()(T* Ptr) const
	{
		delete[] Ptr;
	}
};

/**
 * @brief 배타적 소유권 스마트 포인터
 * std::unique_ptr와 유사하며, 단 하나의 소유자만 허용, 복사가 불가능하고 이동만 가능
 * UObject 계열에는 사용 금지 (TObjectPtr 사용, 언리얼에서는 객체 수명 및 메모리 관리와 겹치는 면이 있어서 배제됨)
 * UPROPERTY로 선언 불가능 (현재 미구현이나, 마찬가지로 관리 시스템에서 충돌되기 때문)
 * 엔진 내부 시스템에서 배타적 소유권이 필요한 경우 사용할 것
 */
template<typename T, typename Deleter = TDefaultDelete<T>>
class TUniquePtr
{
public:
	/**
	 * @brief 기본 생성자 (nullptr 포인터)
	 */
	constexpr TUniquePtr() noexcept
		: Ptr(nullptr)
		, DeleterInstance()
	{}

	/**
	 * @brief nullptr 생성자
	 */
	constexpr TUniquePtr(decltype(nullptr)) noexcept
		: Ptr(nullptr)
		, DeleterInstance()
	{}

	/**
	 * @brief 포인터로부터 생성
	 *
	 * @param InPtr 관리할 포인터
	 */
	explicit TUniquePtr(T* InPtr) noexcept
		: Ptr(InPtr)
		, DeleterInstance()
	{}

	/**
	 * @brief 포인터와 커스텀 삭제자로부터 생성
	 *
	 * @param InPtr 관리할 포인터
	 * @param InDeleter 커스텀 삭제자
	 */
	TUniquePtr(T* InPtr, const Deleter& InDeleter) noexcept
		: Ptr(InPtr)
		, DeleterInstance(InDeleter)
	{}

	/**
	 * @brief 복사 생성자 (삭제 - 이동 전용)
	 */
	TUniquePtr(const TUniquePtr&) = delete;

	/**
	 * @brief 복사 대입 연산자 (삭제 - 이동 전용)
	 */
	TUniquePtr& operator=(const TUniquePtr&) = delete;

	/**
	 * @brief 이동 생성자
	 */
	TUniquePtr(TUniquePtr&& Other) noexcept
		: Ptr(Other.Ptr)
		, DeleterInstance(MoveTemp(Other.DeleterInstance))
	{
		Other.Ptr = nullptr;
	}

	/**
	 * @brief 다른 타입 이동 생성자
	 */
	template<typename U, typename UDeleter>
	TUniquePtr(TUniquePtr<U, UDeleter>&& Other) noexcept
		: Ptr(Other.Release())
		, DeleterInstance(MoveTemp(Other.GetDeleter()))
	{
	}

	/**
	 * @brief 이동 대입 연산자
	 */
	TUniquePtr& operator=(TUniquePtr&& Other) noexcept
	{
		if (this != &Other)
		{
			Reset(Other.Release());
			DeleterInstance = MoveTemp(Other.DeleterInstance);
		}
		return *this;
	}

	/**
	 * @brief nullptr 대입
	 */
	TUniquePtr& operator=(decltype(nullptr)) noexcept
	{
		Reset();
		return *this;
	}

	/**
	 * @brief 소멸자
	 */
	~TUniquePtr()
	{
		Reset();
	}

	/**
	 * @brief 포인터 역참조 연산자
	 * Fail-fast 원칙에 따라 nullptr 역참조 시 즉시 크래시 발생
	 */
	T* operator->() const noexcept
	{
		assert(Ptr != nullptr && "TUniquePtr::operator->()가 nullptr에서 호출되었습니다! 먼저 IsValid()를 사용하여 확인하십시오");
		return Ptr;
	}

	/**
	 * @brief 참조 연산자
	 * Fail-fast 원칙에 따라 nullptr 참조 시 즉시 크래시 발생
	 */
	T& operator*() const noexcept
	{
		assert(Ptr != nullptr && "TUniquePtr::operator*()가 nullptr에서 호출되었습니다! 먼저 IsValid()를 사용하여 확인하십시오");
		return *Ptr;
	}

	/**
	 * @brief bool 변환 연산자
	 */
	explicit operator bool() const noexcept
	{
		return IsValid();
	}

	/**
	 * @brief 소유권을 포기하고 포인터 반환
	 * TUniquePtr는 더 이상 포인터를 관리하지 않으며, 호출자가 직접 해제해야 하도록 처리됨
	 * @return 소유권 포기된 포인터
	 */
	T* Release() noexcept
	{
		T* OldPtr = Ptr;
		Ptr = nullptr;
		return OldPtr;
	}

	/**
	 * @brief 새로운 포인터로 교체
	 * 기존 객체를 파괴하고, 새 포인터를 관리
	 * @param InPtr 새로운 포인터 (nullptr 가능)
	 */
	void Reset(T* InPtr = nullptr) noexcept
	{
		if (Ptr)
		{
			DeleterInstance(Ptr);
		}
		Ptr = InPtr;
	}

	/**
	 * @brief 관리 중인 포인터 반환
	 */
	T* Get() const noexcept
	{
		return Ptr;
	}

	/**
	 * @brief 유효성 검사
	 */
	bool IsValid() const noexcept
	{
		return Ptr != nullptr;
	}

	/**
	 * @brief 삭제자 반환
	 */
	Deleter& GetDeleter() noexcept
	{
		return DeleterInstance;
	}

	/**
	 * @brief 삭제자 반환 (const)
	 */
	const Deleter& GetDeleter() const noexcept
	{
		return DeleterInstance;
	}

	// Friend 선언
	template<typename U, typename UDeleter>
	friend class TUniquePtr;

private:
	T* Ptr;
	Deleter DeleterInstance;
};

/**
 * @brief 배열용 TUniquePtr 특수화
 */
template<typename T, typename Deleter>
class TUniquePtr<T[], Deleter>
{
public:
	constexpr TUniquePtr() noexcept
		: Ptr(nullptr)
		, DeleterInstance()
	{}

	constexpr TUniquePtr(decltype(nullptr)) noexcept
		: Ptr(nullptr)
		, DeleterInstance()
	{}

	explicit TUniquePtr(T* InPtr) noexcept
		: Ptr(InPtr)
		, DeleterInstance()
	{}

	TUniquePtr(const TUniquePtr&) = delete;
	TUniquePtr& operator=(const TUniquePtr&) = delete;

	TUniquePtr(TUniquePtr&& Other) noexcept
		: Ptr(Other.Ptr)
		, DeleterInstance(MoveTemp(Other.DeleterInstance))
	{
		Other.Ptr = nullptr;
	}

	TUniquePtr& operator=(TUniquePtr&& Other) noexcept
	{
		if (this != &Other)
		{
			Reset(Other.Release());
			DeleterInstance = MoveTemp(Other.DeleterInstance);
		}
		return *this;
	}

	TUniquePtr& operator=(decltype(nullptr)) noexcept
	{
		Reset();
		return *this;
	}

	~TUniquePtr()
	{
		Reset();
	}

	/**
	 * @brief 배열 인덱스 연산자
	 */
	T& operator[](size_t Index) const noexcept
	{
		return Ptr[Index];
	}

	explicit operator bool() const noexcept
	{
		return IsValid();
	}

	T* Release() noexcept
	{
		T* OldPtr = Ptr;
		Ptr = nullptr;
		return OldPtr;
	}

	void Reset(T* InPtr = nullptr) noexcept
	{
		if (Ptr)
		{
			DeleterInstance(Ptr);
		}
		Ptr = InPtr;
	}

	T* Get() const noexcept
	{
		return Ptr;
	}

	bool IsValid() const noexcept
	{
		return Ptr != nullptr;
	}

	Deleter& GetDeleter() noexcept
	{
		return DeleterInstance;
	}

	const Deleter& GetDeleter() const noexcept
	{
		return DeleterInstance;
	}

private:
	T* Ptr;
	Deleter DeleterInstance;
};

/**
 * @brief TUniquePtr 생성 헬퍼 함수 (단일 객체)
 *
 * @param InArgs 객체 생성자에 전달할 인자들
 * @return 생성된 TUniquePtr
 */
template<typename T, typename... Args>
TUniquePtr<T> MakeUnique(Args&&... InArgs)
{
	return TUniquePtr<T>(new T(Forward<Args>(InArgs)...));
}

/**
 * @brief TUniquePtr 생성 헬퍼 함수 (배열)
 *
 * @param Size 배열 크기
 * @return 생성된 TUniquePtr
 */
template<typename T>
TUniquePtr<T[]> MakeUnique(size_t Size)
{
	return TUniquePtr<T[]>(new T[Size]);
}

// 비교 연산자
template<typename T, typename U, typename TDeleter, typename UDeleter>
bool operator==(const TUniquePtr<T, TDeleter>& Lhs, const TUniquePtr<U, UDeleter>& Rhs)
{
	return Lhs.Get() == Rhs.Get();
}

template<typename T, typename U, typename TDeleter, typename UDeleter>
bool operator!=(const TUniquePtr<T, TDeleter>& Lhs, const TUniquePtr<U, UDeleter>& Rhs)
{
	return !(Lhs == Rhs);
}

template<typename T, typename Deleter>
bool operator==(const TUniquePtr<T, Deleter>& Lhs, decltype(nullptr))
{
	return !Lhs.IsValid();
}

template<typename T, typename Deleter>
bool operator==(decltype(nullptr), const TUniquePtr<T, Deleter>& Rhs)
{
	return !Rhs.IsValid();
}

template<typename T, typename Deleter>
bool operator!=(const TUniquePtr<T, Deleter>& Lhs, decltype(nullptr))
{
	return Lhs.IsValid();
}

template<typename T, typename Deleter>
bool operator!=(decltype(nullptr), const TUniquePtr<T, Deleter>& Rhs)
{
	return Rhs.IsValid();
}
