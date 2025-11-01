#include "pch.h"
#include "Render/UI/Window/Public/UIWindow.h"
#include "ImGui/imgui_internal.h"
#include "Render/UI/Widget/Public/Widget.h"
#include "Manager/UI/Public/UIManager.h"

IMPLEMENT_ABSTRACT_CLASS(UUIWindow, UObject)
int UUIWindow::IssuedWindowID = 0;

UUIWindow::UUIWindow() : Config(FUIWindowConfig()), CurrentState(Config.InitialState)
{
	// 고유한 윈도우 ID 생성
	WindowID = ++IssuedWindowID;

	// 윈도우 플래그 업데이트
	Config.UpdateWindowFlags();

	// 초기 상태 설정
	LastWindowSize = Config.DefaultSize;
	LastWindowPosition = Config.DefaultPosition;

	// 초기 상태가 Visible이면 윈도우도 열린 상태로 설정
	bIsWindowOpen = (CurrentState == EUIWindowState::Visible || CurrentState == EUIWindowState::Maximized);

	if (IssuedWindowID == 1)
	{
		cout << "UIWindow: Created: " << WindowID << "\n";
	}
	else
	{
		UE_LOG("UIWindow: Created: %u", WindowID);
	}
}

UUIWindow::~UUIWindow()
{
	for (UWidget* Widget : Widgets)
	{
		if (!Widget->IsSingleton())
		{
			SafeDelete(Widget);
		}
	}
}

void UUIWindow::DeleteWidget(UWidget* Widget)
{
	if (Widget)
	{
		WidgetsToDelete.Add(Widget);
	}
}

void UUIWindow::ClearWidget()
{
	for (UWidget* Widget : Widgets)
	{
		if (!Widget->IsSingleton())
		{
			SafeDelete(Widget);
		}
	}
	Widgets.Empty(); // 배열 비우기
}

/**
 * @brief 뷰포트가 리사이징 되었을 때 앵커/좌상단 기준 상대 위치 비율을 고정하는 로직
 */
void UUIWindow::OnMainWindowResized() const
{
	if (!ImGui::GetCurrentContext() || !IsVisible())
		return;

	const ImGuiViewport* Viewport = ImGui::GetMainViewport();
	const ImVec2 CurrentViewportSize = Viewport->WorkSize;

	const ImVec2 Anchor = PositionRatio;
	const ImVec2 Pivot = {0.f, 0.f};

	ImVec2 ResponsiveSize(
		CurrentViewportSize.x * SizeRatio.x,
		CurrentViewportSize.y * SizeRatio.y
	);

	ImVec2 TargetPosition(
		Viewport->WorkPos.x + CurrentViewportSize.x * Anchor.x,
		Viewport->WorkPos.y + CurrentViewportSize.y * Anchor.y
	);

	ImVec2 FinalPosition(
		TargetPosition.x - ResponsiveSize.x * Pivot.x,
		TargetPosition.y - ResponsiveSize.y * Pivot.y
	);

	ImGui::SetWindowSize(ResponsiveSize, ImGuiCond_Always);
	ImGui::SetWindowPos(FinalPosition, ImGuiCond_Always);
}

/**
 * @brief 윈도우가 뷰포트 범위 밖으로 나갔을 시 클램프 하는 로직
 */
void UUIWindow::ClampWindow() const
{
	if (!IsVisible())
	{
		return;
	}

	const ImGuiViewport* Viewport = ImGui::GetMainViewport();
	const ImVec2 WorkPosition = Viewport->WorkPos;
	const ImVec2 WorkSize = Viewport->WorkSize;
	float MenuBarOffset = GetMenuBarOffset();

	ImVec2 Position = LastWindowPosition;
	ImVec2 Size = LastWindowSize;

	bool bSizeChanged = false;

	if (Size.x > WorkSize.x)
	{
		Size.x = WorkSize.x;
		bSizeChanged = true;
	}

	if (Size.y > WorkSize.y)
	{
		Size.y = WorkSize.y;
		bSizeChanged = true;
	}

	if (bSizeChanged)
	{
		ImGui::SetWindowSize(Size);
	}

	bool bPositionChanged = false;

	if (Position.x + Size.x > WorkPosition.x + WorkSize.x)
	{
		Position.x = WorkPosition.x + WorkSize.x - Size.x;
		bPositionChanged = true;
	}

	if (Position.y + Size.y > WorkPosition.y + WorkSize.y)
	{
		Position.y = WorkPosition.y + WorkSize.y - Size.y;
		bPositionChanged = true;
	}

	if (Position.x < WorkPosition.x)
	{
		Position.x = WorkPosition.x;
		bPositionChanged = true;
	}

	if (Position.y < WorkPosition.y + MenuBarOffset)
	{
		Position.y = WorkPosition.y + MenuBarOffset;
		bPositionChanged = true;
	}

	if (bPositionChanged)
	{
		ImGui::SetWindowPos(Position);
	}
}

/**
 * @brief 내부적으로 사용되는 윈도우 렌더링 처리
 * 서브클래스에서 직접 호출할 수 없도록 접근한정자 설정
 */
void UUIWindow::RenderWindow()
{
	// bIsWindowOpen이 false면 (프로그램적으로 닫힌 상태) 렌더링하지 않음
	// IsVisible()은 CurrentState를 체크하는데, X 버튼 감지를 위해서는 ImGui::Begin이 호출되어야 하므로
	// bIsWindowOpen이 true면 일단 Begin을 호출해야 함
	if (!bIsWindowOpen)
	{
		return;
	}

	// 도킹 설정 적용
	ApplyDockingSettings();

	// 메인 메뉴바 높이 고려용 오프셋 계산
	float MenuBarOffset = GetMenuBarOffset();

	// FIXME(KHJ): ImGui 윈도우 복원 로직이 잘 작동하지 않음. 중복 코드도 많아 개편 및 작동 수정 필요
	// ImGui 윈도우 시작
	// 복원이 필요한 경우 위치와 크기 강제 설정
	if (bShouldRestorePosition && RestoreFrameCount > 0)
	{
		ImVec2 AdjustedPosition = LastWindowPosition;
		AdjustedPosition.y = max(AdjustedPosition.y, MenuBarOffset);
		ImGui::SetNextWindowPos(AdjustedPosition, ImGuiCond_Always);
		--RestoreFrameCount;
		if (RestoreFrameCount <= 0)
		{
			bShouldRestorePosition = false;
		}
	}

	else if (!bShouldRestorePosition)
	{
		ImVec2 AdjustedDefaultPosition = Config.DefaultPosition;
		AdjustedDefaultPosition.y = max(AdjustedDefaultPosition.y, MenuBarOffset);
		ImGui::SetNextWindowPos(AdjustedDefaultPosition, ImGuiCond_FirstUseEver);
	}

	// ImGui의 내부 상태를 무시하고 강제로 적용
	if (bShouldRestoreSize && RestoreFrameCount > 0)
	{
		ImGui::SetNextWindowSize(LastWindowSize, ImGuiCond_Always);
		ImGui::SetNextWindowSizeConstraints(LastWindowSize, LastWindowSize);

		// ImGui 내부 상태 초기화를 위해 FirstUseEver도 시도
		ImGui::SetNextWindowSize(LastWindowSize, ImGuiCond_FirstUseEver);

		// 크기 복원도 같은 프레임 카운터 사용
		if (RestoreFrameCount <= 0)
		{
			bShouldRestoreSize = false;
			bForceSize = false;
			ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(10000, 10000));
		}
	}

	else if (!bShouldRestoreSize)
	{
		ImGui::SetNextWindowSize(Config.DefaultSize, ImGuiCond_FirstUseEver);
	}

	OnPreRenderWindow(MenuBarOffset);

	bool bIsOpen = bIsWindowOpen;

	if (ImGui::Begin(Config.WindowTitle.ToString().data(), &bIsOpen, Config.WindowFlags))
	{
		// 잘 적용되지 않는 문제로 인해 여러 번 강제 적용 시도
		if (bShouldRestoreSize && RestoreFrameCount > 0)
		{
			ImGui::SetWindowSize(LastWindowSize, ImGuiCond_Always);
			ImGui::SetWindowSize(LastWindowSize);
			if (bShouldRestorePosition)
			{
				ImGui::SetWindowPos(LastWindowPosition, ImGuiCond_Always);
			}
		}

		if (!bIsResized)
		{
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImVec2 currentPos = ImGui::GetWindowPos();
			ImVec2 currentSize = ImGui::GetWindowSize();
			ImVec2 pivot = {0.f, 0.f};

			PositionRatio.x = (currentPos.x - viewport->WorkPos.x + currentSize.x * pivot.x) / viewport->WorkSize.x;
			PositionRatio.y = (currentPos.y - viewport->WorkPos.y + currentSize.y * pivot.y) / viewport->WorkSize.y;

			SizeRatio.x = currentSize.x / viewport->WorkSize.x;
			SizeRatio.y = currentSize.y / viewport->WorkSize.y;
		}
		// 실제 UI 컨텐츠 렌더링
		RenderWidget();

		// 윈도우 정보 업데이트
		UpdateWindowInfo();
	}

	if (bIsResized)
	{
		OnMainWindowResized();
		bIsResized = false;
	}

	ImGui::End();

	// 윈도우가 닫혔는지 확인
	if (!bIsOpen && bIsWindowOpen)
	{
		if (OnWindowClose())
		{
			bIsWindowOpen = false;
			SetWindowState(EUIWindowState::Hidden);

			// Outliner나 Details 패널이 닫힐 경우 UIManager에 알림
			const FName WindowTitle = GetWindowTitle();
			if (WindowTitle == "Outliner" || WindowTitle == "Details")
			{
				UUIManager::GetInstance().OnPanelVisibilityChanged();
			}
		}
		else
		{
			// 닫기 취소
			bIsWindowOpen = true;
		}
	}
}

void UUIWindow::RenderWidget() 
{
	// 위젯 추가 대비 복사본 순회
	TArray<UWidget*> WidgetsToRender = Widgets; 

	for (auto* Widget : WidgetsToRender)
	{
		if (Widget && find(WidgetsToDelete.begin(), WidgetsToDelete.end(), Widget) == WidgetsToDelete.end())
		{
			Widget->RenderWidget();
			Widget->PostProcess();
		}
	}

	ProcessDeleteWidgets();
}

void UUIWindow::Update() const
{
	for (UWidget* Widget : Widgets)
	{
		Widget->Update();
	}
}

void UUIWindow::ProcessDeleteWidgets()
{
	if (WidgetsToDelete.IsEmpty())
	{
		return;
	}

	TArray<UWidget*> DeletionSet;
	for (auto* Widget : WidgetsToDelete)
	{
		if (Widget && !Widget->IsSingleton())
		{
			DeletionSet.Add(Widget);
			SafeDelete(Widget);
		}
	}

	Widgets.RemoveAll(
		  [&DeletionSet](UWidget* Widget)
		  {
		  	 return DeletionSet.Contains(Widget);
		  });

	WidgetsToDelete.Empty();
}

/**
 * @brief 윈도우 위치와 크기 자동 설정
 * XXX(KHJ): 기본적으로 가볍게 어디에 위치할지 세팅할 수 있게 잡아두긴 했는데 잘 안 쓰는 느낌, 필요 없다면 Deprecated 처리할 것
 */
void UUIWindow::ApplyDockingSettings() const
{
	ImGuiIO& IO = ImGui::GetIO();
	float ScreenWidth = IO.DisplaySize.x;
	float ScreenHeight = IO.DisplaySize.y;
	float menuBarOffset = GetMenuBarOffset();

	switch (Config.DockDirection)
	{
	case EUIDockDirection::Left:
		ImGui::SetNextWindowPos(ImVec2(0, menuBarOffset), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(Config.DefaultSize.x, ScreenHeight - menuBarOffset), ImGuiCond_FirstUseEver);
		break;

	case EUIDockDirection::Right:
		ImGui::SetNextWindowPos(ImVec2(ScreenWidth - Config.DefaultSize.x, menuBarOffset), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(Config.DefaultSize.x, ScreenHeight - menuBarOffset), ImGuiCond_FirstUseEver);
		break;

	case EUIDockDirection::Top:
		ImGui::SetNextWindowPos(ImVec2(0, menuBarOffset), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(ScreenWidth, Config.DefaultSize.y), ImGuiCond_FirstUseEver);
		break;

	case EUIDockDirection::Bottom:
		ImGui::SetNextWindowPos(ImVec2(0, ScreenHeight - Config.DefaultSize.y), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(ScreenWidth, Config.DefaultSize.y), ImGuiCond_FirstUseEver);
		break;

	case EUIDockDirection::BottomLeft:
	{
		const ImGuiViewport* Viewport = ImGui::GetMainViewport();
		const float Margin = 10.0f;
		ImVec2 WorkPos = Viewport ? Viewport->WorkPos : ImVec2(0.0f, 0.0f);
		ImVec2 WorkSize = Viewport ? Viewport->WorkSize : ImVec2(ScreenWidth, ScreenHeight);
		ImVec2 TargetPos(WorkPos.x + Margin, WorkPos.y + WorkSize.y - Config.DefaultSize.y - Margin);
		TargetPos.y = max(TargetPos.y, menuBarOffset);
		ImGui::SetNextWindowPos(TargetPos, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(Config.DefaultSize, ImGuiCond_FirstUseEver);
	}
		break;

	case EUIDockDirection::Center:
		{
			ImVec2 Center = ImVec2(ScreenWidth * 0.5f, (ScreenHeight + menuBarOffset) * 0.5f);
			ImVec2 WindowPosition = ImVec2(Center.x - Config.DefaultSize.x * 0.5f,
			                               Center.y - Config.DefaultSize.y * 0.5f);
			// 최소 Y 위치는 메뉴바 아래로 제한
			WindowPosition.y = max(WindowPosition.y, menuBarOffset);
			ImGui::SetNextWindowPos(WindowPosition, ImGuiCond_FirstUseEver);
		}
		break;

	case EUIDockDirection::None:
	default:
		// 기본 위치 사용
		break;
	}
}

void UUIWindow::OnPreRenderWindow(float MenuBarOffset)
{
	// Default implementation does nothing. Derived windows may override.
}


/**
 * @brief ImGui 컨텍스트에서 현재 윈도우 정보 업데이트
 */
void UUIWindow::UpdateWindowInfo()
{
	// 현재 윈도우가 포커스되어 있는지 확인
	bool bCurrentlyFocused = ImGui::IsWindowFocused();

	if (bCurrentlyFocused != bIsFocused)
	{
		bIsFocused = bCurrentlyFocused;

		if (bIsFocused)
		{
			OnFocusGained();
		}
		else
		{
			OnFocusLost();
		}
	}

	// 윈도우 크기와 위치 업데이트
	LastWindowSize = ImGui::GetWindowSize();
	LastWindowPosition = ImGui::GetWindowPos();
}

/**
 * @brief 메인 메뉴바 높이를 고려한 오프셋을 반환하는 함수
 * @return 메뉴바 높이 (px)
 */
float UUIWindow::GetMenuBarOffset() const
{
	if (Config.WindowTitle.ToString() == "MainMenuBar")
	{
		return 0.0f;
	}

	// ImGui의 메인 뷰포트를 통해 메뉴바 높이 가져오기
	ImGuiViewport* Viewport = ImGui::GetMainViewport();
	if (Viewport)
	{
		float menuBarHeight = Viewport->WorkPos.y - Viewport->Pos.y;

		if (menuBarHeight > 0)
		{
			return menuBarHeight;
		}
	}

	// 기본값으로 임의의 표준 메뉴바 높이 반환
	return 23.0f;
}
