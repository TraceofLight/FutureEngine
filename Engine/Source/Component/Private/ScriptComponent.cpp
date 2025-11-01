#include "pch.h"
#include "Component/Public/ScriptComponent.h"
#include "Manager/Lua/Public/LuaManager.h"
#include "Render/UI/Widget/Public/ScriptComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"
#include "Actor/Public/Actor.h"

IMPLEMENT_CLASS(UScriptComponent, UActorComponent)

UScriptComponent::UScriptComponent()
{
    bCanEverTick = true;
}


void UScriptComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!ScriptName.IsNone())
    {
        AssignScript(ScriptName);
    }

    if (LuaEnv.valid())
    {
        LuaEnv["BeginPlay"]();

        // Delegate 자동 바인딩
        BindOwnerDelegates();
    }
}

void UScriptComponent::EndPlay()
{
    // Delegate 해제
    UnbindOwnerDelegates();

    ClearScript();
}

void UScriptComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
    if (LuaEnv.valid())
    {
        LuaEnv["Tick"](DeltaTime);
    }
}

void UScriptComponent::AssignScript(const FName& NewScriptName)
{
    UE_LOG("Assign Script: %s", NewScriptName.ToString().c_str());

    // 기존 바인딩 해제 (중복 방지)
    UnbindOwnerDelegates();

    ScriptName = NewScriptName;
    LuaEnv = ULuaManager::GetInstance().LoadLuaEnvironment(this, NewScriptName);

    if (LuaEnv.valid())
    {
        LuaEnv["Owner"] = GetOwner();
        LuaEnv["BeginPlay"]();

        // Delegate 자동 바인딩
        BindOwnerDelegates();
    }
    else
    {
        UE_LOG_ERROR("[UScriptComponent] '%s' 스크립트 로드 실패", ScriptName.ToString().c_str());
        ScriptName = FName();
    }
}

void UScriptComponent::CreateAndAssignScript(const FName& NewScriptName)
{
    if (ScriptName == NewScriptName)
    {
        UE_LOG_ERROR("[UScriptComponent] 현재 할당된 스크립트와 이름(%s)이 동일함", NewScriptName.ToString().c_str());
        return;
    }
    ClearScript();

    LuaEnv = ULuaManager::GetInstance().CreateLuaEnvironment(this, NewScriptName);
    if (LuaEnv.valid())
    {
        ScriptName = NewScriptName;
        LuaEnv["Owner"] = GetOwner();
        LuaEnv["BeginPlay"]();

        // Delegate 자동 바인딩
        BindOwnerDelegates();
    }
    else
    {
        UE_LOG_ERROR("[UScriptComponent] '%s' 스크립트 생성 실패", NewScriptName.ToString().c_str());
    }
}

void UScriptComponent::OpenCurrentScriptInEditor()
{
    if (ScriptName.IsNone())
    {
        UE_LOG("[UScriptComponent] 편집할 스크립트가 할당되지 않았습니다.");
        return;
    }
    ULuaManager::GetInstance().OpenScriptInEditor(ScriptName);
}

void UScriptComponent::ClearScript()
{
    // Delegate 해제
    UnbindOwnerDelegates();

    if (LuaEnv.valid())
    {
        LuaEnv["EndPlay"]();
    }

    if (!ScriptName.IsNone())
    {
        ULuaManager::GetInstance().UnregisterComponent(this, ScriptName);
    }

    LuaEnv = sol::environment{};
    ScriptName = FName();
}

void UScriptComponent::HotReload(sol::environment NewEnv)
{
    // 기존 Delegate 해제
    UnbindOwnerDelegates();

    if (LuaEnv.valid())
    {
        LuaEnv["EndPlay"]();
    }

    if (NewEnv.valid())
    {
        LuaEnv = NewEnv;
        LuaEnv["Owner"] = GetOwner();
        LuaEnv["BeginPlay"]();

        // 새 환경으로 Delegate 재바인딩
        BindOwnerDelegates();
    }
    else
    {
        LuaEnv = sol::environment{};
    }
}

UClass* UScriptComponent::GetSpecificWidgetClass() const
{
    return UScriptComponentWidget::StaticClass();
}

UObject* UScriptComponent::Duplicate()
{
    UScriptComponent* Duplicated = Cast<UScriptComponent>(Super::Duplicate());
    Duplicated->ScriptName = ScriptName;
    return Duplicated;
}

void UScriptComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
    if (bInIsLoading)
    {
        FString ScriptNameStr;
        FJsonSerializer::ReadString(InOutHandle, "ScriptName", ScriptNameStr, "");
        ScriptName = ScriptNameStr;

    }
    else
    {
        if (!ScriptName.IsNone())
        {
            InOutHandle["ScriptName"] = ScriptName.ToString();
        }
    }
}

void UScriptComponent::BindOwnerDelegates()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    const TArray<FDelegateInfoBase*>& Delegates = Owner->GetAllDelegates();
    for (FDelegateInfoBase* DelegateInfo : Delegates)
    {
        if (DelegateInfo)
        {
            uint32 BindingID = DelegateInfo->AddLuaHandler(this);
            if (BindingID != 0)
            {
                BoundDelegates.Add({DelegateInfo, BindingID});
            }
        }
    }
}

void UScriptComponent::UnbindOwnerDelegates()
{
    // 저장된 바인딩 ID로 명시적으로 제거
    for (const auto& Pair : BoundDelegates)
    {
        FDelegateInfoBase* DelegateInfo = Pair.first;
        uint32 BindingID = Pair.second;

        if (DelegateInfo)
        {
            DelegateInfo->Remove(BindingID);
        }
    }

    BoundDelegates.Empty();
}
