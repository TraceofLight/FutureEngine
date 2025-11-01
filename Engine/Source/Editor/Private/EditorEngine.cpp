#include "pch.h"
#include "Core/Public/Object.h"
#include "Editor/Public/EditorEngine.h"
#include "Editor/Public/Editor.h"
#include "Level/Public/Level.h"
#include "Manager/Config/Public/ConfigManager.h"
#include "Manager/Lua/Public/LuaManager.h"
#include "Manager/Path/Public/PathManager.h"
#include "Manager/UI/Public/ViewportManager.h"

IMPLEMENT_CLASS(UEditorEngine, UObject)
UEditorEngine* GEditor = nullptr;
UWorld* GWorld = nullptr;
UEditorEngine::UEditorEngine()
{
    GEditor = this;
    UWorld* EditorWorld = NewObject<UWorld>(this);
    EditorWorld->SetWorldType(EWorldType::Editor);

    if (EditorWorld)
    {
        FWorldContext EditorContext;
        EditorContext.SetWorld(EditorWorld);
        WorldContexts.Add(EditorContext); 

        GWorld = EditorWorld;
    }
    EditorModule = NewObject<UEditor>();

    CreateNewLevel();
    EditorWorld->BeginPlay();

    ULuaManager::GetInstance().Initialize();
}

UEditorEngine::~UEditorEngine()
{
    if (IsPIESessionActive())
    {
        EndPIE();
    }

    for (auto WorldContext : WorldContexts)
    {
        if (WorldContext.World())
        {
            delete WorldContext.World();
        }
    }
    if (EditorModule)
    {
        delete EditorModule;
    }
    
    GWorld = nullptr;
}

/**
 * @brief WorldContext를 순회하며 World의 Tick을 처리, EditorModule Update
 */
void UEditorEngine::Tick(float DeltaSeconds)
{
    for (FWorldContext& Context : WorldContexts)
    {
        UWorld* World = Context.World();
        if (World)
        {
            if (World->GetWorldType() == EWorldType::Editor)
            {
                World->Tick(DeltaSeconds);
            }
            else if (World->GetWorldType() == EWorldType::PIE)
            {
                // PIE 상태가 Playing일 때만 틱을 실행
                if (PIEState == EPIEState::Playing)
                {
                    World->Tick(DeltaSeconds);
                }
            }
        }
    }

    if (EditorModule)
    {
        EditorModule->Update();
    }

    ULuaManager::GetInstance().Update(DeltaSeconds);
}

/**
 * @brief PIE가 활성화되어 있는지 확인
 */
bool UEditorEngine::IsPIESessionActive() const
{    
    for (const FWorldContext& Context : WorldContexts)
    {
        if (Context.World() && Context.GetType() == EWorldType::PIE)
        {
            return true;
        }
    }
    return false;
}

/**
 * brief 에디터 월드를 복제해 PIE 시작
 */
void UEditorEngine::StartPIE()
{
    if (PIEState != EPIEState::Stopped)
    {
        return;
    }

    PIEState = EPIEState::Playing;
    UWorld* EditorWorld = GetEditorWorldContext().World();
    if (!EditorWorld)
    {
        return;
    }

    // PIE 시작 시 마지막으로 클릭한 뷰포트를 PIE 전용 뷰포트로 저장
    UViewportManager& ViewportMgr = UViewportManager::GetInstance();
    int32 LastClickedViewport = ViewportMgr.GetLastClickedViewportIndex();
    ViewportMgr.SetPIEActiveViewportIndex(LastClickedViewport);

    UWorld* PIEWorld = Cast<UWorld>(EditorWorld->Duplicate());

    if (PIEWorld)
    {
        PIEWorld->SetWorldType(EWorldType::PIE);
        FWorldContext PIEContext;
        PIEContext.SetWorld(PIEWorld);
        WorldContexts.Add(PIEContext);

        GWorld = PIEWorld;
        PIEWorld->BeginPlay();
    }
}

/**
 * @brief PIE 종료하고 에디터 월드로 돌아감
 */
void UEditorEngine::EndPIE()
{
    if (PIEState == EPIEState::Stopped)
    {
        return;
    }
    PIEState = EPIEState::Stopped;

    // PIE 전용 뷰포트 인덱스 리셋
    UViewportManager::GetInstance().SetPIEActiveViewportIndex(-1);

    FWorldContext* PIEContext = GetPIEWorldContext();
    if (PIEContext)
    {
        UWorld* PIEWorld = PIEContext->World();
        PIEWorld->EndPlay();
        delete PIEWorld;

        WorldContexts.Remove(*PIEContext);
    }

    // GWorld를 다시 Editor World로 복원
    GWorld = GetEditorWorldContext().World();
}

/**
 * @brief PIE 일시정지
 */
void UEditorEngine::PausePIE()
{
    if (PIEState != EPIEState::Playing)
    {
        return;
    }
    PIEState = EPIEState::Paused;
}

/**
 * @brief PIE 재개
 */
void UEditorEngine::ResumePIE()
{
    if (PIEState != EPIEState::Paused)
    {
        return;
    }
    PIEState = EPIEState::Playing;
}

/**
 * @brief 주어진 뷰포트 인덱스에 따라 렌더링할 World 반환
 * @param ViewportIndex 뷰포트 인덱스 (0~3)
 * @return PIE active viewport면 PIE World, 아니면 Editor World
 */
UWorld* UEditorEngine::GetWorldForViewport(int32 ViewportIndex)
{
    // PIE가 활성화되지 않았으면 항상 에디터 월드
    if (!IsPIESessionActive())
    {
        return GetEditorWorldContext().World();
    }

    // PIE 활성 뷰포트 확인
    int32 PIEActiveIndex = UViewportManager::GetInstance().GetPIEActiveViewportIndex();

    // 현재 뷰포트가 PIE 활성 뷰포트면 PIE World, 아니면 Editor World
    if (ViewportIndex == PIEActiveIndex)
    {
        FWorldContext* PIEContext = GetPIEWorldContext();
        return PIEContext ? PIEContext->World() : GetEditorWorldContext().World();
    }
    else
    {
        return GetEditorWorldContext().World();
    }
}

/**
 * @brief 경로의 파일을 불러와서 현재 Editor 월드의 Level 교체 
 */
bool UEditorEngine::LoadLevel(const FString& InFilePath)
{
    UE_LOG("GEditor: Loading Level: %s", InFilePath.data());
    
    // PIE 실행 시 PIE 종료 후 로직 실행
    if (IsPIESessionActive())
    {
        EndPIE();
    }
    return GetEditorWorldContext().World()->LoadLevel(path(InFilePath));
}

/**
 * @brief 현재 Editor 월드의 레벨을 파일로 저장
 */
bool UEditorEngine::SaveCurrentLevel(const FString& InLevelName)
{
    UE_LOG("GEditor: Saving Level: %s", InLevelName.c_str());
    
    // PIE 실행 시 PIE 종료 후 로직 실행
    if (IsPIESessionActive())
    {
        EndPIE();
    }

    path FilePath = InLevelName;
    if (FilePath.empty())
    {
        FName CurrentLevelName = GetEditorWorldContext().World()->GetLevel()->GetName();
        FilePath = GenerateLevelFilePath(CurrentLevelName == FName::GetNone()? "Untitled" : CurrentLevelName.ToString());
    }

    UE_LOG("GEditor: 현재 레벨을 다음 경로에 저장합니다: %s", FilePath.string().c_str());

    try
    {
        bool bSuccess = GetEditorWorldContext().World()->SaveCurrentLevel(FilePath);
        if (bSuccess)
        {
            UConfigManager::GetInstance().SetLastUsedLevelPath(InLevelName);

            UE_LOG("GEditor: 레벨이 성공적으로 저장되었습니다");
        }
        else
        {
            UE_LOG("GEditor: 레벨을 저장하는 데에 실패했습니다");
        }

        return bSuccess;
    }
    catch (const exception& Exception)
    {
        UE_LOG("GEditor: 저장 과정에서 Exception 발생: %s", Exception.what());
        return false;
    }
}

/**
 * @brief 현재 Editor 월드에 새 레벨 변경
 */
bool UEditorEngine::CreateNewLevel(const FString& InLevelName)
{
    UE_LOG("GEditor: Create New Level: %s", InLevelName.c_str());
    
    // PIE 실행 시 PIE 종료 후 로직 실행
    if (IsPIESessionActive()) { EndPIE(); }
    GetEditorWorldContext().World()->CreateNewLevel(InLevelName);
    return true;
}

path UEditorEngine::GenerateLevelFilePath(const FString& InLevelName)
{
    path LevelDirectory = GetLevelDirectory();
    path FileName = InLevelName + ".scene";
    return LevelDirectory / FileName;
}

path UEditorEngine::GetLevelDirectory()
{
    UPathManager& PathManager = UPathManager::GetInstance();
    return PathManager.GetWorldPath();
}

FWorldContext* UEditorEngine::GetPIEWorldContext()
{
    for (FWorldContext& Context : WorldContexts)
    {
        if (Context.World() && Context.GetType() == EWorldType::PIE)
        {
            return &Context; 
        }
    }
    return nullptr;
}

FWorldContext* UEditorEngine::GetActiveWorldContext()
{
    FWorldContext* PIEContext = GetPIEWorldContext();
    if (PIEContext)
    {
        return PIEContext;
    }
    
    if (!WorldContexts.IsEmpty())
    {
        return &WorldContexts[0];
    }

    return nullptr;
}
