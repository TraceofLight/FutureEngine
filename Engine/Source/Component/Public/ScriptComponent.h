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
    
private:
    FName ScriptName;
    sol::environment LuaEnv;

public:
    UClass* GetSpecificWidgetClass() const override;
    UObject* Duplicate() override;
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
