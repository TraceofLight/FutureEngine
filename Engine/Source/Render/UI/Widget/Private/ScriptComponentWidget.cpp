#include "pch.h"
#include "Render/UI/Widget/Public/ScriptComponentWidget.h"
#include "Component/Public/ScriptComponent.h"
#include "Manager/Lua/Public/LuaManager.h"

IMPLEMENT_CLASS(UScriptComponentWidget, UWidget)

void UScriptComponentWidget::Update()
{
}

void UScriptComponentWidget::RenderWidget()
{
    static char NewScriptInputBuffer[128] = "";
    
    ULevel* CurrentLevel = GWorld->GetLevel();
    UScriptComponent* ScriptComponent = nullptr;
    if (CurrentLevel)
    {
        UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
        ScriptComponent = Cast<UScriptComponent>(NewSelectedComponent);

        if (!ScriptComponent) 
        { 
            ImGui::TextWrapped("No UScriptComponent is selected in the Outliner.");
            return; 
        }
    }

    ULuaManager& LuaManager = ULuaManager::GetInstance();
    ImGui::PushID(ScriptComponent);

    // -- 스크립트 할당 드롭다운 -- //
    ImGui::Text("Assigned Script");

    FName CurrentScriptName = ScriptComponent->GetScriptFileName();
    FString PreviewLabelFStr = CurrentScriptName.IsNone() ? "<None>" : CurrentScriptName.ToString();
    const char* PreviewLabel = PreviewLabelFStr.c_str();

    // -- 콤보 박스 시작 -- //
    if (ImGui::BeginCombo("##ScriptCombo", PreviewLabel))
    {
        // -- None Select -- //
        bool bIsNoneSelected = CurrentScriptName.IsNone();
        if (ImGui::Selectable("<None>", bIsNoneSelected)) { ScriptComponent->ClearScript(); }
        if (bIsNoneSelected) { ImGui::SetItemDefaultFocus(); }

        // -- Script Name DropList -- //
        const auto& ScriptCache = LuaManager.GetLuaScriptInfo(); 
        for (const auto& Pair : ScriptCache)
        {
            const FName& ScriptName = Pair.first;
            FString ScriptNameFStr = ScriptName.ToString();
            const char* ItemLabel = ScriptNameFStr.c_str();

            bool bIsSelected = (CurrentScriptName == ScriptName);
            if (ImGui::Selectable(ItemLabel, bIsSelected))
            {
                if (ScriptComponent->GetScriptFileName() != ScriptName)
                {
                    ScriptComponent->AssignScript(ScriptName);
                }
            }
            if (bIsSelected) { ImGui::SetItemDefaultFocus(); }
        }
        
        ImGui::EndCombo();
    }

    ImGui::Separator();

    bool bHasScriptAssigned = !CurrentScriptName.IsNone();
    if (!bHasScriptAssigned)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        ImGui::BeginDisabled();
    }

    // -- [Edit] 버튼 -- //
    if (ImGui::Button("Edit Script", ImVec2(-1, 0)))
    {
        ScriptComponent->OpenCurrentScriptInEditor();
    }

    if (!bHasScriptAssigned)
    {
        ImGui::EndDisabled();
        ImGui::PopStyleVar();
    }
    
    ImGui::Separator();

    // -- 새 스크립트 생성 -- //
    ImGui::Text("Create New Script");
    
    // ".lua" 힌트가 있는 텍스트 입력창
    ImGui::InputTextWithHint("##NewScriptName", "NewScriptName", NewScriptInputBuffer, IM_ARRAYSIZE(NewScriptInputBuffer));
    ImGui::SameLine();

    // -- [Create] 버튼 -- //
    if (ImGui::Button("Create & Assign"))
    {
        if (strlen(NewScriptInputBuffer) > 0)
        {
            FString NewNameStr = NewScriptInputBuffer;
            
            if (!NewNameStr.empty())
            {
                ScriptComponent->CreateAndAssignScript(NewNameStr);
                NewScriptInputBuffer[0] = '\0'; // 입력 버퍼 비우기
            }
        }
    }

    ImGui::PopID();
}
