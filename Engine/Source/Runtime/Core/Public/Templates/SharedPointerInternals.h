#pragma once
#include <atomic>

/**
 * @brief 스마트 포인터 스레드 안전성 모드
 * 스레드 안전성 여부를 취사선택할 수 있도록 하는 enum 클래스
 */
enum class ESPMode
{
	NotThreadSafe, // non-atomic, 좋은 성능
	ThreadSafe // atomic, 안전함
};

/**
 * @brief 참조 카운터 타입 선택 헬퍼
 */
template <ESPMode Mode>
struct TReferenceCounterType
{
	using Type = std::atomic<int32>;
};

template <>
struct TReferenceCounterType<ESPMode::NotThreadSafe>
{
	using Type = int32;
};

/**
 * @brief 참조 카운터 제어 블록의 베이스 클래스
 * TSharedPtr과 TWeakPtr에서 사용하는 참조 카운팅을 관리
 * - SharedReferenceCount: TSharedPtr의 참조 수
 * - WeakReferenceCount: TWeakPtr의 참조 수 (+ SharedPtr이 있으면 +1)
 * @tparam Mode 스레드 안전성 모드
 */
template <ESPMode Mode = ESPMode::ThreadSafe>
class FReferenceControllerBase
{
public:
	FReferenceControllerBase()
		: SharedReferenceCount(1)
		  , WeakReferenceCount(1)
	{
	}

	virtual ~FReferenceControllerBase() = default;

	/**
	 * @brief Shared 참조 카운트 증가
	 */
	void AddSharedReference() const
	{
		if constexpr (Mode == ESPMode::ThreadSafe)
		{
			SharedReferenceCount.fetch_add(1, std::memory_order_relaxed);
		}
		else
		{
			++SharedReferenceCount;
		}
	}

	/**
	 * @brief Shared 참조 카운트 감소 및 객체 파괴 처리
	 */
	void ReleaseSharedReference() const
	{
		int32 OldCount;
		if constexpr (Mode == ESPMode::ThreadSafe)
		{
			OldCount = SharedReferenceCount.fetch_sub(1, std::memory_order_acq_rel);
		}
		else
		{
			OldCount = SharedReferenceCount--;
		}

		if (OldCount == 1)
		{
			// 마지막 SharedPtr이 해제되면 객체 파괴
			DestroyObject();

			// WeakPtr도 없으면 제어 블록 자체 해제
			int32 OldWeakCount;
			if constexpr (Mode == ESPMode::ThreadSafe)
			{
				OldWeakCount = WeakReferenceCount.fetch_sub(1, std::memory_order_acq_rel);
			}
			else
			{
				OldWeakCount = WeakReferenceCount--;
			}

			if (OldWeakCount == 1)
			{
				DestroyThis();
			}
		}
	}

	/**
	 * @brief Weak 참조 카운트 증가
	 */
	void AddWeakReference() const
	{
		if constexpr (Mode == ESPMode::ThreadSafe)
		{
			WeakReferenceCount.fetch_add(1, std::memory_order_relaxed);
		}
		else
		{
			++WeakReferenceCount;
		}
	}

	/**
	 * @brief Weak 참조 카운트 감소 및 제어 블록 해제 처리
	 */
	void ReleaseWeakReference() const
	{
		int32 OldCount;
		if constexpr (Mode == ESPMode::ThreadSafe)
		{
			OldCount = WeakReferenceCount.fetch_sub(1, std::memory_order_acq_rel);
		}
		else
		{
			OldCount = WeakReferenceCount--;
		}

		if (OldCount == 1)
		{
			DestroyThis();
		}
	}

	/**
	 * @brief 현재 Shared 참조 카운트 반환
	 */
	int32 GetSharedReferenceCount() const
	{
		if constexpr (Mode == ESPMode::ThreadSafe)
		{
			return SharedReferenceCount.load(std::memory_order_relaxed);
		}
		else
		{
			return SharedReferenceCount;
		}
	}

	/**
	 * @brief 현재 Weak 참조 카운트 반환 (디버깅용)
	 */
	int32 GetWeakReferenceCount() const
	{
		if constexpr (Mode == ESPMode::ThreadSafe)
		{
			return WeakReferenceCount.load(std::memory_order_relaxed);
		}
		else
		{
			return WeakReferenceCount;
		}
	}

protected:
	using CounterType = TReferenceCounterType<Mode>::Type;
	mutable CounterType SharedReferenceCount;
	mutable CounterType WeakReferenceCount;

	// 관리하는 객체를 파괴
	virtual void DestroyObject() = 0;

	// 제어 블록 자체를 파괴
	virtual void DestroyThis() = 0;
};

/**
 * @brief 일반 포인터용 참조 제어 블록
 * @warning TSharedPtr(new T())처럼 직접 생성할 때 사용, MakeShared의 최적화 수혜를 받지 못하기 때문에 신경써서 사용할 것
 * 객체와 제어 블록이 분리된 메모리에 할당됨 (2회 할당)
 * @tparam T 관리할 객체 타입
 * @tparam Mode 스레드 안전성 모드
 */
template <typename T, ESPMode Mode = ESPMode::ThreadSafe>
class FReferenceController : public FReferenceControllerBase<Mode>
{
public:
	explicit FReferenceController(T* InObject)
		: Object(InObject)
	{
	}

protected:
	void DestroyObject() override
	{
		delete Object;
		Object = nullptr;
	}

	void DestroyThis() override
	{
		delete this;
	}

private:
	T* Object;
};

/**
 * @brief MakeShared 최적화용 참조 제어 블록
 * 제어 블록과 객체를 연속된 메모리에 함께 할당
 * 단 1회 메모리 할당 -> 메모리 파편화 감소 & 캐시 친화적 (제어 블록과 객체가 인접)
 * @tparam T 관리할 객체 타입
 * @tparam Mode 스레드 안전성 모드
 */
template <typename T, ESPMode Mode = ESPMode::ThreadSafe>
class FReferenceControllerWithObject : public FReferenceControllerBase<Mode>
{
public:
	/**
	 * @brief 제어 블록과 객체를 한 번에 생성
	 * 메모리 정렬을 올바르게 처리하여 안전성을 보장
	 * 전체 메모리는 두 타입 중 더 큰 정렬 요구사항을 따름
	 * 제어 블록 크기를 객체의 정렬 요구사항에 맞게 패딩
	 * 객체가 시작되는 주소가 올바른 정렬을 갖도록 보장
	 * @param InArgs 객체 생성자에 전달할 인자들
	 * @return 생성된 제어 블록 포인터
	 */
	template <typename... Args>
	static FReferenceControllerWithObject* Create(Args&&... InArgs)
	{
		// 정렬 요구사항 계산
		constexpr size_t ControllerSize = sizeof(FReferenceControllerWithObject<T, Mode>);
		constexpr size_t ControllerAlign = alignof(FReferenceControllerWithObject<T, Mode>);
		constexpr size_t ObjectAlign = alignof(T);

		// 제어 블록 크기를 객체의 정렬 요구사항에 맞게 패딩
		constexpr size_t PaddedControllerSize = (ControllerSize + ObjectAlign - 1) & ~(ObjectAlign - 1);

		// 전체 메모리 크기 및 정렬 계산
		const size_t TotalSize = PaddedControllerSize + sizeof(T);
		const size_t Alignment = ControllerAlign > ObjectAlign ? ControllerAlign : ObjectAlign;

		// 우리 엔진의 정렬 할당 new 사용 (Memory.cpp의 operator new(size_t, align_val_t) 호출)
		void* Memory = ::operator new(TotalSize, static_cast<std::align_val_t>(Alignment));

		// 제어 블록 생성 (Placement New) - 기본 생성자로 생성
		FReferenceControllerWithObject* Controller = new(Memory) FReferenceControllerWithObject();

		// 객체를 올바르게 정렬된 위치에 Placement New로 생성 - 여기서 생성자 인자 전달!
		void* ObjectMemory = reinterpret_cast<char*>(Memory) + PaddedControllerSize;
		T* ObjectPtr = new(ObjectMemory) T(Forward<Args>(InArgs)...);

		return Controller;
	}

	/**
	 * @brief 제어 블록 다음에 위치한 객체의 포인터를 반환
	 * 정렬을 고려하여 올바른 객체 위치를 계산
	 */
	T* GetObjectPtr() const
	{
		constexpr size_t ControllerSize = sizeof(FReferenceControllerWithObject<T, Mode>);
		constexpr size_t ObjectAlign = alignof(T);
		constexpr size_t PaddedControllerSize = (ControllerSize + ObjectAlign - 1) & ~(ObjectAlign - 1);

		const char* ControllerAddr = reinterpret_cast<const char*>(this);
		return reinterpret_cast<T*>(const_cast<char*>(ControllerAddr + PaddedControllerSize));
	}

protected:
	void DestroyObject() override
	{
		// 정렬을 고려하여 올바른 위치에 있는 객체의 소멸자 호출
		T* ObjectPtr = GetObjectPtr();
		ObjectPtr->~T();
	}

	void DestroyThis() override
	{
		// 정렬 값 계산 (할당 시와 동일하게)
		constexpr size_t ControllerAlign = alignof(FReferenceControllerWithObject<T, Mode>);
		constexpr size_t ObjectAlign = alignof(T);
		constexpr size_t Alignment = ControllerAlign > ObjectAlign ? ControllerAlign : ObjectAlign;

		// 소멸자 명시적 호출 (delete this는 무한 재귀 유발 가능)
		this->~FReferenceControllerWithObject();

		// 정렬 값을 명시하여 메모리 해제 (우리 엔진의 operator delete(void*, align_val_t) 호출)
		::operator delete(this, static_cast<std::align_val_t>(Alignment));
	}

private:
	// 생성자를 private으로 하여 반드시 Create()를 통해서만 생성되도록 함
	FReferenceControllerWithObject() = default;
};
