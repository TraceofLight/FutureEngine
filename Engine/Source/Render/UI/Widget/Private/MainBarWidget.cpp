#include "pch.h"
#include <shobjidl.h>
#include "Render/UI/Widget/Public/MainBarWidget.h"
#include "Manager/UI/Public/UIManager.h"
#include "Manager/UI/Public/ViewportManager.h"
#include "Render/UI/Window/Public/UIWindow.h"
#include "Level/Public/Level.h"
#include "Render/Renderer/Public/Renderer.h"

IMPLEMENT_CLASS(UMainBarWidget, UWidget)

/**
 * @brief MainBarWidget 초기화 함수
 * UIManager 인스턴스를 여기서 가져온다
 */
void UMainBarWidget::Initialize()
{
	UIManager = &UUIManager::GetInstance();
	if (!UIManager)
	{
		UE_LOG("MainBarWidget: UIManager를 찾을 수 없습니다!");
		return;
	}

	UE_LOG("MainBarWidget: 메인 메뉴바 위젯이 초기화되었습니다");
}

void UMainBarWidget::Update()
{
	// 업데이트 정보 필요할 경우 추가할 것
}

void UMainBarWidget::RenderWidget()
{
	if (!bIsMenuBarVisible)
	{
		MenuBarHeight = 0.0f;
		return;
	}

	// 정보 팝업 렌더링
	if (bShowInfoPopup)
	{
		ImGui::OpenPopup("정보");
		bShowInfoPopup = false;
	}

	// 정보 팝업 모달 윈도우
	if (ImGui::BeginPopupModal("정보", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Alt + ` 를 누르면 콘솔창이 나옵니다");
		ImGui::Separator();
		if (ImGui::Button("확인", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// FutureEngine 스타일: 메뉴바 색상 및 스타일 설정
	const ImVec4 MenuBarBg = ImVec4(pow(0.12f, 2.2f), pow(0.12f, 2.2f), pow(0.12f, 2.2f), 1.0f);
	const ImVec4 PopupBg = ImVec4(pow(0.09f, 2.2f), pow(0.09f, 2.2f), pow(0.09f, 2.2f), 1.0f);
	const ImVec4 Header = ImVec4(pow(0.20f, 2.2f), pow(0.20f, 2.2f), pow(0.20f, 2.2f), 1.0f);
	const ImVec4 HeaderHovered = ImVec4(pow(0.35f, 2.2f), pow(0.35f, 2.2f), pow(0.35f, 2.2f), 1.0f);
	const ImVec4 HeaderActive = ImVec4(pow(0.50f, 2.2f), pow(0.50f, 2.2f), pow(0.50f, 2.2f), 1.0f);
	const ImVec4 TextColor = ImVec4(pow(0.92f, 2.2f), pow(0.92f, 2.2f), pow(0.92f, 2.2f), 1.0f);

	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, MenuBarBg);
	ImGui::PushStyleColor(ImGuiCol_PopupBg, PopupBg);
	ImGui::PushStyleColor(ImGuiCol_Header, Header);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, HeaderHovered);
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, HeaderActive);
	ImGui::PushStyleColor(ImGuiCol_Text, TextColor);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));

	// FutureEngine 철학: 독립 윈도우로 메뉴바 위치 조정
	const ImVec2 screenSize = ImGui::GetIO().DisplaySize;
	constexpr float TopPadding = 3.0f;
	ImGui::SetNextWindowPos(ImVec2(0.0f, TopPadding));
	ImGui::SetNextWindowSize(ImVec2(screenSize.x, 20.0f));

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("MainMenuBarContainer", nullptr, flags))
	{
		if (ImGui::BeginMenuBar())
		{
			MenuBarHeight = ImGui::GetWindowSize().y;

			// FutureEngine 철학: 왼쪽 60px 더미 공간 (로고 자리)
			ImGui::Dummy(ImVec2(60.0f, 0.0f));
			ImGui::SameLine(0.0f, 0.0f);

			// 메뉴 Listing - FutureEngine 스타일
			RenderFileMenu();
			RenderViewMenu();
			RenderShowFlagsMenu();
			RenderWindowsMenu();
			RenderToolsMenu();
			RenderHelpMenu();

			// Custom window controls
			RenderWindowControls();

			ImGui::EndMenuBar();
		}
		else
		{
			MenuBarHeight = 0.0f;
		}
		ImGui::End();
	}
	else
	{
		MenuBarHeight = 0.0f;
	}

	// 스타일 복원
	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(6);
}


/**
 * @brief Windows 토글 메뉴를 렌더링하는 함수
 * 등록된 MainMenu를 제외한 모든 UIWindow의 토글 옵션을 표시
 */
void UMainBarWidget::RenderWindowsMenu() const
{
	if (ImGui::BeginMenu("창"))
	{
		if (!UIManager)
		{
			ImGui::Text("UIManager를 사용할 수 없습니다");
			ImGui::EndMenu();
			return;
		}

		// 모든 등록된 UIWindow에 대해 토글 메뉴 항목 생성
		const auto& AllWindows = UIManager->GetAllUIWindows();

		if (AllWindows.IsEmpty())
		{
			ImGui::Text("등록된 창이 없습니다");
		}
		else
		{
			for (auto* Window : AllWindows)
			{
				if (!Window)
				{
					continue;
				}

				if (Window->GetWindowTitle() == "MainMenuBar")
				{
					continue;
				}

				if (ImGui::MenuItem(Window->GetWindowTitle().ToString().data(), nullptr, Window->IsVisible()))
				{
					Window->ToggleVisibility();

					UE_LOG("MainBarWidget: %s 창 토글됨 (현재 상태: %s)",
						Window->GetWindowTitle().ToString().data(),
						Window->IsVisible() ? "표시" : "숨김");

					// Outliner나 Details 패널의 visibility가 변경되면 layout 재정리
					const FName WindowTitle = Window->GetWindowTitle();
					if (WindowTitle == "Outliner" || WindowTitle == "Details")
					{
						UIManager->OnPanelVisibilityChanged();
					}
				}
			}

			ImGui::Separator();

			// 전체 창 제어 옵션
			if (ImGui::MenuItem("모든 창 표시"))
			{
				UIManager->ShowAllWindows();
				UE_LOG("MainBarWidget: 모든 창 표시됨");
			}

			if (ImGui::MenuItem("모든 창 숨김"))
			{
				for (auto* Window : UIManager->GetAllUIWindows())
				{
					if (!Window)
					{
						continue;
					}

					if (Window->GetWindowTitle() == "MainMenuBar")
					{
						continue;
					}

					Window->SetWindowState(EUIWindowState::Hidden);
				}

				UE_LOG("MainBarWidget: 모든 창 숨겨짐");
			}
		}

		ImGui::EndMenu();
	}
}

/**
 * @brief File 메뉴를 렌더링합니다
 */
void UMainBarWidget::RenderFileMenu()
{
	if (ImGui::BeginMenu("파일"))
	{
		// 레벨 관련 메뉴
		if (ImGui::MenuItem("새 레벨", "Ctrl+N"))
		{
			CreateNewLevel();
		}

		ImGui::Separator();

		if (ImGui::MenuItem("레벨 열기", "Ctrl+O"))
		{
			LoadLevel();
		}

		if (ImGui::MenuItem("레벨 저장", "Ctrl+S"))
		{
			SaveCurrentLevel();
		}

		ImGui::Separator();

		// 일반 파일 작업
		if (ImGui::MenuItem("일반 파일 열기"))
		{
			UE_LOG("MainBarWidget: 일반 파일 열기 메뉴 선택됨");
			// TODO(KHJ): 일반 파일 열기 로직 구현
		}

		ImGui::Separator();

		if (ImGui::MenuItem("종료", "Alt+F4"))
		{
			UE_LOG("MainBarWidget: 프로그램 종료 메뉴 선택됨");
			// TODO(KHJ): 프로그램 종료 로직 구현
		}

		ImGui::EndMenu();
	}
}
/**
 * @brief View 메뉴를 렌더링하는 함수
 * ViewMode 선택 기능 (Lit, Unlit, Wireframe)
 */
void UMainBarWidget::RenderViewMenu()
{
	if (ImGui::BeginMenu("보기"))
	{
		// ViewportManager에서 현재 활성 뷰포트의 ViewMode 가져오기
		UViewportManager& ViewportMgr = UViewportManager::GetInstance();
		EViewModeIndex CurrentMode = ViewportMgr.GetActiveViewportViewMode();

		// ViewMode 메뉴 아이템
		bool bIsGroraud = (CurrentMode == EViewModeIndex::VMI_Gouraud);
		bool bIsLambert = (CurrentMode == EViewModeIndex::VMI_Lambert);
		bool bIsBlinnPhong = (CurrentMode == EViewModeIndex::VMI_BlinnPhong);
		bool bIsUnlit = (CurrentMode == EViewModeIndex::VMI_Unlit);
		bool bIsWireframe = (CurrentMode == EViewModeIndex::VMI_Wireframe);
		bool bIsSceneDepth = (CurrentMode == EViewModeIndex::VMI_SceneDepth);
		bool bIsWorldNormal = (CurrentMode == EViewModeIndex::VMI_WorldNormal);

		//if (ImGui::MenuItem("조명 적용(Lit)", nullptr, bIsLit) && !bIsLit)
		//{
		//	EditorInstance->SetViewMode(EViewModeIndex::VMI_Lit);
		//	UE_LOG("MainBarWidget: ViewMode를 Lit으로 변경");
		//}
		if (ImGui::MenuItem("고로 셰이딩 적용(Gouraud)", nullptr, bIsGroraud) && !bIsGroraud)
		{
			ViewportMgr.SetActiveViewportViewMode(EViewModeIndex::VMI_Gouraud);
			UE_LOG("MainBarWidget: 활성 뷰포트의 ViewMode를 고로 셸이딩으로 변경");
		}
		if (ImGui::MenuItem("램버트 셰이딩 적용(Lambert)", nullptr, bIsLambert) && !bIsLambert)
		{
			ViewportMgr.SetActiveViewportViewMode(EViewModeIndex::VMI_Lambert);
			UE_LOG("MainBarWidget: 활성 뷰포트의 ViewMode를 램버트 셸이딩로 변경");
		}
		if (ImGui::MenuItem("블린-퐁 셰이딩 적용(Blinn-Phong)", nullptr, bIsBlinnPhong) && !bIsBlinnPhong)
		{
			ViewportMgr.SetActiveViewportViewMode(EViewModeIndex::VMI_BlinnPhong);
			UE_LOG("MainBarWidget: 활성 뷰포트의 ViewMode를 블린-펑 셸이딩으로 변경");
		}
		if (ImGui::MenuItem("조명 비적용(Unlit)", nullptr, bIsUnlit) && !bIsUnlit)
		{
			ViewportMgr.SetActiveViewportViewMode(EViewModeIndex::VMI_Unlit);
			UE_LOG("MainBarWidget: 활성 뷰포트의 ViewMode를 Unlit으로 변경");
		}

		if (ImGui::MenuItem("와이어프레임(Wireframe)", nullptr, bIsWireframe) && !bIsWireframe)
		{
			ViewportMgr.SetActiveViewportViewMode(EViewModeIndex::VMI_Wireframe);
			UE_LOG("MainBarWidget: 활성 뷰포트의 ViewMode를 Wireframe으로 변경");
		}

		if (ImGui::MenuItem("씬 뎁스(SceneDepth)", nullptr, bIsSceneDepth) && !bIsSceneDepth)
		{
			ViewportMgr.SetActiveViewportViewMode(EViewModeIndex::VMI_SceneDepth);
			UE_LOG("MainBarWidget: 활성 뷰포트의 ViewMode를 SceneDepth으로 변경");
		}

		if (ImGui::MenuItem("월드 노멀(WorldNormal)", nullptr, bIsWorldNormal) && !bIsWorldNormal)
		{
			ViewportMgr.SetActiveViewportViewMode(EViewModeIndex::VMI_WorldNormal);
			UE_LOG("MainBarWidget: 활성 뷰포트의 ViewMode를 WorldNormal으로 변경");
		}

		ImGui::EndMenu();
	}
}

/**
 * @brief ShowFlags 메뉴를 렌더링하는 함수
 * Static Mesh, BillBoard 등의 플래그 상태 확인 및 토글
 */
void UMainBarWidget::RenderShowFlagsMenu()
{
	if (ImGui::BeginMenu("표시 옵션"))
	{
		// 현재 레벨 가져오기
		ULevel* CurrentLevel = GWorld->GetLevel();
		if (!CurrentLevel)
		{
			ImGui::Text("현재 레벨을 찾을 수 없습니다");
			ImGui::EndMenu();
			return;
		}

		// ShowFlags 가져오기
		uint64 ShowFlags = CurrentLevel->GetShowFlags();

		// BillBoard 표시 옵션
		bool bShowBillboard = (ShowFlags & EEngineShowFlags::SF_Billboard) != 0;
		if (ImGui::MenuItem("빌보드 표시", nullptr, bShowBillboard))
		{
			if (bShowBillboard)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Billboard);
				UE_LOG("MainBarWidget: 빌보드 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Billboard);
				UE_LOG("MainBarWidget: 빌보드 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// UUID 표시 옵션
		bool bShowUUID = (ShowFlags & EEngineShowFlags::SF_UUID) != 0;
		if (ImGui::MenuItem("UUID 표시", nullptr, bShowUUID))
		{
			if (bShowUUID)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_UUID);
				UE_LOG("MainBarWidget: UUID 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_UUID);
				UE_LOG("MainBarWidget: UUID 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// Bounds 표시 옵션
		bool bShowBounds = (ShowFlags & EEngineShowFlags::SF_Bounds) != 0;
		if (ImGui::MenuItem("바운딩박스 표시", nullptr, bShowBounds))
		{
			if (bShowBounds)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Bounds);
				UE_LOG("MainBarWidget: 바운딩박스 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Bounds);
				UE_LOG("MainBarWidget: 바운딩박스 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// StaticMesh 표시 옵션
		bool bShowStaticMesh = (ShowFlags & EEngineShowFlags::SF_StaticMesh) != 0;
		if (ImGui::MenuItem("스태틱 메쉬 표시", nullptr, bShowStaticMesh))
		{
			if (bShowStaticMesh)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_StaticMesh);
				UE_LOG("MainBarWidget: 스태틱 메쉬 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_StaticMesh);
				UE_LOG("MainBarWidget: 스태틱 메쉬 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// Text 표시 옵션
		bool bShowText = (ShowFlags & EEngineShowFlags::SF_Text) != 0;
		if (ImGui::MenuItem("텍스트 표시", nullptr, bShowText))
		{
			if (bShowText)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Text);
				UE_LOG("MainBarWidget: 텍스트 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Text);
				UE_LOG("MainBarWidget: 텍스트 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// Decal 표시 옵션
		bool bShowDecal = (ShowFlags & EEngineShowFlags::SF_Decal) != 0;
		if (ImGui::MenuItem("데칼 표시", nullptr, bShowDecal))
		{
			if (bShowDecal)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Decal);
				UE_LOG("MainBarWidget: 데칼 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Decal);
				UE_LOG("MainBarWidget: 데칼 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// Octree 표시 옵션
		bool bShowFog = (ShowFlags & EEngineShowFlags::SF_Fog) != 0;
		if (ImGui::MenuItem("Fog 표시", nullptr, bShowFog))
		{
			if (bShowFog)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Fog);
				UE_LOG("MainBarWidget: Fog 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Fog);
				UE_LOG("MainBarWidget: Fog 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}
		
		// Octree 표시 옵션
		bool bShowOctree = (ShowFlags & EEngineShowFlags::SF_Octree) != 0;
		if (ImGui::MenuItem("Octree 표시", nullptr, bShowOctree))
		{
			if (bShowOctree)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Octree);
				UE_LOG("MainBarWidget: Octree 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Octree);
				UE_LOG("MainBarWidget: Octree 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// FXAA 표시 옵션
		bool bEnableFXAA = (ShowFlags & EEngineShowFlags::SF_FXAA) != 0;
		if (ImGui::MenuItem("FXAA 적용", nullptr, bEnableFXAA))
		{
			if (bEnableFXAA)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_FXAA);
				UE_LOG("MainBarWidget: FXAA 비활성화");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_FXAA);
				UE_LOG("MainBarWidget: FXAA 활성화");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		ImGui::EndMenu();
	}
}

void UMainBarWidget::RenderToolsMenu()
{
	if (ImGui::BeginMenu("도구"))
	{
		if (ImGui::MenuItem("셰이더 핫 리로드"))
		{
			URenderer::GetInstance().HotReloadShaders();
			UE_LOG("Vertex Shader, PixelShader 핫 리로드 완료");
		}
		ImGui::EndMenu();
	}
}

/**
 * @brief Help 메뉴에 대한 렌더링 함수
 */
void UMainBarWidget::RenderHelpMenu()
{
	if (ImGui::BeginMenu("도움말"))
	{
		if (ImGui::MenuItem("정보", "F1"))
		{
			bShowInfoPopup = true;
			UE_LOG("MainBarWidget: 정보 메뉴 선택됨");
		}

		ImGui::EndMenu();
	}
}

/**
 * @brief 새 레벨 생성
 */
void UMainBarWidget::CreateNewLevel()
{
	if (!GEditor)
	{
		UE_LOG_ERROR("MainBarWidget: GEditor가 초기화되지 않았습니다");
		return;
	}
	
	// TODO: 레벨 이름 입력 다이얼로그 추가 가능
	FString LevelName = "NewLevel";
	
	bool bSuccess = GEditor->CreateNewLevel(LevelName);
	if (bSuccess)
	{
		UE_LOG_SUCCESS("MainBarWidget: 새 레벨 '%s' 생성 성공", LevelName.c_str());
	}
	else
	{
		UE_LOG_ERROR("MainBarWidget: 새 레벨 생성 실패");
	}
}

/**
 * @brief 레벨 열기
 */
void UMainBarWidget::LoadLevel()
{
	if (!GEditor)
	{
		UE_LOG_ERROR("MainBarWidget: GEditor가 초기화되지 않았습니다");
		return;
	}
	
	path FilePath = OpenLoadFileDialog();
	if (FilePath.empty())
	{
		UE_LOG("MainBarWidget: 레벨 열기 취소됨");
		return;
	}
	
	bool bSuccess = GEditor->LoadLevel(FilePath.string());
	if (bSuccess)
	{
		UE_LOG_SUCCESS("MainBarWidget: 레벨 로드 성공: %s", FilePath.string().c_str());
	}
	else
	{
		UE_LOG_ERROR("MainBarWidget: 레벨 로드 실패: %s", FilePath.string().c_str());
	}
}

/**
 * @brief 현재 레벨 저장
 */
void UMainBarWidget::SaveCurrentLevel()
{
	if (!GEditor)
	{
		UE_LOG_ERROR("MainBarWidget: GEditor가 초기화되지 않았습니다");
		return;
	}
	
	path FilePath = OpenSaveFileDialog();
	if (FilePath.empty())
	{
		UE_LOG("MainBarWidget: 레벨 저장 취소됨");
		return;
	}
	
	bool bSuccess = GEditor->SaveCurrentLevel(FilePath.string());
	if (bSuccess)
	{
		UE_LOG_SUCCESS("MainBarWidget: 레벨 저장 성공: %s", FilePath.string().c_str());
	}
	else
	{
		UE_LOG_ERROR("MainBarWidget: 레벨 저장 실패: %s", FilePath.string().c_str());
	}
}

/**
 * @brief 파일 열기 다이얼로그
 */
path UMainBarWidget::OpenLoadFileDialog()
{
	path ResultPath = L"";
	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	
	// COM 초기화 실패 시 early return
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
	{
		return ResultPath;
	}
	
	// COM이 이미 초기화되어 있는지 확인 (S_FALSE 또는 RPC_E_CHANGED_MODE)
	bool bNeedUninitialize = (hr == S_OK);
	
	IFileOpenDialog* pFileOpen = nullptr;
	hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
		IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
	
	if (SUCCEEDED(hr) && pFileOpen)
	{
		COMDLG_FILTERSPEC fileTypes[] = {
			{L"Scene Files (*.scene)", L"*.scene"},
			{L"All Files (*.*)", L"*.*"}
		};
		pFileOpen->SetFileTypes(ARRAYSIZE(fileTypes), fileTypes);
		pFileOpen->SetFileTypeIndex(1);
		pFileOpen->SetTitle(L"Load Level");
		
		DWORD dwFlags;
		pFileOpen->GetOptions(&dwFlags);
		pFileOpen->SetOptions(dwFlags | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);
		
		hr = pFileOpen->Show(GetActiveWindow());
		if (SUCCEEDED(hr))
		{
			IShellItem* pItem = nullptr;
			hr = pFileOpen->GetResult(&pItem);
			if (SUCCEEDED(hr) && pItem)
			{
				PWSTR pszFilePath = nullptr;
				hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
				if (SUCCEEDED(hr) && pszFilePath)
				{
					ResultPath = path(pszFilePath);
					CoTaskMemFree(pszFilePath);
				}
				pItem->Release();
			}
		}
		pFileOpen->Release();
	}
	
	// COM을 우리가 초기화한 경우에만 Uninitialize
	if (bNeedUninitialize)
	{
		CoUninitialize();
	}
	
	return ResultPath;
}

/**
 * @brief 파일 저장 다이얼로그
 */
path UMainBarWidget::OpenSaveFileDialog()
{
	path ResultPath = L"";
	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	
	// COM 초기화 실패 시 early return
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
	{
		return ResultPath;
	}
	
	// COM이 이미 초기화되어 있는지 확인 (S_FALSE 또는 RPC_E_CHANGED_MODE)
	bool bNeedUninitialize = (hr == S_OK);
	
	IFileSaveDialog* pFileSave = nullptr;
	hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL,
		IID_IFileSaveDialog, reinterpret_cast<void**>(&pFileSave));
	
	if (SUCCEEDED(hr) && pFileSave)
	{
		COMDLG_FILTERSPEC fileTypes[] = {
			{L"Scene Files (*.scene)", L"*.scene"},
			{L"All Files (*.*)", L"*.*"}
		};
		pFileSave->SetFileTypes(ARRAYSIZE(fileTypes), fileTypes);
		pFileSave->SetFileTypeIndex(1);
		pFileSave->SetDefaultExtension(L"Scene");
		pFileSave->SetTitle(L"Save Level");
		
		DWORD dwFlags;
		pFileSave->GetOptions(&dwFlags);
		pFileSave->SetOptions(dwFlags | FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST);
		
		hr = pFileSave->Show(GetActiveWindow());
		if (SUCCEEDED(hr))
		{
			IShellItem* pItem = nullptr;
			hr = pFileSave->GetResult(&pItem);
			if (SUCCEEDED(hr) && pItem)
			{
				PWSTR pszFilePath = nullptr;
				hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
				if (SUCCEEDED(hr) && pszFilePath)
				{
					ResultPath = path(pszFilePath);
					CoTaskMemFree(pszFilePath);
				}
				pItem->Release();
			}
		}
		pFileSave->Release();
	}
	
	// COM을 우리가 초기화한 경우에만 Uninitialize
	if (bNeedUninitialize)
	{
		CoUninitialize();
	}
	
	return ResultPath;
}

void UMainBarWidget::RenderWindowControls() const
{
	HWND MainWindowHandle = GetActiveWindow();
	if (!MainWindowHandle)
	{
		return;
	}

	// 우측 정렬을 위해 여백 계산
	const float ButtonWidth = 46.0f;
	const float ButtonCount = 3.0f;
	const float Padding = 30.0f;
	const float TotalWidth = ButtonWidth * ButtonCount + Padding;

	// DisplaySize 사용 (윈도우 크기 변경 시 즉시 반영)
	const float ScreenWidth = ImGui::GetIO().DisplaySize.x;
	const float ButtonPosX = ScreenWidth - TotalWidth;

	// 버튼이 화면 밖으로 나가지 않도록 보호
	if (ButtonPosX > 0.0f)
	{
		ImGui::SameLine(ButtonPosX);
	}
	else
	{
		ImGui::SameLine();
	}

	// 버튼 기본 스타일 설정
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));

	// 최소화 버튼
	if (ImGui::Button("-", ImVec2(ButtonWidth, 0)))
	{
		ShowWindow(MainWindowHandle, SW_MINIMIZE);
	}

	// 최대화 / 복원 버튼
	ImGui::SameLine();
	bool bIsMaximized = IsZoomed(MainWindowHandle);
	if (ImGui::Button("□", ImVec2(ButtonWidth, 0)))
	{
		// 상단바 더블클릭과 동일한 방식 사용
		SendMessageW(MainWindowHandle, WM_SYSCOMMAND, bIsMaximized ? SC_RESTORE : SC_MAXIMIZE, 0);
	}

	ImGui::PopStyleColor(3);

	// 닫기 버튼
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
	if (ImGui::Button("X", ImVec2(ButtonWidth, 0)))
	{
		PostMessageW(MainWindowHandle, WM_CLOSE, 0, 0);
	}
	ImGui::PopStyleColor(3);
}
