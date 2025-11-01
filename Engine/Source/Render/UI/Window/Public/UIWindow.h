#pragma once
#include "Core/Public/Object.h"
#include "ImGui/imgui.h"
#include "Render/UI/Factory/Public/UIWindowFactory.h"
#include "Actor/Public/Actor.h"

class UWidget;
/**
 * @brief UI 윈도우 표시 상태 열거형
 */
enum class EUIWindowState : uint8_t
{
	Hidden, // 숨김
	Visible, // 보임
	Minimized, // 최소화
	Maximized // 최대화
};

/**
 * @brief UI 윈도우 설정 구조체
 */
struct FUIWindowConfig
{
	FName WindowTitle = "Untitled Window"; // 윈도우 제목
	ImVec2 DefaultSize = ImVec2(300, 400); // 기본 크기
	ImVec2 DefaultPosition = ImVec2(100, 100); // 기본 위치
	ImVec2 MinSize = ImVec2(200, 150); // 최소 크기
	ImVec2 MaxSize = ImVec2(1920, 1080); // 최대 크기

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_None; // ImGui 윈도우 플래그
	EUIWindowState InitialState = EUIWindowState::Visible; // 초기 상태
	EUIDockDirection DockDirection = EUIDockDirection::None; // 도킹 방향

	bool bResizable = true; // 크기 조절 가능 여부
	bool bMovable = true; // 이동 가능 여부
	bool bCollapsible = true; // 접기 가능 여부
	bool bAutoFocus = false; // 자동 포커스 여부
	bool bBringToFrontOnFocus = true; // 포커스시 앞으로 가져오기

	int Priority = 0; // 렌더링 우선순위 (낮을수록 먼저 렌더링)

	/**
	 * @brief 기본 생성자
	 */
	FUIWindowConfig() = default;

	/**
	 * @brief 윈도우 플래그 자동 설정
	 */
	void UpdateWindowFlags()
	{
		WindowFlags = ImGuiWindowFlags_None;

		if (!bResizable) WindowFlags |= ImGuiWindowFlags_NoResize;
		if (!bMovable) WindowFlags |= ImGuiWindowFlags_NoMove;
		if (!bCollapsible) WindowFlags |= ImGuiWindowFlags_NoCollapse;
		if (bAutoFocus) WindowFlags |= ImGuiWindowFlags_NoFocusOnAppearing;
		if (!bBringToFrontOnFocus) WindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	}
};

/**
 * @brief 추상 UI 윈도우 기본 클래스
 *
 * @param Config 윈도우 설정
 * @param CurrentState 현재 윈도우 상태
 * @param WindowID 고유 윈도우 ID
 * @param bIsFocused 현재 포커스 상태
 * @param LastFocusTime 마지막 포커스 시간
 * @param bIsWindowOpen ImGui 윈도우 열림 상태
 * @param LastWindowSize 마지막 윈도우 크기
 * @param LastWindowPos 마지막 윈도우 위치
 */
class UUIWindow : public UObject
{
	DECLARE_CLASS(UUIWindow, UObject)
	friend class UUIManager;

public:
	UUIWindow();
	virtual ~UUIWindow() override;

	virtual void Initialize() = 0;

	virtual void Cleanup()
	{
	}

	virtual void OnFocusGained()
	{
	}

	virtual void OnFocusLost()
	{
	}

	virtual bool OnWindowClose() { return true; }
	virtual bool IsSingleton() { return false; }

	virtual void OnSelectedComponentChanged(UActorComponent* Component) {};

	// Getter & Setter
	const FUIWindowConfig& GetConfig() const { return Config; }
	FUIWindowConfig& GetMutableConfig() { return Config; }
	EUIWindowState GetWindowState() const { return CurrentState; }
	const uint32& GetWindowID() const { return WindowID; }
	const FName& GetWindowTitle() const { return Config.WindowTitle; }
	int GetPriority() const { return Config.Priority; }
	float GetLastFocusTime() const { return LastFocusTime; }
	bool IsFocused() const { return bIsFocused; }
	const TArray<UWidget*>& GetWidgets() const { return Widgets; }
	void SetIsResized(bool IsResized) { bIsResized = IsResized; }

	// 윈도우 위치 / 크기 가져오기
	ImVec2 GetLastWindowPosition() const { return LastWindowPosition; }
	ImVec2 GetLastWindowSize() const { return LastWindowSize; }

	void SetLastWindowPosition(const ImVec2& Position)
	{
		LastWindowPosition = Position;
		bShouldRestorePosition = true;
		RestoreFrameCount = 10;
	}

	void SetLastWindowSize(const ImVec2& Size)
	{
		LastWindowSize = Size;
		bShouldRestoreSize = true;
		RestoreFrameCount = 10;
		bForceSize = true;
	}

	bool IsVisible() const
	{
		return CurrentState == EUIWindowState::Visible || CurrentState == EUIWindowState::Maximized;
	}

	void SetWindowState(EUIWindowState NewState)
	{
		CurrentState = NewState;
		// CurrentState와 bIsWindowOpen 동기화
		bIsWindowOpen = (CurrentState == EUIWindowState::Visible || CurrentState == EUIWindowState::Maximized);
	}
	void SetWindowTitle(const FString& NewTitle) { Config.WindowTitle = NewTitle; }
	void SetPriority(int NewPriority) { Config.Priority = NewPriority; }
	void SetConfig(const FUIWindowConfig& InConfig) { Config = InConfig; }
	void ToggleVisibility() { SetWindowState(IsVisible() ? EUIWindowState::Hidden : EUIWindowState::Visible); }
	void AddWidget(UWidget* Widget) { Widgets.Add(Widget); }
	void DeleteWidget(UWidget* Widget);
	void ClearWidget();

	void OnMainWindowResized() const;
	void ClampWindow() const;
	float GetMenuBarOffset() const;

protected:
	void ApplyDockingSettings() const;
	virtual void OnPreRenderWindow(float MenuBarOffset);
	void UpdateWindowInfo();

	// Render
	void RenderWindow();
	void RenderWidget();
	void Update() const;

protected:
	float WindowHeight = 0;
	FUIWindowConfig Config;
private:
	void ProcessDeleteWidgets();
	
	static int32 IssuedWindowID;
	
	EUIWindowState CurrentState;
	uint32 WindowID;

	bool bIsFocused = false;
	float LastFocusTime = 0.0f;

	// ImGui 내부 상태
	bool bIsWindowOpen = false;
	ImVec2 LastWindowSize;
	ImVec2 LastWindowPosition;

	TArray<UWidget*> Widgets;
	TArray<UWidget*> WidgetsToDelete;

	ImVec2 PositionRatio = { 0.5f, 0.5f };
	ImVec2 SizeRatio = { 0.5f, 0.5f };
	bool bIsResized = false;

	// 최소화된 UI 복원 관련
	bool bShouldRestorePosition = false;
	bool bShouldRestoreSize = false;
	int RestoreFrameCount = 0;
	bool bForceSize = false;
};
