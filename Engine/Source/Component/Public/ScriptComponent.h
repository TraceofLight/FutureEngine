#pragma once
#include "ActorComponent.h"
#include <sol/environment.hpp>

class UScriptComponent : public UActorComponent
{
    DECLARE_CLASS(UScriptComponent, UActorComponent)
    
public:
    UScriptComponent();
    
    void BeginPlay() override;
    void EndPlay() override;
    void TickComponent(float DeltaTime) override;
    
public:
    /**
     * @brief 캐시된 스크립트 찾아 로드하고 바인딩
     */
    void AssignScript(const FName& NewScriptName);

    /**
     * @brief 새 스크립트 생성 요청하고 바인딩
     */
    void CreateAndAssignScript(const FName& NewScriptName);

    /** 
     * @brief 현재 스크립트를 외부 편집기에서 오픈
     */
    void OpenCurrentScriptInEditor();
    
    /** 
     * @brief 기존 스크립트 정리
     */
    void ClearScript();

    void HotReload(sol::environment NewEnv);

    const FName& GetScriptFileName() const { return ScriptName; }

    /**
     * @brief Lua 콜백 함수 호출 (Delegate에서 사용)
     * @tparam Args 전달할 인자 타입들
     * @param FunctionName 호출할 Lua 함수 이름
     * @param args 전달할 인자들
     */
    template<typename... Args>
    void CallLuaCallback(const FString& FunctionName, Args&&... args)
    {
        if (!LuaEnv.valid())
        {
            return;
        }

        sol::optional<sol::protected_function> LuaFunc = LuaEnv[FunctionName.c_str()];
        if (!LuaFunc || !LuaFunc->valid())
        {
            return;
        }

        auto result = (*LuaFunc)(std::forward<Args>(args)...);
        if (!result.valid())
        {
            sol::error err = result;
            UE_LOG_ERROR("[ScriptComponent] Lua error in '%s': %s", FunctionName.c_str(), err.what());
        }
    }

private:
    FName ScriptName;
    sol::environment LuaEnv;

    // Delegate 자동 바인딩
    void BindOwnerDelegates();
    void UnbindOwnerDelegates();

    /** @brief (DelegateInfo, 바인딩ID) 쌍 저장 (해제용) */
    TArray<std::pair<FDelegateInfoBase*, uint32>> BoundDelegates;

public:
    UClass* GetSpecificWidgetClass() const override;
    UObject* Duplicate() override;
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};

// ============================================================================
// FDelegateInfo 템플릿 구현 (forward declaration 문제 해결)
// ============================================================================
#include "Core/Public/Delegate.h"

template<typename... Args>
uint32 FDelegateInfo<Args...>::AddLuaHandler(UScriptComponent* Script)
{
	if (!DelegatePtr || !Script)
	{
		return 0;  // 실패
	}

	// WeakObjectPtr로 캡처하여 RAII 패턴 적용
	TWeakObjectPtr<UScriptComponent> WeakScript(Script);
	FString DelegateName = this->Name;

	uint32 BindingID = DelegatePtr->Add([WeakScript, DelegateName](auto&&... args)
	{
		UScriptComponent* ValidScript = WeakScript.Get();
		if (ValidScript)
		{
			ValidScript->CallLuaCallback(DelegateName, std::forward<decltype(args)>(args)...);
		}
	});

	return BindingID;
}

template<typename... Args>
void FDelegateInfo<Args...>::Remove(uint32 BindingID)
{
	if (DelegatePtr && BindingID != 0)
	{
		DelegatePtr->Remove(BindingID);
	}
}
