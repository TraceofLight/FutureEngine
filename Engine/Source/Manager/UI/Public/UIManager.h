#pragma once
#include "Core/Public/Object.h"

class UUIWindow;
class UImGuiHelper;
class UMainMenuWindow;
class ULevelTabBarWindow;
class UStatusBarWidget;
/**
 * @brief UI 매니저 클래스
 * 모든 UI 윈도우를 관리하는 싱글톤 클래스
 * @param UIWindows 등록된 모든 UI 윈도우들
 * @param FocusedWindow 현재 포커스된 윈도우
 * @param bIsInitialized 초기화 상태
 * @param TotalTime 전체 경과 시간
 */
UCLASS()
class UUIManager : public UObject
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(UUIManager, UObject)

public:
	void Initialize();
	void Initialize(HWND InWindowHandle);
	void Shutdown();
	void Update();
	void Render();
	bool RegisterUIWindow(UUIWindow* InWindow);
	bool UnregisterUIWindow(UUIWindow* InWindow);
	void PrintDebugInfo() const;

	UUIWindow* FindUIWindow(const FName& InWindowName) const;
	UWidget* FindWidget(const FName& InWidgetName) const;
	void HideAllWindows() const;
	void ShowAllWindows() const;

	// 윈도우 최소화 / 복원 처리
	void OnWindowMinimized();
	void OnWindowRestored();

	// Getter & Setter
	size_t GetUIWindowCount() const { return UIWindows.Num(); }
	const TArray<UUIWindow*>& GetAllUIWindows() const { return UIWindows; }
	UUIWindow* GetFocusedWindow() const { return FocusedWindow; }

	void SetFocusedWindow(UUIWindow* InWindow);

	// ImGui 관련 메서드
	static LRESULT WndProcHandler(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam);

	void RepositionImGuiWindows() const;

	// 메인 메뉴바 관련 메서드
	void RegisterMainMenuWindow(UMainMenuWindow* InMainMenuWindow);
	float GetMainMenuBarHeight() const;
	float GetRightPanelWidth() const;
	void ArrangeRightPanels();
	void ForceArrangeRightPanels();
	void OnPanelVisibilityChanged();

	// 레벨 바 관련 메서드
	void RegisterLevelTabBarWindow(ULevelTabBarWindow* InLevelBarWindow);

	// 상태바 관련 메서드
	void RegisterStatusBarWidget(UStatusBarWidget* InStatusBarWidget);
	float GetStatusBarHeight() const;
	
	void OnSelectedComponentChanged(UActorComponent* InSelectedComponent) const;

private:
	TArray<UUIWindow*> UIWindows;
	UUIWindow* FocusedWindow = nullptr;
	bool bIsInitialized = false;
	float TotalTime = 0.0f;

	// 윈도우 상태 저장
	struct FUIWindowSavedState
	{
		uint32 WindowID;
		ImVec2 SavedPosition;
		ImVec2 SavedSize;
		bool bWasVisible;
	};

	TArray<FUIWindowSavedState> SavedWindowStates;
	bool bIsMinimized = false;

	// ImGui Helper
	UImGuiHelper* ImGuiHelper = nullptr;

	// 레벨바 윈도우
	ULevelTabBarWindow* LevelTabBarWindow = nullptr;

	// Main Menu Window
	UMainMenuWindow* MainMenuWindow = nullptr;

	// Status Bar Widget
	UStatusBarWidget* StatusBarWidget = nullptr;

	void SortUIWindowsByPriority();
	void UpdateFocusState();

	// 오른쪽 패널 상태 추적 변수들
	float SavedOutlinerHeightForDual = 0.0f; // 두 패널이 있을 때 Outliner 높이
	float SavedDetailHeightForDual = 0.0f; // 두 패널이 있을 때 Detail 높이
	float SavedPanelWidth = 0.0f; // 기억된 패널 너비
	bool bHasSavedDualLayout = false; // 두 패널 레이아웃이 저장되어 있는지



	// 오른쪽 패널 레이아웃 헬퍼 함수
	void ArrangeRightPanelsInitial(UUIWindow* InOutlinerWindow, UUIWindow* InDetailWindow,
		float InScreenWidth, float InScreenHeight, float InMenuBarHeight,
		float InAvailableHeight);
	void ArrangeRightPanelsDynamic(UUIWindow* InOutlinerWindow, UUIWindow* InDetailWindow,
		float InScreenWidth, float InScreenHeight, float InMenuBarHeight,
		float InAvailableHeight, float InTargetWidth,
		bool bOutlinerHeightChanged, bool bDetailHeightChanged);
};
