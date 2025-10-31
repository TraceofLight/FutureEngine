#pragma once
#include "WeakObjectPtr.h"
#include <functional>
#include <vector>

/**
 * @brief 다중 바인딩을 지원하는 델리게이트 템플릿 클래스
 *
 * TDelegate는 여러 함수나 객체 메서드를 바인딩하고,
 * Broadcast를 통해 모든 바인딩을 한번에 호출할 수 있습니다.
 *
 * - Add(): 일반 함수, 람다 등록
 * - AddDynamic(): UObject 멤버 함수 등록 (안전한 약한 참조 사용)
 * - Broadcast(): 모든 핸들러 실행
 *
 * 사용 예시:
 * @code
 * DECLARE_DELEGATE(FOnDamaged, int, float);
 * FOnDamaged OnDamaged;
 *
 * // 람다 등록
 * OnDamaged.Add([](int Dmg, float Mult) {
 *     UE_LOG("Damage: %d", Dmg);
 * });
 *
 * // UObject 메서드 등록
 * OnDamaged.AddDynamic(this, &AMyActor::OnDamageReceived);
 *
 * // 모든 핸들러 호출
 * OnDamaged.Broadcast(100, 1.5f);
 * @endcode
 *
 * @tparam Args 델리게이트 파라미터 타입들
 */
template<typename... Args>
class TDelegate
{
public:
	using HandlerType = std::function<void(Args...)>;

	/**
	 * @brief 일반 함수나 람다를 등록
	 * @param Handler 등록할 핸들러 (람다, 함수 포인터, functor 등)
	 * @return 바인딩 ID (나중에 Remove에 사용)
	 */
	uint32 Add(const HandlerType& Handler)
	{
		FDelegateBinding Binding;
		Binding.Handler = Handler;
		Binding.WeakObject = nullptr;  // 일반 핸들러는 객체 없음
		Binding.DelegateID = NextDelegateID++;

		Bindings.push_back(Binding);
		return Binding.DelegateID;
	}

	/**
	 * @brief UObject 멤버 함수를 안전하게 바인딩
	 *
	 * WeakObjectPtr를 사용하여 객체가 삭제되면 자동으로
	 * 바인딩을 건너뛰어 크래시를 방지합니다.
	 *
	 * @tparam T UObject를 상속받는 타입
	 * @param Instance 객체 인스턴스
	 * @param Func 멤버 함수 포인터
	 * @return 바인딩 ID
	 */
	template<typename T>
	uint32 AddDynamic(T* Instance, void (T::*Func)(Args...))
	{
		static_assert(std::is_base_of_v<UObject, T>,
		              "AddDynamic: T must inherit from UObject");

		if (!Instance)
		{
			return 0;  // 무효한 ID
		}

		FDelegateBinding Binding;

		// WeakObjectPtr로 안전하게 저장
		Binding.WeakObject = Instance;

		// 람다로 멤버 함수 호출을 래핑
		// WeakObject를 캡처하여 실행 시마다 유효성 검사
		Binding.Handler = [WeakObj = TWeakObjectPtr<T>(Instance), Func](Args... args)
		{
			if (T* Obj = WeakObj.Get())
			{
				(Obj->*Func)(args...);
			}
		};

		Binding.DelegateID = NextDelegateID++;
		Bindings.push_back(Binding);

		return Binding.DelegateID;
	}

	/**
	 * @brief 모든 바인딩된 핸들러를 실행
	 *
	 * 실행 중 삭제된 객체의 바인딩은 자동으로 제거됩니다.
	 * 역순으로 순회하여 안전하게 제거합니다.
	 *
	 * @param args 핸들러에 전달할 인자들
	 */
	void Broadcast(Args... args)
	{
		// 역순으로 순회하며 무효한 바인딩 제거
		for (int i = static_cast<int>(Bindings.size()) - 1; i >= 0; --i)
		{
			FDelegateBinding& Binding = Bindings[i];

			// UObject 바인딩인 경우 유효성 검사
			if (Binding.WeakObject != nullptr)
			{
				if (Binding.WeakObject.IsValid())
				{
					// 객체가 살아있으면 실행
					Binding.Handler(args...);
				}
				else
				{
					// 객체가 삭제되었으면 바인딩 제거
					Bindings.erase(Bindings.begin() + i);
				}
			}
			else
			{
				// 일반 핸들러 (람다 등) - 그냥 실행
				Binding.Handler(args...);
			}
		}
	}

	/**
	 * @brief 특정 바인딩을 ID로 제거
	 * @param DelegateID Add/AddDynamic에서 반환된 ID
	 */
	void Remove(uint32 DelegateID)
	{
		Bindings.erase(
			std::remove_if(Bindings.begin(), Bindings.end(),
				[DelegateID](const FDelegateBinding& B) {
					return B.DelegateID == DelegateID;
				}),
			Bindings.end()
		);
	}

	/**
	 * @brief 특정 UObject의 모든 바인딩 제거
	 * @param Object 제거할 객체
	 */
	void RemoveAll(UObject* Object)
	{
		if (!Object)
		{
			return;
		}

		Bindings.erase(
			std::remove_if(Bindings.begin(), Bindings.end(),
				[Object](const FDelegateBinding& B) {
					return B.WeakObject.Get() == Object;
				}),
			Bindings.end()
		);
	}

	/**
	 * @brief 모든 바인딩 제거
	 */
	void Clear()
	{
		Bindings.clear();
	}

	/**
	 * @brief 바인딩이 하나라도 있는지 확인
	 * @return 바인딩이 있으면 true
	 */
	bool IsBound() const
	{
		return !Bindings.empty();
	}

	/**
	 * @brief 현재 바인딩 개수 반환
	 * @return 바인딩된 핸들러 수
	 */
	size_t Num() const
	{
		return Bindings.size();
	}

private:
	/**
	 * @brief 델리게이트 바인딩 정보를 저장하는 구조체
	 */
	struct FDelegateBinding
	{
		HandlerType Handler;                      // 실제 호출할 함수
		TWeakObjectPtr<UObject> WeakObject;       // AddDynamic용 약한 참조
		uint32 DelegateID;                         // 고유 식별자

		FDelegateBinding()
			: Handler(nullptr)
			, WeakObject(nullptr)
			, DelegateID(0)
		{
		}
	};

	std::vector<FDelegateBinding> Bindings;  // 모든 바인딩 저장
	uint32 NextDelegateID = 1;                // 다음 할당할 ID (0은 무효)
};

/**
 * @brief 델리게이트 선언 매크로 (가변인자 지원)
 *
 * 파라미터 개수에 상관없이 사용 가능합니다.
 *
 * 사용 예시:
 * @code
 * DECLARE_DELEGATE(FSimpleDelegate);                   // void()
 * DECLARE_DELEGATE(FOnHealthChanged, int);             // void(int)
 * DECLARE_DELEGATE(FOnDamaged, int, float);            // void(int, float)
 * DECLARE_DELEGATE(FOnEvent, AActor*, bool, FVector);  // void(AActor*, bool, FVector)
 * @endcode
 *
 * @param DelegateName 델리게이트 타입 이름
 * @param ... 파라미터 타입들 (선택 사항, 없으면 void())
 */
#define DECLARE_DELEGATE(DelegateName, ...) \
	typedef TDelegate<__VA_ARGS__> DelegateName
