#include "pch.h"
#include "Component/Public/ScriptComponent.h"
#include "Manager/Lua/Public/LuaManager.h"
#include "Render/UI/Widget/Public/ScriptComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"

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
    }
}

void UScriptComponent::EndPlay()
{
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
    ScriptName = NewScriptName;
    LuaEnv = ULuaManager::GetInstance().LoadLuaEnvironment(this, NewScriptName);

    if (LuaEnv.valid())
    {
        LuaEnv["Owner"] = GetOwner();
        LuaEnv["BeginPlay"]();
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
    if (LuaEnv.valid())
    {
        LuaEnv["EndPlay"]();
    }

    if (NewEnv.valid())
    {
        LuaEnv = NewEnv;
        LuaEnv["Owner"] = GetOwner();
        LuaEnv["BeginPlay"]();
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
