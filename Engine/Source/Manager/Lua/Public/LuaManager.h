#pragma once
#include "Core/Public/Object.h"
#include "Core/Public/WeakObjectPtr.h"
#include "Component/Public/ScriptComponent.h"
#include "sol/sol.hpp"

struct FLuaScriptInfo
{
    FLuaScriptInfo() {}
    explicit FLuaScriptInfo(path InFullPath, filesystem::file_time_type InLastModifiedTime)
        : FullPath(InFullPath), LastModifiedTime(InLastModifiedTime) {}
    path FullPath;
    filesystem::file_time_type LastModifiedTime;
    TArray<TWeakObjectPtr<class UScriptComponent>> ScriptComponents;
};

class ULuaManager: public UObject
{
    DECLARE_SINGLETON_CLASS(ULuaManager, UObject)

// Init Section
public:
    void Initialize();
    void Update(float DeltaTime);
    const TMap<FName, FLuaScriptInfo>& GetLuaScriptInfo() const { return LuaScriptCaches; }

private:
    void BindTypesToLua();
    void LoadAllLuaScripts();
    
    sol::state MasterLuaState;
    TMap<FName, FLuaScriptInfo> LuaScriptCaches;
    path LuaScriptPath;
    path LuaTemplatePath;

// Load & Create Section
public:
    sol::environment LoadLuaEnvironment(UScriptComponent* ScriptComponent, const FName& LuaScriptName);
    sol::environment CreateLuaEnvironment(UScriptComponent* ScriptComponent, const FName& LuaScriptName);
    void OpenScriptInEditor(const FName& LuaScriptName);
    void UnregisterComponent(UScriptComponent* ScriptComponent, const FName& LuaScriptName);

private:
    void CheckForHotReload();

    float HotReloadTimer = 0.0f;
    float HotReloadInterval = 1.0f;
};
