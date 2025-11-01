#include "pch.h"
#include "Manager/Lua/Public/LuaManager.h"
#include "Manager/Path/Public/PathManager.h"

IMPLEMENT_SINGLETON_CLASS(ULuaManager, UObject)

ULuaManager::ULuaManager()
{
}

ULuaManager::~ULuaManager()
{
    
}

void ULuaManager::Initialize()
{
    MasterLuaState.open_libraries();
    MasterLuaState.script("print('--- [LuaManager] sol2 & Lua link SUCCESS! ---')");

    BindTypesToLua();
    LoadAllLuaScripts();
}

void ULuaManager::Update(float DeltaTime)
{
    HotReloadTimer += DeltaTime;
    if (HotReloadTimer >= HotReloadInterval)
    {
        HotReloadTimer = 0.0f;
        CheckForHotReload();
    }
}

void ULuaManager::BindTypesToLua()
{
// -- Vector -- //
    MasterLuaState.new_usertype<FVector>("FVector",
        sol::factories(
            []() { return FVector(); },
            [](float x, float y, float z) { return FVector(x, y, z); }
        ),
        "X", &FVector::X,
        "Y", &FVector::Y,
        "Z", &FVector::Z,

        sol::meta_function::addition, sol::overload(
            [](const FVector& a, const FVector& b) { return a + b; }
        ),
    
        sol::meta_function::subtraction, sol::overload(
            [](const FVector& a, const FVector& b) { return a - b; }
        ),

        sol::meta_function::multiplication, sol::overload(
            [](const FVector& a, const FVector& b) { return a * b; },
            [](const FVector& v, float s) { return v * s; },
            [](float s, const FVector& v) { return s * v; }
        ),
    
        sol::meta_function::division, sol::overload(
            [](const FVector& a, const FVector& b) { return a / b; },
            [](const FVector& v, float s) { return v / s; }
        ),

        // --- 함수 ---
        "Normalize", &FVector::Normalize,
        "Length", &FVector::Length
    );
    
    // -- Actor -- //
    MasterLuaState.new_usertype<AActor>("AActor",
        "Location", sol::property(
            &AActor::GetActorLocation, 
            &AActor::SetActorLocation
        ),
        "Scale", sol::property(
            &AActor::GetActorScale3D, 
            &AActor::SetActorScale3D
        )
    );

    // -- Log -- //
    MasterLuaState.set_function("Log", [](sol::variadic_args Vars) {
            std::stringstream ss;
            sol::state_view Lua(Vars.lua_state());

            for (auto v : Vars)
            {
                sol::protected_function ToString = Lua["tostring"];
                sol::protected_function_result Result = ToString(v);

                FString Str;
                if (Result.valid())
                {
                    Str = Result.get<std::string>();
                }
                else
                {
                    Str = "[nil or error]";
                }
                ss << Str << "\t";
            }

            std::string FinalLogMessage = ss.str();
            if (!FinalLogMessage.empty())
            {
                FinalLogMessage.pop_back();
            }

            UE_LOG("%s", FinalLogMessage.c_str());
        }
    );
}

void ULuaManager::LoadAllLuaScripts()
{
    LuaScriptPath = UPathManager::GetInstance().GetLuaScriptPath();
    LuaScriptCaches.clear();

    if (!exists(LuaScriptPath) || !filesystem::is_directory(LuaScriptPath))
    {
        UE_LOG("[LuaManager] Lua 스크립트 경로를 찾을 수 없거나 디렉터리가 아닙니다: %s", LuaScriptPath.string().c_str());
        return;
    }

    for (const auto& Entry : filesystem::recursive_directory_iterator(LuaScriptPath))
    {
        if (Entry.is_regular_file() && Entry.path().extension() == ".lua")
        {
            FString FileName = Entry.path().filename().stem().string();
            FString FullPath = Entry.path().string();
            filesystem::file_time_type LastModifiedTime = filesystem::last_write_time(Entry);

            if (LuaScriptCaches.find(FileName) != LuaScriptCaches.end())
            {
                FString AlreadyExistFile = LuaScriptCaches[FileName].FullPath.string();
                UE_LOG_ERROR("[LuaManager] %s 경로에 있는 Lua 스크립트는 이미 %s 경로로 등록되어있습니다! (파일명 중복)", AlreadyExistFile.c_str(), FullPath.c_str());
                continue;
            }
            LuaScriptCaches[FileName] = FLuaScriptInfo(FullPath, LastModifiedTime);

            if (FileName == "template")
            {
                LuaTemplatePath = FullPath;
            }
        }
    }
}

sol::environment ULuaManager::LoadLuaEnvironment(UScriptComponent* ScriptComponent, const FName& LuaScriptName)
{
    const auto& CacheIt = LuaScriptCaches.find(LuaScriptName);
    if (CacheIt != LuaScriptCaches.end())
    {
        sol::environment Env(MasterLuaState, sol::create, MasterLuaState.globals());
        MasterLuaState.script_file(LuaScriptCaches[LuaScriptName].FullPath.string(), Env);
        LuaScriptCaches[LuaScriptName].ScriptComponents.Emplace(ScriptComponent);
        return Env;
    }
    return sol::environment{};
}

sol::environment ULuaManager::CreateLuaEnvironment(UScriptComponent* ScriptComponent, const FName& LuaScriptName)
{
    FString ScriptFileNameStr = LuaScriptName.ToString() + ".lua";
    path DestPath = LuaScriptPath / ScriptFileNameStr;

    // --- 안전 검사 (1): 템플릿 파일 확인 ---
    if (!exists(LuaTemplatePath))
    {
        UE_LOG("[LuaManager] Lua 템플릿 파일이 존재하지 않습니다: %ls", LuaTemplatePath.c_str());
        return sol::environment{};
    }

    // --- 안전 검사 (2): 대상 파일이 이미 존재하는지 확인 ---
    if (exists(DestPath))
    {
        UE_LOG("[LuaManager] 파일이 이미 존재합니다: %s. 생성을 건너뜁니다.", ScriptFileNameStr.c_str());
        return sol::environment{};
    }

    // 복사
    try
    {
        std::filesystem::copy_file(LuaTemplatePath, DestPath);
    }
    catch (const std::filesystem::filesystem_error& Error)
    {
        UE_LOG("[LuaManager] Lua 템플릿 복사 실패: %hs", Error.what());
        return sol::environment{};
    }

    LuaScriptCaches[LuaScriptName] = FLuaScriptInfo(DestPath, filesystem::last_write_time(DestPath));
    LuaScriptCaches[LuaScriptName].ScriptComponents.Emplace(ScriptComponent);
    
    UE_LOG("[LuaManager] 새 Lua 스크립트 생성됨: %s", ScriptFileNameStr.c_str());
    sol::environment Env(MasterLuaState, sol::create, MasterLuaState.globals());
    
    MasterLuaState.script_file(DestPath.string(), Env); 
    
    return Env;
}

void ULuaManager::OpenScriptInEditor(const FName& LuaScriptName)
{
    if (LuaScriptName.IsNone())
    {
        UE_LOG("[LuaManager] 선택된 스크립트가 없습니다.");
        return;
    }
    
    const auto& CacheIt = LuaScriptCaches.find(LuaScriptName);

    if (CacheIt == LuaScriptCaches.end())
    {
        UE_LOG_ERROR("[LuaManager] %s 스크립트 경로를 찾을 수 없습니다.", LuaScriptName.ToString().c_str());
        return;
    }

    const FLuaScriptInfo& ScriptInfo = CacheIt->second;
    FString FullPathFString = ScriptInfo.FullPath.string();
    HINSTANCE hInst = ShellExecute(NULL, L"open", StringToWideString(FullPathFString).c_str(),
        NULL, LuaScriptPath.c_str(), SW_SHOWNORMAL
    );

    // 오류
    if ((INT_PTR)hInst <= 32)
    {
        MessageBox(NULL, L"파일 열기에 실패했습니다.", L"Error", MB_OK | MB_ICONERROR);
    }
}

void ULuaManager::UnregisterComponent(UScriptComponent* ScriptComponent, const FName& LuaScriptName)
{
    if (LuaScriptName.IsNone()) return;
    
    const auto& CacheIt = LuaScriptCaches.find(LuaScriptName);
    if (CacheIt != LuaScriptCaches.end())
    {
        FLuaScriptInfo& Info = CacheIt->second;

        for (int32 Idx = Info.ScriptComponents.Num() - 1; Idx >= 0; --Idx)
        {
            if (Info.ScriptComponents[Idx].Get() == ScriptComponent || Info.ScriptComponents[Idx].Get() == nullptr)
            {
                Info.ScriptComponents.RemoveAtSwap(Idx);
            }
        }
    }
}

void ULuaManager::CheckForHotReload()
{
    TArray<path> DeletedScripts;
    for (auto& Pair : LuaScriptCaches)
    {
        FName ScriptName = Pair.first;
        FLuaScriptInfo& Info = Pair.second;

        try
        {
            if (!exists(Info.FullPath))
            {
                DeletedScripts.Emplace(Info.FullPath);
                
                // TODO - ScriptComponent에게 바인딩 해제 알려주기
                continue; 
            }

            auto CurrentModifiedTime = std::filesystem::last_write_time(Info.FullPath);
            if (CurrentModifiedTime > Info.LastModifiedTime)
            {
                UE_LOG("[LuaManager] 핫 리로드: %s", ScriptName.ToString().c_str());
                Info.LastModifiedTime = CurrentModifiedTime;

                for (int32 Idx = Info.ScriptComponents.Num() - 1; Idx >= 0; --Idx)
                {
                    TWeakObjectPtr<UScriptComponent> WeakComp = Info.ScriptComponents[Idx];

                    if (UScriptComponent* Comp = WeakComp.Get()) 
                    {
                        // 새 환경 생성
                        sol::environment NewEnv(MasterLuaState, sol::create, MasterLuaState.globals());
                        MasterLuaState.script_file(Info.FullPath.string(), NewEnv);
                        Comp->HotReload(NewEnv);
                    }
                    else
                    {
                        Info.ScriptComponents.RemoveAtSwap(Idx);
                    }
                }
            }
        }
        catch (const std::filesystem::filesystem_error& Error)
        {
            UE_LOG_ERROR("[LuaManager] 핫 리로드 파일 검사 중 오류: %hs", Error.what());
        }
    }
}
