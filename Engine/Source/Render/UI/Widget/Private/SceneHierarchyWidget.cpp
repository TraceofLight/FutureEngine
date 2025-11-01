#include "pch.h"
#include "Render/UI/Widget/Public/SceneHierarchyWidget.h"


#include "Level/Public/Level.h"
#include "Actor/Public/Actor.h"
#include "Editor/Public/Camera.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/UI/Viewport/Public/ViewportClient.h"
#include "Render/UI/Viewport/Public/Viewport.h"
#include "Manager/UI/Public/ViewportManager.h"
#include "Global/Quaternion.h"
#include "Component/Public/LightComponentBase.h"
#include "Component/Public/HeightFogComponent.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Texture/Public/Texture.h"
#include "Manager/Path/Public/PathManager.h"

IMPLEMENT_CLASS(USceneHierarchyWidget, UWidget)
USceneHierarchyWidget::USceneHierarchyWidget()
{
}

USceneHierarchyWidget::~USceneHierarchyWidget() = default;

void USceneHierarchyWidget::Initialize()
{
	LoadActorIcons();
	UE_LOG("SceneHierarchyWidget: Initialized");
}

void USceneHierarchyWidget::Update()
{
}

void USceneHierarchyWidget::RenderWidget()
{
	// 에디터 UI는 현재 World를 참조
	UWorld* World = GWorld;
	if (!World)
	{
		return;
	}

	ULevel* CurrentLevel = World->GetLevel();

	if (!CurrentLevel)
	{
		ImGui::TextUnformatted("No Level Loaded");
		return;
	}

	// 헤더 정보
	ImGui::Text("Level: %s", CurrentLevel->GetName().ToString().c_str());
	ImGui::Separator();

	// 검색창 렌더링
	RenderSearchBar();

	const TArray<AActor*>& LevelActors = CurrentLevel->GetLevelActors();

	if (LevelActors.IsEmpty())
	{
		ImGui::TextUnformatted("No Actors in Level");
		return;
	}

	// 필터링 업데이트
	if (bNeedsFilterUpdate)
	{
		UE_LOG("SceneHierarchy: 필터 업데이트 실행 중...");
		UpdateFilteredActors(LevelActors);
		bNeedsFilterUpdate = false;
	}

	// Actor 개수 표시
	if (SearchFilter.empty())
	{
		ImGui::Text("Total Actors: %zu", LevelActors.Num());
	}
	else
	{
		ImGui::Text("%d / %zu actors", static_cast<int32>(FilteredIndices.Num()), LevelActors.Num());
	}
	ImGui::Spacing();

	// Actor 리스트를 스크롤 가능한 영역으로 표시
	if (ImGui::BeginChild("ActorList", ImVec2(0, 0), true))
	{
		// IMGui Clipper를 사용하여 눈에 보이는 부분만 렌더하여 비용을 절약한다.
		if (SearchFilter.empty())
		{
			// 검색어가 없으면 모든 Actor 표시
			ImGuiListClipper clipper;
			clipper.Begin(LevelActors.Num());
			while (clipper.Step())
			{
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
				{
					if (LevelActors[i])
						RenderActorInfo(LevelActors[i], i);
				}
			}
			clipper.End();

		}
		else
		{
			// 필터링된 Actor들만 표시
			ImGuiListClipper clipper;
			clipper.Begin(FilteredIndices.Num());
			while (clipper.Step())
			{
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
				{
					int32 FilterIndex = FilteredIndices[i];

					if (FilterIndex < LevelActors.Num() && LevelActors[FilterIndex])
					{
						RenderActorInfo(LevelActors[FilterIndex], FilterIndex);
					}
				}
			}
			clipper.End();

			// 검색 결과가 없으면 메시지 표시
			if (FilteredIndices.IsEmpty())
			{
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "검색 결과가 없습니다.");
			}
		}
	}
	ImGui::EndChild();
}

/**
 * @brief Actor 정보를 렌더링하는 헬퍼 함수
 * @param InActor 렌더링할 Actor
 * @param InIndex Actor의 인덱스
 */
void USceneHierarchyWidget::RenderActorInfo(AActor* InActor, int32 InIndex)
{
	// TODO(KHJ): 컴포넌트 정보 탐색을 위한 트리 노드를 작업 후 남겨두었음, 필요하다면 사용할 것

	if (!InActor)
	{
		return;
	}

	ImGui::PushID(InIndex);

	// 현재 선택된 Actor인지 확인
	bool bIsSelected = (GEditor->GetEditorModule()->GetSelectedActor() == InActor);

	// 선택된 Actor는 하이라이트
	if (bIsSelected)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f)); // 노란색
	}

	FName ActorName = InActor->GetName();
	FString ActorDisplayName = ActorName.ToString();

	// Actor의 PrimitiveComponent들의 Visibility 체크
	bool bHasPrimitive = false;
	bool bHasLight = false;
	bool bHasFog = false;
	bool bAllVisible = true;
	UPrimitiveComponent* FirstPrimitive = nullptr;

	// Actor의 모든 Component 중에서 PrimitiveComponent 찾기
	for (auto& Component : InActor->GetOwnedComponents())
	{
		if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component))
		{
			bHasPrimitive = true;

			if (!FirstPrimitive)
			{
				FirstPrimitive = PrimitiveComponent;
			}

			if (!PrimitiveComponent->IsVisible())
			{
				bAllVisible = false;
			}
		}
		if (ULightComponentBase* LightComponent = Cast<ULightComponentBase>(Component))
		{
			bHasLight = true;
			if (!LightComponent->GetVisible())
			{
				bAllVisible = false;
			}
		}
		if (UHeightFogComponent* FogComponent = Cast<UHeightFogComponent>(Component))
		{
			bHasFog = true;
			if (!FogComponent->GetVisible())
			{
				bAllVisible = false;
			}
		}
	}

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
	// PrimitiveComponent나 light, fog가 있는 경우에만 Visibility 버튼 표시
	if (bHasPrimitive || bHasLight || bHasFog)
	{
		if (ImGui::SmallButton(bAllVisible ? "[O]" : "[X]"))
		{
			// 모든 PrimitiveComponent의 Visibility 토글
			bool bNewVisibility = !bAllVisible;
			for (auto& Component : InActor->GetOwnedComponents())
			{
				if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component))
				{
					PrimComp->SetVisibility(bNewVisibility);
				}
				if (ULightComponentBase* LightComp = Cast<ULightComponentBase>(Component))
				{
					LightComp->SetVisible(bNewVisibility);
				}
				if (UHeightFogComponent* FogComp = Cast<UHeightFogComponent>(Component))
				{
					FogComp->SetVisible(bNewVisibility);
				}
			}
			UE_LOG_INFO("SceneHierarchy: %s의 가시성이 %s로 변경되었습니다",
			            ActorName.ToString().data(),
			            bNewVisibility ? "Visible" : "Hidden");
		}
	}
	else
	{
		// PrimitiveComponent가 없는 경우 비활성화된 버튼 표시
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
		ImGui::SmallButton("[-]");
		ImGui::PopStyleVar();
	}
	ImGui::PopStyleColor(3);
	
	// 아이콘 표시
	ImGui::SameLine();
	UTexture* IconTexture = GetIconForActor(InActor);
	if (IconTexture && IconTexture->GetTextureSRV())
	{
		ImVec2 IconSize(20.0f, 20.0f); // 아이콘 크기
		ImGui::Image((ImTextureID)IconTexture->GetTextureSRV(), IconSize);
		ImGui::SameLine();
	}

	// 이름 변경 모드인지 확인
	if (RenamingActor == InActor)
	{
		// 입력 필드 색상을 검은색으로 설정
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
		
		// 이름 변경 입력창
		ImGui::PushItemWidth(-1.0f);
		bool bEnterPressed = ImGui::InputText("##Rename", RenameBuffer, sizeof(RenameBuffer),
		                                      ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
		ImGui::PopItemWidth();
		
		ImGui::PopStyleColor(3);

		// Enter 키로 확인
		if (bEnterPressed)
		{
			FinishRenaming(true);
		}
		// ESC 키로 취소
		else if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			FinishRenaming(false);
		}
		// 다른 곳 클릭으로 취소
		else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered())
		{
			FinishRenaming(false);
		}

		// 포커스 설정 (첫 렌더링에서만)
		if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive())
		{
			ImGui::SetKeyboardFocusHere(-1);
		}
	}
	else
	{
		// 일반 선택 모드
		bool bClicked = ImGui::Selectable(ActorDisplayName.data(), bIsSelected, ImGuiSelectableFlags_SpanAllColumns);

		if (bClicked)
		{
			double CurrentTime = ImGui::GetTime();

			// 이미 선택된 Actor를 2초 이내 다시 클릭한 경우
			if (bIsSelected && LastClickedActor == InActor &&
				(CurrentTime - LastClickTime) > RENAME_CLICK_DELAY &&
				(CurrentTime - LastClickTime) < 2.0f)
			{
				// 이름 변경 모드 시작
				StartRenaming(InActor);
			}
			else
			{
				// 일반 선택
				SelectActor(InActor, false);
			}

			LastClickTime = CurrentTime;
			LastClickedActor = InActor;
		}

		// 더블 클릭 감지: 카메라 이동 수행
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			SelectActor(InActor, true);
			// 더블클릭 시 이름변경 모드 비활성화
			FinishRenaming(false);
		}
	}

	if (bIsSelected)
	{
		ImGui::PopStyleColor();
	}

	ImGui::PopID();
}

/**
 * @brief Actor를 선택하는 함수
 * @param InActor 선택할 Actor
 * @param bInFocusCamera 카메라 포커싱 여부 (더블 클릭 시 true)
 */
void USceneHierarchyWidget::SelectActor(AActor* InActor, bool bInFocusCamera)
{
	UEditor* Editor = GEditor->GetEditorModule();
	Editor->SelectActor(InActor);
	UE_LOG("SceneHierarchy: %s를 선택했습니다", InActor->GetName().ToString().data());

	// 카메라 포커싱은 더블 클릭에서만 수행
	if (InActor && bInFocusCamera)
	{
		GEditor->GetEditorModule()->FocusOnSelectedActor();
		UE_LOG_SUCCESS("SceneHierarchy: %s에 카메라 포커싱 완료", InActor->GetName().ToString().data());
	}
}

/**
 * @brief 검색창을 렌더링하는 함수
 */
void USceneHierarchyWidget::RenderSearchBar()
{
	// 검색 지우기 버튼
	if (ImGui::SmallButton("X"))
	{
		memset(SearchBuffer, 0, sizeof(SearchBuffer));
		SearchFilter.clear();
		bNeedsFilterUpdate = true;
	}

	// 검색창
	ImGui::SameLine();
	
	// 입력 필드 색상을 검은색으로 설정
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
	
	ImGui::PushItemWidth(-1.0f); // 나머지 너비 모두 사용
	bool bTextChanged = ImGui::InputTextWithHint("##Search", "검색...", SearchBuffer, sizeof(SearchBuffer));
	ImGui::PopItemWidth();
	
	ImGui::PopStyleColor(3);

	// 검색어가 변경되면 필터 업데이트 플래그 설정
	if (bTextChanged)
	{
		FString NewSearchFilter = FString(SearchBuffer);
		if (NewSearchFilter != SearchFilter)
		{
			UE_LOG("SceneHierarchy: 검색어 변경: '%s' -> '%s'", SearchFilter.data(), NewSearchFilter.data());
			SearchFilter = NewSearchFilter;
			bNeedsFilterUpdate = true;
		}
	}
}

/**
 * @brief 필터링된 Actor 인덱스 리스트를 업데이트하는 함수
 * @param InLevelActors 레벨의 모든 Actor 리스트
 */
void USceneHierarchyWidget::UpdateFilteredActors(const TArray<AActor*>& InLevelActors)
{
	FilteredIndices.Empty();

	if (SearchFilter.empty())
	{
		return; // 검색어가 없으면 모든 Actor 표시
	}

	// 검색 성능 최적화: 대소문자 변환을 한 번만 수행
	FString SearchLower = SearchFilter;
	std::transform(SearchLower.begin(), SearchLower.end(), SearchLower.begin(), ::tolower);

	// UE_LOG("SceneHierarchy: 검색어 = '%s', 변환된 검색어 = '%s'", SearchFilter.data(), SearchLower.data());
	// UE_LOG("SceneHierarchy: Level에 %zu개의 Actor가 있습니다", InLevelActors.size());

	for (int32 i = 0; i < InLevelActors.Num(); ++i)
	{
		if (InLevelActors[i])
		{
			FString ActorName = InLevelActors[i]->GetName().ToString();
			bool bMatches = IsActorMatchingSearch(ActorName, SearchLower);
			// UE_LOG("SceneHierarchy: Actor[%d] = '%s', 매치 = %s", i, ActorName.c_str(), bMatches ? "Yes" : "No");

			if (bMatches)
			{
				FilteredIndices.Add(i);
			}
		}
	}

	UE_LOG("SceneHierarchy: 필터링 결과: %d개 찾음", FilteredIndices.Num());
}

/**
 * @brief Actor 이름이 검색어와 일치하는지 확인
 * @param InActorName Actor 이름
 * @param InSearchTerm 검색어 (대소문자를 무시)
 * @return 일치하면 true
 */
bool USceneHierarchyWidget::IsActorMatchingSearch(const FString& InActorName, const FString& InSearchTerm)
{
	if (InSearchTerm.empty())
	{
		return true;
	}

	FString ActorNameLower = InActorName;
	std::transform(ActorNameLower.begin(), ActorNameLower.end(), ActorNameLower.begin(), ::tolower);

	bool bResult = ActorNameLower.find(InSearchTerm) != std::string::npos;

	return bResult;
}

/**
 * @brief 이름 변경 모드를 시작하는 함수
 * @param InActor 이름을 변경할 Actor
 */
void USceneHierarchyWidget::StartRenaming(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}

	RenamingActor = InActor;
	FString CurrentName = InActor->GetName().ToString();

	// 현재 이름을 버퍼에 복사 - Detail 패널과 동일한 방식 사용
	strncpy_s(RenameBuffer, CurrentName.data(), sizeof(RenameBuffer) - 1);
	RenameBuffer[sizeof(RenameBuffer) - 1] = '\0';

	UE_LOG("SceneHierarchy: '%s' 에 대한 이름 변경 시작", CurrentName.data());
}

/**
 * @brief 이름 변경을 완료하는 함수
 * @param bInConfirm true면 적용, false면 취소
 */
void USceneHierarchyWidget::FinishRenaming(bool bInConfirm)
{
	if (!RenamingActor)
	{
		return;
	}

	if (bInConfirm)
	{
		FString NewName = FString(RenameBuffer);
		// 빈 이름 방지 및 이름 변경 여부 확인
		if (!NewName.empty() && NewName != RenamingActor->GetName().ToString())
		{
			// Detail 패널과 동일한 방식 사용
			RenamingActor->SetName(NewName);
			UE_LOG_SUCCESS("SceneHierarchy: Actor의 이름을 '%s' (으)로 변경하였습니다", NewName.c_str());

			// 검색 필터를 업데이트해야 할 수도 있음
			bNeedsFilterUpdate = true;
		}
		else if (NewName.empty())
		{
			UE_LOG_WARNING("SceneHierarchy: 빈 이름으로 인해 이름 변경 취소됨");
		}
	}
	else
	{
		UE_LOG_WARNING("SceneHierarchy: 이름 변경 취소");
	}

	// 상태 초기화
	RenamingActor = nullptr;
	RenameBuffer[0] = '\0';
}

/**
 * @brief Actor 클래스별 아이콘 텍스처를 로드하는 함수
 */
void USceneHierarchyWidget::LoadActorIcons()
{
	UE_LOG("SceneHierarchy: 아이콘 로드 시작...");
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
			UE_LOG("SceneHierarchy: 아이콘 로드 성공: '%s' -> %p", ClassName.c_str(), IconTexture);
		}
		else
		{
			UE_LOG_WARNING("SceneHierarchy: 아이콘 로드 실패: %s", FullPath.c_str());
		}
	}
	UE_LOG_SUCCESS("SceneHierarchy: 아이콘 로드 완료 (%d/%d)", LoadedCount, (int32)IconFiles.Num());
}

/**
 * @brief Actor에 맞는 아이콘 텍스처를 반환하는 함수
 * @param InActor 아이콘을 가져올 Actor
 * @return 아이콘 텍스처 (없으면 기본 Actor 아이콘)
 */
UTexture* USceneHierarchyWidget::GetIconForActor(AActor* InActor)
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
		UE_LOG("SceneHierarchy: '%s' (변환: '%s')에 대한 아이콘을 찾을 수 없습니다", OriginalClassName.c_str(), ClassName.c_str());
		LoggedClasses.insert(OriginalClassName);
	}

	return nullptr;
}
