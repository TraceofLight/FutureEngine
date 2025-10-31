#include "pch.h"
#include "Render/UI/Widget/Public/ActorDetailWidget.h"
#include "Level/Public/Level.h"
#include "Actor/Public/Actor.h"
#include "Component/Public/ActorComponent.h"
#include "Component/Public/SceneComponent.h"
#include "Component/Public/TextComponent.h"
#include "Global/Vector.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Manager/Input/Public/InputManager.h"
#include "Manager/Path/Public/PathManager.h"
#include "Texture/Public/Texture.h"

IMPLEMENT_CLASS(UActorDetailWidget, UWidget)

UActorDetailWidget::UActorDetailWidget()
{
	LoadComponentClasses();
	LoadActorIcons();
}

UActorDetailWidget::~UActorDetailWidget() = default;

void UActorDetailWidget::Initialize()
{
	UE_LOG("ActorDetailWidget: Initialized");
}

void UActorDetailWidget::Update()
{
	// 특별한 업데이트 로직이 필요하면 여기에 추가
}

void UActorDetailWidget::RenderWidget()
{
	ULevel* CurrentLevel = GWorld->GetLevel();

	if (!CurrentLevel)
	{
		ImGui::TextUnformatted("No Level Loaded");
		return;
	}

	AActor* SelectedActor = GEditor->GetEditorModule()->GetSelectedActor();
	if (!SelectedActor)
	{
		ImGui::TextUnformatted("No Object Selected");
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Detail 확인을 위해 Object를 선택해주세요");
		SelectedComponent = nullptr; 
		CachedSelectedActor = nullptr;
		return;
	}

	// 선택된 액터가 변경되면, 컴포넌트 선택 상태를 초기화
	if (CachedSelectedActor != SelectedActor)
	{
		CachedSelectedActor = SelectedActor;
		SelectedComponent = SelectedActor->GetRootComponent();
	}

	    // Actor 헤더 렌더링 (이름 + rename 기능)
	    RenderActorHeader(SelectedActor);
	
	    ImGui::Separator();
	
	    // CollapsingHeader 검은색 스타일 적용
	    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
	    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
	    
	    if (ImGui::CollapsingHeader("Tick Settings"))
	    {
	        // 체크박스 검은색 스타일 적용
	        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
	        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
	        ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
	        
	        bool bCanEverTick = SelectedActor->CanTick();
	        if (ImGui::Checkbox("Enable Tick", &bCanEverTick))
	        {
	            SelectedActor->SetCanTick(bCanEverTick);
	        }
	
	        bool bTickInEditor = SelectedActor->CanTickInEditor();
	        if (ImGui::Checkbox("Tick in Editor", &bTickInEditor))
	        {
	            SelectedActor->SetTickInEditor(bTickInEditor);
	        }
	        
	        ImGui::PopStyleColor(4);
	    }
	    
	    ImGui::PopStyleColor(3);
	ImGui::Separator();

	// 컴포넌트 트리 렌더링
	RenderComponents(SelectedActor);
	
	// 선택된 컴포넌트의 트랜스폼 정보 렌더링
	RenderTransformEdit();
}

void UActorDetailWidget::RenderActorHeader(AActor* InSelectedActor)
{
	if (!InSelectedActor)
	{
		return;
	}

	FName ActorName = InSelectedActor->GetName();
	FString ActorDisplayName = ActorName.ToString();

	// 아이콘 표시
	UTexture* ActorIcon = GetIconForActor(InSelectedActor);
	if (ActorIcon && ActorIcon->GetTextureSRV())
	{
		float IconSize = 16.0f;
		ImGui::Image((ImTextureID)ActorIcon->GetTextureSRV(), ImVec2(IconSize, IconSize));
		ImGui::SameLine();
	}
	else
	{
		ImGui::Text("[A]");
		ImGui::SameLine();
	}

	if (bIsRenamingActor)
	{
		// Rename 모드
		// 입력 필드 색상을 검은색으로 설정
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
		
		ImGui::SetKeyboardFocusHere();
		if (ImGui::InputText("##ActorRename", ActorNameBuffer, sizeof(ActorNameBuffer),
		                     ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
		{
			FinishRenamingActor(InSelectedActor);
		}
		
		ImGui::PopStyleColor(3);

		// ESC로 취소, InputManager보다 일단 내부 API로 입력 받는 것으로 처리
		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			CancelRenamingActor();
		}
	}
	else
	{
		// 더블클릭으로 rename 시작
		ImGui::Text("%s", ActorDisplayName.data());

		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			StartRenamingActor(InSelectedActor);
		}

		// 툴팁 UI
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Double-Click to Rename");
		}
	}
}

/**
 * @brief 컴포넌트들 렌더(SceneComponent는 트리 형태로, 아닌 것은 일반 형태로
 * @param InSelectedActor 선택된 Actor
 */
void UActorDetailWidget::RenderComponents(AActor* InSelectedActor)
{
	if (!InSelectedActor) { return; }

	const auto& Components = InSelectedActor->GetOwnedComponents();

	ImGui::Text("Components (%d)", static_cast<int>(Components.size()));
	RenderAddComponentButton(InSelectedActor);
	ImGui::Separator();

	if (Components.empty())
	{
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No components");
		return;
	}
	
	if (InSelectedActor->GetRootComponent())
	{
		RenderSceneComponents(InSelectedActor->GetRootComponent());
	}
	ImGui::Separator();
	
	bool bHasActorComponents = false;
	for (UActorComponent* Component : Components)
	{
		if (Component->IsA(USceneComponent::StaticClass())) { continue; }
		bHasActorComponents = true;
		RenderActorComponent(Component);
	}
	if (bHasActorComponents) { ImGui::Separator(); }
}

void UActorDetailWidget::RenderSceneComponents(USceneComponent* InSceneComponent)
{
	if (!InSceneComponent || InSceneComponent->IsVisualizationComponent()) { return; }
	FString ComponentName = InSceneComponent->GetName().ToString();

	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;

	bool bHasNonVisualizationChildren = false;
	if (InSceneComponent)
	{
		const TArray<USceneComponent*>& Children = InSceneComponent->GetChildren();
		for (USceneComponent* Child : Children)
		{
			if (!Child)
			{
				continue;
			}
			if (Child->IsVisualizationComponent())
			{
				continue;
			}
			bHasNonVisualizationChildren = true;
			break;
		}
	}

	if (!bHasNonVisualizationChildren)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Leaf;
	}

	if (SelectedComponent == InSceneComponent)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	bool bNodeOpen = ImGui::TreeNodeEx((void*)InSceneComponent, NodeFlags, "%s", ComponentName.c_str());

	// -----------------------------
	// Drag Source
	// -----------------------------
	if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
		{
			ImGui::SetDragDropPayload("COMPONENT_PTR", &InSceneComponent, sizeof(USceneComponent*));
			ImGui::Text("Dragging %s", ComponentName.c_str());
			ImGui::EndDragDropSource();
		}
	}

	// -----------------------------
	// Drag Target
	// -----------------------------
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("COMPONENT_PTR"))
		{
			IM_ASSERT(payload->DataSize == sizeof(USceneComponent*));
			USceneComponent* DraggedComp = *(USceneComponent**)payload->Data;

			if (!DraggedComp || DraggedComp == InSceneComponent)
			{
				ImGui::EndDragDropTarget();		ImGui::TreePop();
				return;
			}

			AActor* Owner = DraggedComp->GetOwner();
			if (!Owner)
			{
				ImGui::EndDragDropTarget();		ImGui::TreePop();
				return;
			}
			
			if (DraggedComp->GetAttachParent() == InSceneComponent)
			{
				ImGui::EndDragDropTarget();		ImGui::TreePop();
				return;
			}
			
			// -----------------------------
			// 자기 자신이나 자식에게 Drop 방지
			// -----------------------------
			if (DraggedComp && InSceneComponent)
			{
				USceneComponent* Iter = InSceneComponent;
				while (Iter)
				{
					if (Iter == DraggedComp)
					{
						UE_LOG_WARNING("Cannot drop onto self or own child.");
						ImGui::EndDragDropTarget();
						ImGui::TreePop();

						return;
					}
					Iter = Iter->GetAttachParent();
				}
			}

			// -----------------------------
			// 부모-자식 관계 설정
			// -----------------------------
			if (DraggedComp)
			{
				// 드롭 대상이 유효한 SceneComponent가 아니면, 작업을 진행하지 않습니다.
				if (!InSceneComponent)
				{
					ImGui::EndDragDropTarget();		ImGui::TreePop();
					return;
				}

				// 자기 부모에게 드롭하는 경우, 아무 작업도 하지 않음
				if (InSceneComponent == DraggedComp->GetAttachParent())
				{
					ImGui::EndDragDropTarget();		ImGui::TreePop();
					return;
				}

				// 1. 이전 부모로부터 분리
				if (USceneComponent* OldParent = DraggedComp->GetAttachParent())
				{
					DraggedComp->DetachFromComponent();
				}

				// 2. 새로운 부모에 연결하고 월드 트랜스폼 유지
				const FMatrix OldWorldMatrix = DraggedComp->GetWorldTransformMatrix();
				DraggedComp->AttachToComponent(InSceneComponent);
				const FMatrix NewParentWorldMatrixInverse = InSceneComponent->GetWorldTransformMatrixInverse();
				const FMatrix NewLocalMatrix = OldWorldMatrix * NewParentWorldMatrixInverse;

				FVector NewLocation, NewRotation, NewScale;
				DecomposeMatrix(NewLocalMatrix, NewLocation, NewRotation, NewScale);

				DraggedComp->SetRelativeLocation(NewLocation);
				DraggedComp->SetRelativeRotation(FQuaternion::FromEuler(NewRotation));
				DraggedComp->SetRelativeScale3D(NewScale);
			}
		}
		ImGui::EndDragDropTarget();
	}

	// -----------------------------
	// 클릭 선택 처리
	// -----------------------------
	if (ImGui::IsItemClicked())
	{
		SelectedComponent = InSceneComponent;
		GEditor->GetEditorModule()->SelectComponent(SelectedComponent);
	}

	// -----------------------------
	// 자식 재귀 렌더링
	// -----------------------------
	if (bNodeOpen)
	{
		if (InSceneComponent)
		{
			for (auto& Child : InSceneComponent->GetChildren())
			{
				RenderSceneComponents(Child);
			}
		}
		ImGui::TreePop();
	}
}

/**
 * @brief SceneComponent가 아닌 ActorComponent를 렌더
 * @param InActorComponent 띄울 ActorComponent
 */
void UActorDetailWidget::RenderActorComponent(UActorComponent* InActorComponent)
{
	FString ComponentName = InActorComponent->GetName().ToString();

	// --- 노드 속성 설정 (항상 잎 노드) ---
	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (SelectedComponent == InActorComponent)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	bool bNodeOpen = ImGui::TreeNodeEx((void*)InActorComponent, NodeFlags, "%s", ComponentName.c_str());

	// --- 클릭 선택 처리 ---
	if (ImGui::IsItemClicked())
	{
		SelectedComponent = InActorComponent;
		GEditor->GetEditorModule()->SelectComponent(SelectedComponent);
	}
    
	if (bNodeOpen)
	{
		ImGui::TreePop();
	}
}

void UActorDetailWidget::RenderAddComponentButton(AActor* InSelectedActor)
{
	ImGui::SameLine();

	const char* ButtonText = "Add Component";
	float ButtonWidth = ImGui::CalcTextSize(ButtonText).x + ImGui::GetStyle().FramePadding.x * 2.0f;
	ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ButtonWidth);

	if (ImGui::Button(ButtonText))
	{
		ImGui::OpenPopup("AddComponentPopup");
	}

    ImGui::SetNextWindowContentSize(ImVec2(200.0f, 0.0f)); 
	if (ImGui::BeginPopup("AddComponentPopup"))
	{
		ImGui::Text("Add Component");
		ImGui::Separator();
		
		for (auto& Pair : ComponentClasses)
		{
			const char* CName = Pair.first.c_str();
			if (CenteredSelectable(CName))
			{
				AddComponentByName(InSelectedActor, Pair.first);
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::EndPopup();
	}
}

bool UActorDetailWidget::CenteredSelectable(const char* label)
{
	// 1. 라벨이 보이지 않는 Selectable을 전체 너비로 만들어 호버링/클릭 영역을 잡습니다.
	bool clicked = ImGui::Selectable(std::string("##").append(label).c_str());

	// 2. 방금 만든 Selectable의 사각형 영역 정보를 가져옵니다.
	ImVec2 minRect = ImGui::GetItemRectMin();
	ImVec2 maxRect = ImGui::GetItemRectMax();

	// 3. 텍스트 크기를 계산합니다.
	ImVec2 textSize = ImGui::CalcTextSize(label);

	// 4. 텍스트를 그릴 중앙 위치를 계산합니다.
	ImVec2 textPos = ImVec2(
		minRect.x + (maxRect.x - minRect.x - textSize.x) * 0.5f,
		minRect.y + (maxRect.y - minRect.y - textSize.y) * 0.5f
	);

	// 5. 계산된 위치에 텍스트를 직접 그립니다.
	ImGui::GetWindowDrawList()->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), label);

	return clicked;
}

void UActorDetailWidget::AddComponentByName(AActor* InSelectedActor, const FString& InComponentName)
{
	if (!InSelectedActor)
	{
		UE_LOG_WARNING("ActorDetailWidget: 컴포넌트를 추가할 액터가 선택되지 않았습니다.");
		return;
	}

	UActorComponent* NewComponent = InSelectedActor->AddComponent(ComponentClasses[InComponentName]); 
	if (!NewComponent)
	{
		UE_LOG_ERROR("ActorDetailWidget: 알 수 없는 컴포넌트 타입 '%s'을(를) 추가할 수 없습니다.", InComponentName.data());
		return;
	}

	if (!NewComponent)
	{
		UE_LOG_ERROR("ActorDetailWidget: '%s' 컴포넌트 생성에 실패했습니다.", InComponentName.data());
		return;
	}

	// 1. 새로 만든 컴포넌트가 SceneComponent인지 확인
	if (USceneComponent* NewSceneComponent = Cast<USceneComponent>(NewComponent))
	{
		// 2. 현재 선택된 컴포넌트가 있고, 그것이 SceneComponent인지 확인
		USceneComponent* ParentSceneComponent = Cast<USceneComponent>(SelectedComponent);

		if (ParentSceneComponent)
		{
			// 3. 선택된 컴포넌트(부모)에 새로 만든 컴포넌트(자식)를 붙임
			//    (SetupAttachment는 UCLASS 내에서 호출하는 것을 가정)
			NewSceneComponent->AttachToComponent(ParentSceneComponent);
			UE_LOG_SUCCESS("'%s'를 '%s'의 자식으로 추가했습니다.", NewComponent->GetName().ToString().data(), ParentSceneComponent->GetName().ToString().data());
		}
		else
		{
			// 4. 선택된 컴포넌트가 없으면 액터의 루트 컴포넌트에 붙임
			NewSceneComponent->AttachToComponent(InSelectedActor->GetRootComponent());
			UE_LOG_SUCCESS("'%s'를 액터의 루트에 추가했습니다.", NewComponent->GetName().ToString().data());
		}

		NewSceneComponent->SetRelativeLocation(FVector::Zero());
		NewSceneComponent->SetRelativeRotation(FQuaternion::Identity());
		NewSceneComponent->SetRelativeScale3D({1, 1, 1});
	}
	else
	{
		UE_LOG_SUCCESS("Non-Scene Component '%s'를 액터에 추가했습니다.", NewComponent->GetName().ToString().data());
	}

	// 새로 추가된 컴포넌트를 자동으로 선택
	GEditor->GetEditorModule()->SelectComponent(NewComponent);
}

void UActorDetailWidget::StartRenamingActor(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}

	bIsRenamingActor = true;
	FString CurrentName = InActor->GetName().ToString();
	strncpy_s(ActorNameBuffer, CurrentName.data(), sizeof(ActorNameBuffer) - 1);
	ActorNameBuffer[sizeof(ActorNameBuffer) - 1] = '\0';

	UE_LOG("ActorDetailWidget: '%s' 에 대한 이름 변경 시작", CurrentName.data());
}

void UActorDetailWidget::FinishRenamingActor(AActor* InActor)
{
	if (!InActor || !bIsRenamingActor)
	{
		return;
	}

	FString NewName = ActorNameBuffer;
	if (!NewName.empty() && NewName != InActor->GetName().ToString())
	{
		// Actor 이름 변경
		InActor->SetName(NewName);
		UE_LOG_SUCCESS("ActorDetailWidget: Actor의 이름을 '%s' (으)로 변경하였습니다", NewName.data());
	}

	bIsRenamingActor = false;
	ActorNameBuffer[0] = '\0';
}

void UActorDetailWidget::CancelRenamingActor()
{
	bIsRenamingActor = false;
	ActorNameBuffer[0] = '\0';
	UE_LOG_WARNING("ActorDetailWidget: 이름 변경 취소");
}

void UActorDetailWidget::RenderTransformEdit()
{
	if (!SelectedComponent) { return; }
	
	// --- Component General Properties ---
	ImGui::Text("Component Properties");
	ImGui::PushID(SelectedComponent);

	// 체크박스 색상을 검은색으로 설정
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
	
	// bCanEverTick 체크박스
	bool bTickEnabled = SelectedComponent->CanEverTick();
	if (ImGui::Checkbox("Enable Tick (bCanEverTick)", &bTickEnabled))
	{
		SelectedComponent->SetCanEverTick(bTickEnabled);
	}

	// bIsEditorOnly 체크박스
	bool bIsEditorOnly = SelectedComponent->IsEditorOnly();
	if (ImGui::Checkbox("Is Editor Only", &bIsEditorOnly))
	{
		SelectedComponent->SetIsEditorOnly(bIsEditorOnly);
	}
	
	ImGui::PopStyleColor(4);

	ImGui::PopID();
	ImGui::Separator();

	// --- SceneComponent Transform Properties ---
	USceneComponent* SceneComponent = Cast<USceneComponent>(SelectedComponent);
	if (!SceneComponent)
	{
		return;
	}

	// Transform 헤더
	ImGui::Text("Transform");

	ImGui::PushID(SceneComponent);

	// Drag 필드 색상을 검은색으로 설정
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

	// Location
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	static FVector cachedLocation = FVector::ZeroVector();
	static bool bIsDraggingLocation = false;
	static USceneComponent* lastDraggedLocationComponent = nullptr;
	static bool lastShowWorldLocation = false;

	// 컴포넌트 전환 또는 World/Local 모드 전환 시 캐싱
	if (lastDraggedLocationComponent != SceneComponent || lastShowWorldLocation != bShowWorldLocation)
	{
		cachedLocation = bShowWorldLocation ? SceneComponent->GetWorldLocation() : SceneComponent->GetRelativeLocation();
		lastDraggedLocationComponent = SceneComponent;
		lastShowWorldLocation = bShowWorldLocation;
		bIsDraggingLocation = false;
	}

	// 드래그 중이 아닐 때만 동기화
	if (!bIsDraggingLocation)
	{
		cachedLocation = bShowWorldLocation ? SceneComponent->GetWorldLocation() : SceneComponent->GetRelativeLocation();
	}

	float PosArray[3] = { cachedLocation.X, cachedLocation.Y, cachedLocation.Z };
	bool PosChanged = false;

	// Location Label (드롭다운 메뉴)
	const char* LocationLabel = bShowWorldLocation ? "Absolute Location" : "Location";
	ImGui::SetNextItemWidth(120.0f); // 고정 너비로 테이블 정렬
	if (ImGui::BeginCombo("##LocationMode", LocationLabel, ImGuiComboFlags_NoArrowButton))
	{
		bool bSelectWorld = bShowWorldLocation;
		bool bSelectLocal = !bShowWorldLocation;

		if (ImGui::Selectable("Absolute Location", bSelectWorld))
		{
			bShowWorldLocation = true;
			cachedLocation = SceneComponent->GetWorldLocation();
			lastShowWorldLocation = bShowWorldLocation;
			bIsDraggingLocation = false;
		}
		if (ImGui::Selectable("Location", bSelectLocal))
		{
			bShowWorldLocation = false;
			cachedLocation = SceneComponent->GetRelativeLocation();
			lastShowWorldLocation = bShowWorldLocation;
			bIsDraggingLocation = false;
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();

	ImVec2 PosX = ImGui::GetCursorScreenPos();
	ImGui::SetNextItemWidth(75.0f);
	PosChanged |= ImGui::DragFloat("##PosX", &PosArray[0], 0.1f, 0.0f, 0.0f, "%.3f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("X: %.3f", PosArray[0]); }
	ImVec2 SizeX = ImGui::GetItemRectSize();
	DrawList->AddLine(ImVec2(PosX.x + 5, PosX.y + 2), ImVec2(PosX.x + 5, PosX.y + SizeX.y - 2), IM_COL32(255, 0, 0, 255), 2.0f);

	// 드래그 상태 추적
	bool bIsAnyLocationItemActive = ImGui::IsItemActive();
	ImGui::SameLine();

	ImVec2 PosY = ImGui::GetCursorScreenPos();
	ImGui::SetNextItemWidth(75.0f);
	PosChanged |= ImGui::DragFloat("##PosY", &PosArray[1], 0.1f, 0.0f, 0.0f, "%.3f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Y: %.3f", PosArray[1]); }
	ImVec2 SizeY = ImGui::GetItemRectSize();
	DrawList->AddLine(ImVec2(PosY.x + 5, PosY.y + 2), ImVec2(PosY.x + 5, PosY.y + SizeY.y - 2), IM_COL32(0, 255, 0, 255), 2.0f);

	bIsAnyLocationItemActive |= ImGui::IsItemActive();
	ImGui::SameLine();

	ImVec2 PosZ = ImGui::GetCursorScreenPos();
	ImGui::SetNextItemWidth(75.0f);
	PosChanged |= ImGui::DragFloat("##PosZ", &PosArray[2], 0.1f, 0.0f, 0.0f, "%.3f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Z: %.3f", PosArray[2]); }
	ImVec2 SizeZ = ImGui::GetItemRectSize();
	DrawList->AddLine(ImVec2(PosZ.x + 5, PosZ.y + 2), ImVec2(PosZ.x + 5, PosZ.y + SizeZ.y - 2), IM_COL32(0, 0, 255, 255), 2.0f);

	bIsAnyLocationItemActive |= ImGui::IsItemActive();
	ImGui::SameLine();

	// Reset button
	if (ImGui::SmallButton(u8"↻##ResetPos"))
	{
		PosArray[0] = PosArray[1] = PosArray[2] = 0.0f;
		PosChanged = true;
	}
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Reset to zero"); }

	if (PosChanged)
	{
		cachedLocation.X = PosArray[0];
		cachedLocation.Y = PosArray[1];
		cachedLocation.Z = PosArray[2];
		if (bShowWorldLocation)
		{
			SceneComponent->SetWorldLocation(cachedLocation);
		}
		else
		{
			SceneComponent->SetRelativeLocation(cachedLocation);
		}
	}

	// 드래그 상태 업데이트
	bIsDraggingLocation = bIsAnyLocationItemActive;

	// Rotation (캐싱 방식으로 Quaternion과 Euler 변환 오차 방지)
	static FVector cachedRotation = FVector::ZeroVector();
	static bool bIsDraggingRotation = false;
	static USceneComponent* lastDraggedComponent = nullptr;
	static bool lastShowWorldRotation = false;

	// 기즈모로 회전 중인지 확인
	bool bIsGizmoDragging = GEditor->GetEditorModule()->GetGizmo()->IsDragging() &&
	                        GEditor->GetEditorModule()->GetGizmo()->GetGizmoMode() == EGizmoMode::Rotate;

	// 컴포넌트 전환 또는 World / Local 모드 전환 시 캐싱
	if (lastDraggedComponent != SceneComponent || lastShowWorldRotation != bShowWorldRotation)
	{
		if (bShowWorldRotation)
		{
			cachedRotation = SceneComponent->GetWorldRotationAsQuaternion().ToEuler();
		}
		else
		{
			cachedRotation = SceneComponent->GetRelativeRotation().ToEuler();
		}
		lastDraggedComponent = SceneComponent;
		lastShowWorldRotation = bShowWorldRotation;
		bIsDraggingRotation = false;
	}

	// UI 드래그 중이 아닐 때 동기화
	if (!bIsDraggingRotation)
	{
		if (bShowWorldRotation)
		{
			FQuaternion WorldQuat = SceneComponent->GetWorldRotationAsQuaternion();
			cachedRotation = WorldQuat.ToEuler();
		}
		else
		{
			FQuaternion RelativeQuat = SceneComponent->GetRelativeRotation();
			cachedRotation = RelativeQuat.ToEuler();
		}
	}

	// 작은 값은 0으로 스냅 (부동소수점 오차 제거)
	constexpr float ZeroSnapThreshold = 0.0001f;
	if (std::abs(cachedRotation.X) < ZeroSnapThreshold)
	{
		cachedRotation.X = 0.0f;
	}
	if (std::abs(cachedRotation.Y) < ZeroSnapThreshold)
	{
		cachedRotation.Y = 0.0f;
	}
	if (std::abs(cachedRotation.Z) < ZeroSnapThreshold)
	{
		cachedRotation.Z = 0.0f;
	}

	float RotArray[3] = { cachedRotation.X, cachedRotation.Y, cachedRotation.Z };
	bool RotChanged = false;

	// Rotation Label (드롭다운 메뉴)
	const char* RotationLabel = bShowWorldRotation ? "Absolute Rotation" : "Rotation";
	ImGui::SetNextItemWidth(120.0f); // 고정 너비로 테이블 정렬
	if (ImGui::BeginCombo("##RotationMode", RotationLabel, ImGuiComboFlags_NoArrowButton))
	{
		bool bSelectWorld = bShowWorldRotation;
		bool bSelectLocal = !bShowWorldRotation;

		if (ImGui::Selectable("Absolute Rotation", bSelectWorld))
		{
			bShowWorldRotation = true;
			cachedRotation = SceneComponent->GetWorldRotationAsQuaternion().ToEuler();
			lastShowWorldRotation = bShowWorldRotation;
			lastDraggedComponent = SceneComponent;
			bIsDraggingRotation = false;
		}
		if (ImGui::Selectable("Rotation", bSelectLocal))
		{
			bShowWorldRotation = false;
			cachedRotation = SceneComponent->GetRelativeRotation().ToEuler();
			lastShowWorldRotation = bShowWorldRotation;
			lastDraggedComponent = SceneComponent;
			bIsDraggingRotation = false;
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();

	ImVec2 RotX = ImGui::GetCursorScreenPos();
	ImGui::SetNextItemWidth(75.0f);
	RotChanged |= ImGui::DragFloat("##RotX", &RotArray[0], 1.0f, 0.0f, 0.0f, "%.3f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("X: %.3f", RotArray[0]); }
	ImVec2 SizeRotX = ImGui::GetItemRectSize();
	DrawList->AddLine(ImVec2(RotX.x + 5, RotX.y + 2), ImVec2(RotX.x + 5, RotX.y + SizeRotX.y - 2), IM_COL32(255, 0, 0, 255), 2.0f);

	// 드래그 상태 추적
	bool bIsAnyItemActive = ImGui::IsItemActive();
	ImGui::SameLine();

	ImVec2 RotY = ImGui::GetCursorScreenPos();
	ImGui::SetNextItemWidth(75.0f);
	RotChanged |= ImGui::DragFloat("##RotY", &RotArray[1], 1.0f, 0.0f, 0.0f, "%.3f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Y: %.3f", RotArray[1]); }
	ImVec2 SizeRotY = ImGui::GetItemRectSize();
	DrawList->AddLine(ImVec2(RotY.x + 5, RotY.y + 2), ImVec2(RotY.x + 5, RotY.y + SizeRotY.y - 2), IM_COL32(0, 255, 0, 255), 2.0f);

	bIsAnyItemActive |= ImGui::IsItemActive();
	ImGui::SameLine();

	ImVec2 RotZ = ImGui::GetCursorScreenPos();
	ImGui::SetNextItemWidth(75.0f);
	RotChanged |= ImGui::DragFloat("##RotZ", &RotArray[2], 1.0f, 0.0f, 0.0f, "%.3f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Z: %.3f", RotArray[2]); }
	ImVec2 SizeRotZ = ImGui::GetItemRectSize();
	DrawList->AddLine(ImVec2(RotZ.x + 5, RotZ.y + 2), ImVec2(RotZ.x + 5, RotZ.y + SizeRotZ.y - 2), IM_COL32(0, 0, 255, 255), 2.0f);

	bIsAnyItemActive |= ImGui::IsItemActive();
	ImGui::SameLine();

	// Reset button
	if (ImGui::SmallButton(u8"↻##ResetRot"))
	{
		RotArray[0] = RotArray[1] = RotArray[2] = 0.0f;
		RotChanged = true;
	}
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Reset to zero"); }

	// 값 변경 시 컴포넌트에 반영
	if (RotChanged)
	{
		cachedRotation.X = RotArray[0];
		cachedRotation.Y = RotArray[1];
		cachedRotation.Z = RotArray[2];
		if (bShowWorldRotation)
		{
			SceneComponent->SetWorldRotationPreservingChildren(cachedRotation);
		}
		else
		{
			SceneComponent->SetRelativeRotationPreservingChildren(FQuaternion::FromEuler(cachedRotation));
		}
	}

	// 드래그 상태 업데이트
	bIsDraggingRotation = bIsAnyItemActive;

	// Scale (항상 Relative Scale 표시)
	FVector ComponentScale = SceneComponent->GetRelativeScale3D();
	bool bUniformScale = SceneComponent->IsUniformScale();
	if (bUniformScale)
	{
		float UniformScale = ComponentScale.X;
		bool ScaleChanged = false;

		// Scale Label (드롭다운 메뉴, 선택지는 하나만)
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::BeginCombo("##ScaleMode", "Scale", ImGuiComboFlags_NoArrowButton))
		{
			ImGui::Selectable("Scale", true);
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Scale is always relative");
		}
		ImGui::SameLine();

		ImVec2 PosScale = ImGui::GetCursorScreenPos();
		// Uniform Scale 너비 = (75 * 3) + (간격 * 2)
		const float ItemWidth = 75.0f;
		const float ItemSpacing = ImGui::GetStyle().ItemSpacing.x;
		const float UniformScaleWidth = (ItemWidth * 3.0f) + (ItemSpacing * 2.0f);
		ImGui::SetNextItemWidth(UniformScaleWidth);

		ScaleChanged = ImGui::DragFloat("##UniformScale", &UniformScale, 0.1f, 0.0f, 0.0f, "%.3f");
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Scale: %.3f", UniformScale); }
		ImVec2 SizeScale = ImGui::GetItemRectSize();
		DrawList->AddLine(ImVec2(PosScale.x + 5, PosScale.y + 2), ImVec2(PosScale.x + 5, PosScale.y + SizeScale.y - 2), IM_COL32(255, 255, 255, 255), 2.0f);
		ImGui::SameLine();

		// Reset button
		if (ImGui::SmallButton(u8"↻##ResetScale"))
		{
			UniformScale = 1.0f;
			ScaleChanged = true;
		}
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Reset to 1.0"); }

		if (ScaleChanged)
		{
			SceneComponent->SetRelativeScale3D({UniformScale, UniformScale, UniformScale});
		}
	}
	else
	{
		float ScaleArray[3] = { ComponentScale.X, ComponentScale.Y, ComponentScale.Z };
		bool ScaleChanged = false;

		// Scale Label
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::BeginCombo("##ScaleMode", "Scale", ImGuiComboFlags_NoArrowButton))
		{
			ImGui::Selectable("Scale", true);
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Scale is always relative");
		}
		ImGui::SameLine();

		ImVec2 ScaleX = ImGui::GetCursorScreenPos();
		ImGui::SetNextItemWidth(75.0f);
		ScaleChanged |= ImGui::DragFloat("##ScaleX", &ScaleArray[0], 0.1f, 0.0f, 0.0f, "%.3f");
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("X: %.3f", ScaleArray[0]); }
		ImVec2 SizeScaleX = ImGui::GetItemRectSize();
		DrawList->AddLine(ImVec2(ScaleX.x + 5, ScaleX.y + 2), ImVec2(ScaleX.x + 5, ScaleX.y + SizeScaleX.y - 2), IM_COL32(255, 0, 0, 255), 2.0f);
		ImGui::SameLine();

		ImVec2 ScaleY = ImGui::GetCursorScreenPos();
		ImGui::SetNextItemWidth(75.0f);
		ScaleChanged |= ImGui::DragFloat("##ScaleY", &ScaleArray[1], 0.1f, 0.0f, 0.0f, "%.3f");
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Y: %.3f", ScaleArray[1]); }
		ImVec2 SizeScaleY = ImGui::GetItemRectSize();
		DrawList->AddLine(ImVec2(ScaleY.x + 5, ScaleY.y + 2), ImVec2(ScaleY.x + 5, ScaleY.y + SizeScaleY.y - 2), IM_COL32(0, 255, 0, 255), 2.0f);
		ImGui::SameLine();

		ImVec2 ScaleZ = ImGui::GetCursorScreenPos();
		ImGui::SetNextItemWidth(75.0f);
		ScaleChanged |= ImGui::DragFloat("##ScaleZ", &ScaleArray[2], 0.1f, 0.0f, 0.0f, "%.3f");
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Z: %.3f", ScaleArray[2]); }
		ImVec2 SizeScaleZ = ImGui::GetItemRectSize();
		DrawList->AddLine(ImVec2(ScaleZ.x + 5, ScaleZ.y + 2), ImVec2(ScaleZ.x + 5, ScaleZ.y + SizeScaleZ.y - 2), IM_COL32(0, 0, 255, 255), 2.0f);
		ImGui::SameLine();

		// Reset button
		if (ImGui::SmallButton(u8"↻##ResetScale"))
		{
			ScaleArray[0] = ScaleArray[1] = ScaleArray[2] = 1.0f;
			ScaleChanged = true;
		}
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Reset to 1.0"); }

		if (ScaleChanged)
		{
			ComponentScale.X = ScaleArray[0];
			ComponentScale.Y = ScaleArray[1];
			ComponentScale.Z = ScaleArray[2];
			SceneComponent->SetRelativeScale3D(ComponentScale);
		}
	}
	
	ImGui::PopStyleColor(3);
	
	// Uniform Scale 체크박스 색상 설정
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
	
	ImGui::Checkbox("Uniform Scale", &bUniformScale);
	SceneComponent->SetUniformScale(bUniformScale);
	
	ImGui::PopStyleColor(4);
	
	ImGui::PopID();
}

void UActorDetailWidget::SwapComponents(UActorComponent* A, UActorComponent* B)
{
	if (!A || !B) return;

	AActor* Owner = A->GetOwner();
	if (!Owner) return;

	auto& Components = Owner->GetOwnedComponents();

	auto ItA = std::find(Components.begin(), Components.end(), A);
	auto ItB = std::find(Components.begin(), Components.end(), B);

	if (ItA != Components.end() && ItB != Components.end())
	{
		// 포인터 임시 저장
		UActorComponent* TempA = *ItA;
		UActorComponent* TempB = *ItB;

		// erase 후 push_back
		Components.erase(ItA); // 먼저 A 제거
		// B 제거 (A 제거 후 iterator invalid 되므로 다시 찾기)
		ItB = std::find(Components.begin(), Components.end(), B);
		Components.erase(ItB);

		// 서로 위치를 바꿔 push_back
		Components.push_back(TempA);
		Components.push_back(TempB);

		// SceneComponent라면 부모/자식 관계 교체
		if (USceneComponent* SceneA = Cast<USceneComponent>(A))
			if (USceneComponent* SceneB = Cast<USceneComponent>(B))
			{
				USceneComponent* ParentA = SceneA->GetAttachParent();
				USceneComponent* ParentB = SceneB->GetAttachParent();

				SceneA->AttachToComponent(ParentB);
				SceneB->AttachToComponent(ParentA);
			}
	}
}

void UActorDetailWidget::DecomposeMatrix(const FMatrix& InMatrix, FVector& OutLocation, FVector& OutRotation, FVector& OutScale)
{
    // 스케일 추출
    OutScale.X = FVector(InMatrix.Data[0][0], InMatrix.Data[0][1], InMatrix.Data[0][2]).Length();
    OutScale.Y = FVector(InMatrix.Data[1][0], InMatrix.Data[1][1], InMatrix.Data[1][2]).Length();
    OutScale.Z = FVector(InMatrix.Data[2][0], InMatrix.Data[2][1], InMatrix.Data[2][2]).Length();

    // 위치 추출
    OutLocation.X = InMatrix.Data[3][0];
    OutLocation.Y = InMatrix.Data[3][1];
    OutLocation.Z = InMatrix.Data[3][2];

    // 회전 행렬 추출 (스케일 제거)
    FMatrix RotationMatrix;
	for (int i = 0; i < 3; ++i)
	{
		RotationMatrix.Data[i][0] = InMatrix.Data[i][0] / OutScale.X;
		RotationMatrix.Data[i][1] = InMatrix.Data[i][1] / OutScale.Y;
		RotationMatrix.Data[i][2] = InMatrix.Data[i][2] / OutScale.Z;
	}

    // 오일러 각으로 변환 (Pitch, Yaw, Roll)
    OutRotation.X = atan2(RotationMatrix.Data[2][1], RotationMatrix.Data[2][2]);
    OutRotation.Y = atan2(-RotationMatrix.Data[2][0], sqrt(RotationMatrix.Data[2][1] * RotationMatrix.Data[2][1] + RotationMatrix.Data[2][2] * RotationMatrix.Data[2][2]));
    OutRotation.Z = atan2(RotationMatrix.Data[1][0], RotationMatrix.Data[0][0]);

	OutRotation = FVector::GetRadianToDegree(OutRotation);
}

void UActorDetailWidget::SetSelectedComponent(UActorComponent* InComponent)
{
	SelectedComponent = InComponent;

	// CachedSelectedActor도 업데이트하여 RenderWidget()에서 덮어쓰지 않도록 함
	if (InComponent)
	{
		CachedSelectedActor = InComponent->GetOwner();
	}
}

void UActorDetailWidget::LoadComponentClasses()
{
	for (UClass* Class : UClass::FindClasses(UActorComponent::StaticClass()))
	{
		if (!Class->IsAbstract())
		{
			ComponentClasses[Class->GetName().ToString().substr(1)] = Class;
		}
	}
}

/**
 * @brief Actor 클래스별 아이콘 텍스처를 로드하는 함수
 */
void UActorDetailWidget::LoadActorIcons()
{
	UE_LOG("ActorDetailWidget: 아이콘 로드 시작...");
	UAssetManager& AssetManager = UAssetManager::GetInstance();
	UPathManager& PathManager = UPathManager::GetInstance();
	FString IconBasePath = PathManager.GetAssetPath().string() + "\\Icon\\";

	// 로드할 아이콘 목록 (클래스 이름 -> 파일명)
	TArray<FString> IconFiles = {
		"Actor.png",
		"StaticMeshActor.png",
		"DirectionalLight.png",
		"PointLight.png",
		"SpotLight.png",
		"SkyLight.png",
		"DecalActor.png",
		"ExponentialHeightFog.png"
	};

	int32 LoadedCount = 0;
	for (const FString& FileName : IconFiles)
	{
		FString FullPath = IconBasePath + FileName;
		UTexture* IconTexture = AssetManager.LoadTexture(FullPath);
		if (IconTexture)
		{
			// 파일명에서 .png 제거하여 클래스 이름으로 사용
			FString ClassName = FileName.substr(0, FileName.find_last_of('.'));
			IconTextureMap[ClassName] = IconTexture;
			LoadedCount++;
			UE_LOG("ActorDetailWidget: 아이콘 로드 성공: '%s' -> %p", ClassName.c_str(), IconTexture);
		}
		else
		{
			UE_LOG_WARNING("ActorDetailWidget: 아이콘 로드 실패: %s", FullPath.c_str());
		}
	}
	UE_LOG_SUCCESS("ActorDetailWidget: 아이콘 로드 완료 (%d/%d)", LoadedCount, (int32)IconFiles.size());
}

/**
 * @brief Actor에 맞는 아이콘 텍스처를 반환하는 함수
 * @param InActor 아이콘을 가져올 Actor
 * @return 아이콘 텍스처 (없으면 기본 Actor 아이콘)
 */
UTexture* UActorDetailWidget::GetIconForActor(AActor* InActor)
{
	if (!InActor)
	{
		return nullptr;
	}

	// 클래스 이름 가져오기
	FString OriginalClassName = InActor->GetClass()->GetName().ToString();
	FString ClassName = OriginalClassName;
	
	// 'A' 접두사 제거 (예: AStaticMeshActor -> StaticMeshActor)
	if (ClassName.size() > 1 && ClassName[0] == 'A')
	{
		ClassName = ClassName.substr(1);
	}

	// 특정 클래스에 대한 매핑
	auto It = IconTextureMap.find(ClassName);
	if (It != IconTextureMap.end())
	{
		return It->second;
	}

	// Light 계열 처리
	if (ClassName.find("Light") != std::string::npos)
	{
		if (ClassName.find("Directional") != std::string::npos)
		{
			auto DirIt = IconTextureMap.find("DirectionalLight");
			if (DirIt != IconTextureMap.end()) return DirIt->second;
		}
		else if (ClassName.find("Point") != std::string::npos)
		{
			auto PointIt = IconTextureMap.find("PointLight");
			if (PointIt != IconTextureMap.end()) return PointIt->second;
		}
		else if (ClassName.find("Spot") != std::string::npos)
		{
			auto SpotIt = IconTextureMap.find("SpotLight");
			if (SpotIt != IconTextureMap.end()) return SpotIt->second;
		}
		else if (ClassName.find("Sky") != std::string::npos || ClassName.find("Ambient") != std::string::npos)
		{
			auto SkyIt = IconTextureMap.find("SkyLight");
			if (SkyIt != IconTextureMap.end()) return SkyIt->second;
		}
	}

	// Fog 처리
	if (ClassName.find("Fog") != std::string::npos)
	{
		auto FogIt = IconTextureMap.find("ExponentialHeightFog");
		if (FogIt != IconTextureMap.end()) return FogIt->second;
	}

	// Decal 처리
	if (ClassName.find("Decal") != std::string::npos)
	{
		auto DecalIt = IconTextureMap.find("DecalActor");
		if (DecalIt != IconTextureMap.end()) return DecalIt->second;
	}

	// 기본 Actor 아이콘 반환
	auto ActorIt = IconTextureMap.find("Actor");
	if (ActorIt != IconTextureMap.end())
	{
		return ActorIt->second;
	}

	// 아이콘을 찾지 못했을 경우 1회만 로그 출력
	static std::unordered_set<FString> LoggedClasses;
	if (LoggedClasses.find(OriginalClassName) == LoggedClasses.end())
	{
		UE_LOG("ActorDetailWidget: '%s' (변환: '%s')에 대한 아이콘을 찾을 수 없습니다", OriginalClassName.c_str(), ClassName.c_str());
		LoggedClasses.insert(OriginalClassName);
	}

	return nullptr;
}
