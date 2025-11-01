#include "pch.h"
#include "Manager/UI/Public/ViewportManager.h"
#include "Render/UI/Viewport/Public/Window.h"
#include "Core/Public/AppWindow.h"
#include "Render/UI/Viewport/Public/Viewport.h"
#include "Render/UI/Viewport/Public/ViewportClient.h"
#include "Manager/UI/Public/UIManager.h"
#include "Render/UI/Layout/Public/Splitter.h"
#include "Render/UI/Layout/Public/SplitterV.h"
#include "Render/UI/Layout/Public/SplitterH.h"
#include "Render/UI/Window/Public/LevelTabBarWindow.h"
#include "Render/UI/Window/Public/MainMenuWindow.h"
#include "Editor/Public/Editor.h"
#include "Editor/Public/Gizmo.h"
#include "Editor/Public/GizmoTypes.h"
#include "Manager/Input/Public/InputManager.h"
#include "Manager/Config/Public/ConfigManager.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_SINGLETON_CLASS(UViewportManager,UObject)

UViewportManager::UViewportManager() = default;
UViewportManager::~UViewportManager()
{
	// 뷰포트 레이아웃 및 카메라 설정 저장
	SaveViewportLayoutToConfig();
	SaveCameraSettingsToConfig();

	Release();
}

/**
 * @brief 뷰포트 매니저를 초기화하고 레이아웃을 구성합니다.
 * @param InWindow 애플리케이션 윈도우 포인터
 */
void UViewportManager::Initialize(FAppWindow* InWindow)
{
	Release();
	// 밖에서 윈도우를 가져와 크기를 가져온다
	int32 Width = 0, Height = 0;
	if (InWindow)
	{
		InWindow->GetClientSize(Width, Height);
	}
	AppWindow = InWindow;

	// 루트 윈도우에 새로운 윈도우를 할당합니다.
	ViewportLayout = EViewportLayout::Single;

	// 2번 인덱스의 뷰포트로 초기화
	ActiveIndex = 2;
	LastClickedViewportIndex = ActiveIndex; // PIE 진입 시 올바른 뷰포트에서 시작하도록 동기화

	// 뷰포트 슬롯 최대치까진 준비(포인터만). 아직 RT/DSV 없음.
	// 4개의 뷰포트, 클라이언트, 카메라 를 할당받습니다.
	InitializeViewportAndClient();

	// 부모 스플리터
	SplitterV = new SSplitterV();
	SplitterV->Ratio = IniSaveSharedV;

	// 자식 스플리터
	LeftSplitterH = new SSplitterH();
	RightSplitterH = new SSplitterH();

	// 두 수평 스플리터가 동일한 비율 값을 공유하도록 설정합니다.
	LeftSplitterH->SetSharedRatio(&SplitterValueH);
	RightSplitterH->SetSharedRatio(&SplitterValueH);

	// 부모 스플리터의 자식 스플리터를 셋합니다.
	SplitterV->SetChildren(LeftSplitterH, RightSplitterH);

	// 자식 스플리터에 붙을 윈도우를 셋합니다.
	LeftTopWindow		= new SWindow();
	LeftBottomWindow	= new SWindow();
	RightTopWindow		= new SWindow();
	RightBottomWindow	= new SWindow();

	// 자식 스플리터에 자식 윈도우를 셋합니다.
	LeftSplitterH->SetChildren(LeftTopWindow, LeftBottomWindow);
	RightSplitterH->SetChildren(RightTopWindow, RightBottomWindow);

	// 리프 배열로 고정 매핑
	Leaves[0] = LeftTopWindow;
	Leaves[1] = LeftBottomWindow;
	Leaves[2] = RightTopWindow;
	Leaves[3] = RightBottomWindow;

	QuadRoot = SplitterV;

	// 뷰포트 레이아웃 및 카메라 설정 로드 (IniSaveSharedV/H 설정됨)
	LoadViewportLayoutFromConfig();
	LoadCameraSettingsFromConfig();

	// Config에서 로드한 IniSaveSharedV/H를 SplitterValueV/H에 적용
	SplitterValueV = IniSaveSharedV;
	SplitterValueH = IniSaveSharedH;

	// QuadRoot의 스플리터 비율 설정
	if (auto* V = Cast(QuadRoot))
	{
		V->Ratio = SplitterValueV;
	}
	if (auto* RootSplit = Cast(QuadRoot))
	{
		if (auto* HLeft = Cast(RootSplit->SideLT))  HLeft->SetEffectiveRatio(SplitterValueH);
		if (auto* HRight = Cast(RootSplit->SideRB)) HRight->SetEffectiveRatio(SplitterValueH);
	}

	// ViewportLayout에 따라 Root 설정
	if (ViewportLayout == EViewportLayout::Quad)
	{
		Root = QuadRoot;
	}
	else
	{
		Root = Leaves[ActiveIndex];
	}

	Root->OnResize(ActiveViewportRect);
}

/**
 * @brief 매 프레임 뷰포트를 업데이트하고 렌더링을 처리합니다.
 */
void UViewportManager::Update()
{
	if (!Root)
	{
		UE_LOG_WARNING("ViewportManager: Update: Root가 null입니다");
		return;
	}

	// 초기화 상태 확인
	if (Viewports.IsEmpty() || Clients.IsEmpty())
	{
		UE_LOG_ERROR("ViewportManager: Update: Viewports(%d) 또는 Clients(%d)가 비어있습니다", Viewports.Num(), Clients.Num());
		return;
	}

	int32 Width = 0, Height = 0;

	// AppWindow는 실제 윈도우의 메세지콜이나 크기를 담당합니다
	if (AppWindow)
	{
		AppWindow->GetClientSize(Width, Height);
	}

	// 91px height
	const int MenuAndLevelHeight = static_cast<int>(UMainMenuWindow::GetInstance().GetMenuBarHeight()) +
		static_cast<int>(ULevelTabBarWindow::GetInstance().GetLevelBarHeight()) - 12;

	// 하단 StatusBar 높이
	const int StatusBarHeight = static_cast<int>(UUIManager::GetInstance().GetStatusBarHeight());

	// ActiveViewportRect는 실제로 렌더링이 될 영역의 뷰포트 입니다
	const int32 RightPanelWidth = static_cast<int32>(UUIManager::GetInstance().GetRightPanelWidth());
	const int32 ViewportWidth = Width - RightPanelWidth;
	const int32 ViewportHeight = Height - MenuAndLevelHeight - StatusBarHeight;
	ActiveViewportRect = FRect{ 0, MenuAndLevelHeight, max(0, ViewportWidth), max(0, ViewportHeight) };

	auto& InputManager = UInputManager::GetInstance();
	int32 HoveringViewportIdx = GetMouseHoveredViewportIndex();

	// 뷰포트 클릭 감지
	if (InputManager.IsKeyPressed(EKeyInput::MouseLeft) ||
		InputManager.IsKeyPressed(EKeyInput::MouseRight) ||
		InputManager.IsKeyPressed(EKeyInput::MouseMiddle))
	{
		if (HoveringViewportIdx != -1)
		{
			LastClickedViewportIndex = HoveringViewportIdx;
		}
	}

	if (ViewportLayout == EViewportLayout::Quad)
	{
		if (QuadRoot)
		{
			QuadRoot->OnResize(ActiveViewportRect);
		}

		// SWindow 레이아웃의 최종 Rect를 FViewport에 동기화합니다.
		// 이 작업을 통해 3D 렌더링이 올바른 화면 영역에 그려집니다.
		for (int32 i = 0; i < 4; ++i)
		{
			if (Leaves[i] && Viewports[i] && Clients[i])
			{
				Viewports[i]->SetRect(Leaves[i]->GetRect());

				// 각 카메라 종횡비 조정
				const float Aspect = Viewports[i]->GetAspect();
				// FutureEngine: Camera null 체크
				if (UCamera* Cam = Clients[i]->GetCamera())
				{
					Cam->SetAspect(Aspect);
				}
			}
		}

		if (HoveringViewportIdx != -1 && HoveringViewportIdx != ActiveIndex)
		{
			ActiveIndex = HoveringViewportIdx;
		}
	}
	else
	{
		SetRoot(Leaves[ActiveIndex]);

		if (Root)
		{
			Root->OnResize(ActiveViewportRect);
		}

		// FutureEngine: 범위 체크
		if (ActiveIndex < static_cast<int32>(Viewports.Num()) && ActiveIndex < 4)
		{
			if (Viewports[ActiveIndex] && Leaves[ActiveIndex])
			{
				Viewports[ActiveIndex]->SetRect(Leaves[ActiveIndex]->GetRect());
			}
		}
	}

	// 스플리터 드래그 처리 함수
	TickInput();

	// 마우스 휠로 오쏘 뷰 줌 제어
	{
		float WheelDelta = InputManager.GetMouseWheelDelta();

		if (WheelDelta != 0.0f)
		{
			int32 Index = GetMouseHoveredViewportIndex();
			if (Index >= 0 && Index < Clients.Num())
			{
				FViewportClient* Client = Clients[Index];
				if (Client && Client->IsOrtho())
				{
					// 오쏘 뷰: 휠로 줌 제어
					constexpr float ZoomStep = 50.0f;  // OrthoZoom 단위 조정
					constexpr float MinOrthoZoom = 10.0f;
					constexpr float MaxOrthoZoom = 10000.0f;

					float NewOrthoZoom = SharedOrthoZoom - (WheelDelta * ZoomStep);

					// 점진적 제한 (최소값 근처에서 점점 느려지게)
					if (NewOrthoZoom < MinOrthoZoom)
					{
						float DistanceFromMin = SharedOrthoZoom - MinOrthoZoom;
						if (DistanceFromMin > 0.0f)
						{
							float ProgressRatio = min(1.0f, DistanceFromMin / 50.0f);
							SharedOrthoZoom = max(MinOrthoZoom, SharedOrthoZoom - (WheelDelta * ZoomStep * ProgressRatio));
						}
						else
						{
							SharedOrthoZoom = MinOrthoZoom;
						}
					}
					else if (NewOrthoZoom > MaxOrthoZoom)
					{
						SharedOrthoZoom = MaxOrthoZoom;
					}
					else
					{
						SharedOrthoZoom = NewOrthoZoom;
					}

					// 모든 오쏘 뷰에 SharedOrthoZoom 적용
					for (FViewportClient* OrthoClient : Clients)
					{
						if (OrthoClient && OrthoClient->IsOrtho())
						{
							if (UCamera* Cam = OrthoClient->GetCamera())
							{
								Cam->SetOrthoZoom(SharedOrthoZoom);
							}
						}
					}
				}
				else if (Client && !Client->IsOrtho() && InputManager.IsKeyDown(EKeyInput::MouseRight))
				{
					// Perspective 뷰: 우클릭 중 휠로 이동 속도 조절
					constexpr float SpeedStep = 3.0f;
					float NewSpeed = EditorCameraSpeed + (WheelDelta * SpeedStep);
					SetEditorCameraSpeed(NewSpeed);
				}
			}
		}
	}

	// 애니메이션 처리함수
	UpdateViewportAnimation();
}

/**
 * @brief 뷰포트 스플리터 오버레이를 렌더링합니다.
 * @note Quad 모드나 애니메이션 중에만 스플리터를 그립니다.
 */
void UViewportManager::RenderOverlay() const
{
	// FutureEngine 철학: Quad 모드나 애니메이션 중에만 스플리터 렌더링
	if (!(ViewportLayout == EViewportLayout::Quad || ViewportAnimation.bIsAnimating))
	{
		return;
	}

	if (QuadRoot)
	{
		QuadRoot->OnPaint();
	}
}

/**
 * @brief 뷰포트 매니저의 모든 리소스를 해제하고 초기화합니다.
 */
void UViewportManager::Release()
{
	for (size_t Index = 0; Index < Viewports.Num(); ++Index)
	{
		FViewport*& Viewport = Viewports[Index];
		if (!Viewport)
		{
			continue;
		}

		if (Index < Clients.Num())
		{
			FViewportClient*& ClientRef = Clients[Index];
			if (ClientRef)
			{
				ClientRef->SetOwningViewport(nullptr);
			}
			Viewport->SetViewportClient(nullptr);
		}

		SafeDelete(Viewport);
	}
	Viewports.Empty();

	for (FViewportClient*& Client : Clients)
	{
		SafeDelete(Client);
	}
	Clients.Empty();

	InitialOffsets.Empty();
	ActiveRmbViewportIdx = -1;
	ActiveIndex = 0;

	SafeDelete(LeftTopWindow);
	SafeDelete(LeftBottomWindow);
	SafeDelete(RightTopWindow);
	SafeDelete(RightBottomWindow);
	LeftTopWindow = LeftBottomWindow = RightTopWindow = RightBottomWindow = nullptr;

	for (auto& Leaf : Leaves)
	{
		Leaf = nullptr;
	}

	SafeDelete(LeftSplitterH);
	SafeDelete(RightSplitterH);
	SafeDelete(SplitterV);
	LeftSplitterH = RightSplitterH = SplitterV = nullptr;

	Root = nullptr;
	QuadRoot = nullptr;
	Capture = nullptr;

	ActiveViewportRect = {};
	ViewportLayout = EViewportLayout::Single;
	SplitterValueV = 0.5f;
	SplitterValueH = 0.5f;
	IniSaveSharedV = 0.5f;
	IniSaveSharedH = 0.5f;
	ViewportAnimation = {};
	AppWindow = nullptr;
}

/**
 * @brief 마우스 입력을 스플리터/윈도우 트리에 라우팅하여 드래그와 리사이즈를 처리합니다.
 */
void UViewportManager::TickInput()
{
	if (!(ViewportLayout == EViewportLayout::Quad || ViewportAnimation.bIsAnimating))
	{
		return;
	}
	if (!QuadRoot)
	{
		return;
	}

	auto& InputManager = UInputManager::GetInstance();
	const FVector& MousePosition = InputManager.GetMousePosition();
	FPoint Point{ static_cast<LONG>(MousePosition.X), static_cast<LONG>(MousePosition.Y) };

	SWindow* Target;

	if (Capture != nullptr)
	{
		// 드래그 캡처 중이면, 히트 테스트 없이 캡처된 위젯으로 고정
		Target = Capture;
	}
	else
	{
		// 캡처가 아니면 화면 좌표로 히트 테스트
		Target = (QuadRoot != nullptr) ? QuadRoot->HitTest(Point) : nullptr;
	}

	if (InputManager.IsKeyPressed(EKeyInput::MouseLeft) || (!Capture && InputManager.IsKeyDown(EKeyInput::MouseLeft)))
	{
		if (Target && Target->OnMouseDown(Point, 0))
		{
			Capture = Target;
		}
	}

	const FVector& Delta = InputManager.GetMouseDelta();
	if ((Delta.X != 0.0f || Delta.Y != 0.0f) && Capture)
	{
		Capture->OnMouseMove(Point);
	}

	if (InputManager.IsKeyReleased(EKeyInput::MouseLeft))
	{
		if (Capture)
		{
			Capture->OnMouseUp(Point, 0);
			Capture = nullptr;
		}
	}

	if (!InputManager.IsKeyDown(EKeyInput::MouseLeft) && Capture)
	{
		Capture->OnMouseUp(Point, 0);
		Capture = nullptr;
	}
}

/**
 * @brief 현재 레이아웃의 모든 리프 뷰포트의 Rect를 수집합니다.
 * @param OutRects 수집된 Rect 배열을 저장할 출력 파라미터
 */
void UViewportManager::GetLeafRects(TArray<FRect>& OutRects) const
{
	OutRects.Empty();
	if (!Root)
	{
		return;
	}

	// 재귀 수집
	struct Local
	{
		static void Collect(SWindow* Node, TArray<FRect>& Out)
		{
			if (auto* Split = Cast(Node))
			{
				Collect(Split->SideLT, Out);
				Collect(Split->SideRB, Out);
			}
			else
			{
				Out.Emplace(Node->GetRect());
			}
		}
	};
	Local::Collect(Root, OutRects);
}

/**
 * @brief 현재 마우스가 호버링 중인 뷰포트의 인덱스를 반환합니다.
 * @return 호버링 중인 뷰포트 인덱스 (-1이면 호버링하지 않음)
 */
int32 UViewportManager::GetMouseHoveredViewportIndex() const
{
	// 범위 검사
	if (Viewports.IsEmpty())
	{
		return -1;
	}

	const auto& InputManager = UInputManager::GetInstance();

	const FVector& MousePosition = InputManager.GetMousePosition();
	const LONG MousePositionX = static_cast<LONG>(MousePosition.X);
	const LONG MousePositionY = static_cast<LONG>(MousePosition.Y);

	for (int32 i = 0; i < Viewports.Num(); ++i)
	{
		// FutureEngine: null 체크
		if (!Viewports[i])
		{
			UE_LOG_WARNING("ViewportManager: GetViewportIndexUnderMouse: Viewports[%d] is null", i);
			continue;
		}

		const FRect& Rect = Viewports[i]->GetRect();
		const int32 ToolbarHeight = Viewports[i]->GetToolbarHeight();

		const LONG RenderAreaTop = Rect.Top + ToolbarHeight;
		const LONG RenderAreaBottom = Rect.Top + Rect.Height;

		if (MousePositionX >= Rect.Left && MousePositionX < Rect.Left + Rect.Width &&
			MousePositionY >= RenderAreaTop && MousePositionY < RenderAreaBottom)
		{
			return i;
		}
	}
	return -1;
}

/**
 * @brief 특정 뷰포트의 뷰 모드를 가져옵니다.
 * @param Index 뷰포트 인덱스
 * @return 해당 뷰포트의 뷰 모드
 */
EViewModeIndex UViewportManager::GetViewportViewMode(int32 Index) const
{
	// 범위 체크
	if (Index < 0 || Index >= Clients.Num())
	{
		UE_LOG_WARNING("ViewportManager::GetViewportViewMode - Invalid Index: %d", Index);
		return EViewModeIndex::VMI_Gouraud; // 기본값 반환
	}

	// FViewportClient에서 ViewMode 가져오기
	if (Clients[Index])
	{
		return Clients[Index]->GetViewMode();
	}

	UE_LOG_WARNING("ViewportManager::GetViewportViewMode - Clients[%d] is null", Index);
	return EViewModeIndex::VMI_Gouraud; // 기본값 반환
}

/**
 * @brief 특정 뷰포트의 뷰 모드를 설정합니다.
 * @param Index 뷰포트 인덱스
 * @param InMode 설정할 뷰 모드
 */
void UViewportManager::SetViewportViewMode(int32 Index, EViewModeIndex InMode)
{
	// 범위 체크
	if (Index < 0 || Index >= Clients.Num())
	{
		UE_LOG_WARNING("ViewportManager::SetViewportViewMode - Invalid Index: %d", Index);
		return;
	}

	// FViewportClient에 ViewMode 설정
	if (Clients[Index])
	{
		Clients[Index]->SetViewMode(InMode);
		UE_LOG("ViewportManager: Viewport[%d]의 ViewMode를 %d로 변경", Index, static_cast<int32>(InMode));
	}
	else
	{
		UE_LOG_WARNING("ViewportManager::SetViewportViewMode - Clients[%d] is null", Index);
	}
}

void UViewportManager::UpdateInitialOffset(int32 Index, const FVector& NewOffset)
{
	if (Index >= 0 && Index < InitialOffsets.Num())
	{
		InitialOffsets[Index] = NewOffset;
	}
}

/**
 * @brief QuadRoot의 현재 Ratio를 IniSaveSharedV/H에 동기화합니다.
 */
void UViewportManager::UpdateIniSaveSharedRatios()
{
	// 싱글 모드일 때도 QuadRoot의 스플리터 비율을 읽어야
	// 다음 쿼드 전환 시 정상적인 비율로 복원됩니다.

	constexpr float MinValidRatio = 0.1f;
	constexpr float MaxValidRatio = 0.9f;
	constexpr float FallbackRatio = 0.5f;

	// 세로 비율
	if (auto* V = Cast(QuadRoot))
	{
		float ReadV = V->Ratio;
		if (ReadV < MinValidRatio || ReadV > MaxValidRatio)
		{
			ReadV = FallbackRatio;
		}
		IniSaveSharedV = ReadV;
	}

	// 가로 비율
	if (auto* RootSplit = Cast(QuadRoot))
	{
		float ReadH = 0.5f;
		if (auto* HLeft = Cast(RootSplit->SideLT))
		{
			ReadH = HLeft->GetEffectiveRatio();
		}
		else if (auto* HRight = Cast(RootSplit->SideRB))
		{
			ReadH = HRight->GetEffectiveRatio();
		}

		if (ReadH < MinValidRatio || ReadH > MaxValidRatio)
		{
			ReadH = FallbackRatio;
		}
		IniSaveSharedH = ReadH;
	}
}

/**
 * @brief 뷰포트 레이아웃 전환 시 현재 스플리터 비율을 저장합니다.
 */
void UViewportManager::PersistSplitterRatios()
{
	UpdateIniSaveSharedRatios();
}

/**
 * @brief 뷰포트 레이아웃 전환 애니메이션을 시작합니다.
 * @param bSingleToQuad true면 Single->Quad, false면 Quad->Single
 * @param ViewportIndex 승격시킬 뷰포트 인덱스 (-1이면 ActiveIndex 사용)
 */
void UViewportManager::StartLayoutAnimation(bool bSingleToQuad, int32 ViewportIndex)
{
	ViewportAnimation.bSingleToQuad = bSingleToQuad;
	ViewportAnimation.bIsAnimating = true;
	ViewportAnimation.AnimationTime = 0.0f;
	ViewportAnimation.PromotedViewportIndex =
		(ViewportIndex >= 0) ? ViewportIndex : ActiveIndex;

	// 애니메이션 시작 전 QuadRoot의 현재 비율을 읽어서 IniSaveSharedV/H 업데이트
	UpdateIniSaveSharedRatios();

	// 공통: 최소 사이즈 0으로 (끝까지 밀 수 있게)
	if (auto* RootSplit = Cast(QuadRoot))
	{
		ViewportAnimation.SavedMinChildSizeV = RootSplit->MinChildSize;
		RootSplit->MinChildSize = 0;

		if (auto* HLeft = Cast(RootSplit->SideLT)) {
			ViewportAnimation.SavedMinChildSizeH = HLeft->MinChildSize;
			HLeft->MinChildSize = 0;
		}
		if (auto* HRight = Cast(RootSplit->SideRB)) {
			if (auto* HR = Cast(HRight)) HR->MinChildSize = 0;
		}
	}

	if (!bSingleToQuad)
	{
		// ===== Quad -> Single =====
		// 현재 splitter의 실제 비율을 읽어온다 (사용자가 드래그한 현재 위치)
		if (auto* V = Cast(QuadRoot))
		{
			ViewportAnimation.StartVRatio = V->Ratio;
		}
		else
		{
			ViewportAnimation.StartVRatio = SplitterValueV;
		}
		ViewportAnimation.StartHRatio = SplitterValueH; // 공유 비율

		switch (ViewportAnimation.PromotedViewportIndex)
		{
		case 0: ViewportAnimation.TargetVRatio = 1.0f; ViewportAnimation.TargetHRatio = 1.0f; break; // LT
		case 1: ViewportAnimation.TargetVRatio = 1.0f; ViewportAnimation.TargetHRatio = 0.0f; break; // LB
		case 2: ViewportAnimation.TargetVRatio = 0.0f; ViewportAnimation.TargetHRatio = 1.0f; break; // RT
		case 3: ViewportAnimation.TargetVRatio = 0.0f; ViewportAnimation.TargetHRatio = 0.0f; break; // RB
		default: ViewportAnimation.TargetVRatio = SplitterValueV; ViewportAnimation.TargetHRatio = SplitterValueH; break;
		}

		ViewportLayout = EViewportLayout::Quad;
		ViewportAnimation.BackupRoot = Root;
		Root = QuadRoot; // 애니 동안 쿼드
	}
	else
	{
		// ===== Single -> Quad =====
		ActiveIndex = ViewportAnimation.PromotedViewportIndex;

		// 시작값: 현재 싱글이 차지하는 방향으로 핸들을 끝까지 보낸 비율
		switch (ActiveIndex)
		{
		case 0: ViewportAnimation.StartVRatio = 1.0f; ViewportAnimation.StartHRatio = 1.0f; break; // LT
		case 1: ViewportAnimation.StartVRatio = 1.0f; ViewportAnimation.StartHRatio = 0.0f; break; // LB
		case 2: ViewportAnimation.StartVRatio = 0.0f; ViewportAnimation.StartHRatio = 1.0f; break; // RT
		case 3: ViewportAnimation.StartVRatio = 0.0f; ViewportAnimation.StartHRatio = 0.0f; break; // RB
		default: ViewportAnimation.StartVRatio = 0.5f; ViewportAnimation.StartHRatio = 0.5f; break;
		}

		// 목표값: 이전에 저장해 둔 쿼드 배치 비율
		ViewportAnimation.TargetVRatio = std::clamp(IniSaveSharedV, 0.0f, 1.0f);
		ViewportAnimation.TargetHRatio = std::clamp(IniSaveSharedH, 0.0f, 1.0f);

		// 현재 내부 상태도 시작값으로 셋
		SplitterValueV = ViewportAnimation.StartVRatio;
		SplitterValueH = ViewportAnimation.StartHRatio;

		// 루트 백업
		ViewportAnimation.BackupRoot = Root;

		// 실제 스플리터에 시작값을 반영하고 배치 (루트 전환 전에 먼저 수행)
		if (auto* RootSplit = Cast(QuadRoot))
		{
			RootSplit->SetEffectiveRatio(SplitterValueV);
			if (auto* HLeft = Cast(RootSplit->SideLT))  HLeft->SetEffectiveRatio(SplitterValueH);
			if (auto* HRight = Cast(RootSplit->SideRB)) HRight->SetEffectiveRatio(SplitterValueH);
			RootSplit->OnResize(ActiveViewportRect);
		}

		// 뷰포트 동기화 (렌더링 전 정확한 상태 설정)
		SyncRectsToViewports();

		// 애니 시작 시점에 쿼드 레이아웃으로 전환 (렌더링 직전에 수행)
		ViewportLayout = EViewportLayout::Quad;
		Root = QuadRoot;
	}
}

/**
 * @brief SWindow 레이아웃의 Rect를 FViewport에 동기화합니다.
 * @note Quad 모드에서는 4개 모두, Single 모드에서는 활성 뷰포트만 동기화합니다.
 */
void UViewportManager::SyncRectsToViewports() const
{
	// 쿼드일 때 4개 모두, 싱글일 때 Active만
	if (ViewportLayout == EViewportLayout::Quad)
	{
		for (int i = 0; i < 4; ++i)
		{
			if (Leaves[i] && Viewports[i] && Clients[i])
			{
				Viewports[i]->SetRect(Leaves[i]->GetRect());
				const float Aspect = Viewports[i]->GetAspect();
				// FutureEngine: Camera null 체크
				if (UCamera* Cam = Clients[i]->GetCamera())
				{
					Cam->SetAspect(Aspect);
				}
			}
		}
	}
	else
	{
		if (Leaves[ActiveIndex] && Viewports[ActiveIndex] && Clients[ActiveIndex])
		{
			Viewports[ActiveIndex]->SetRect(Leaves[ActiveIndex]->GetRect());
			const float Aspect = Viewports[ActiveIndex]->GetAspect();
			// FutureEngine: Camera null 체크
			if (UCamera* Cam = Clients[ActiveIndex]->GetCamera())
			{
				Cam->SetAspect(Aspect);
			}
		}
	}
}

/**
 * @brief 4개의 뷰포트와 뷰포트 클라이언트를 초기화하고 뷰 타입을 설정합니다.
 */
void UViewportManager::InitializeViewportAndClient()
{
	UE_LOG("ViewportManager: InitializeViewportAndClient 시작");

	for (int i = 0; i < 4; i++)
	{
		FViewport* Viewport = new FViewport();
		FViewportClient* ViewportClient = new FViewportClient();

		// FutureEngine 철학: 양방향 관계 설정
		Viewport->SetViewportClient(ViewportClient);
		ViewportClient->SetOwningViewport(Viewport);

		Clients.Add(ViewportClient);
		Viewports.Add(Viewport);

		UE_LOG("ViewportManager: Viewport[%d] 초기화 완료", i);
	}
	// 언리얼 레퍼런스에 맞게 쿼드뷰 설정
	// 좌상단 (Index 0): Top (Orthographic)
	Clients[0]->SetViewType(EViewType::OrthoTop);
	Clients[0]->SetViewMode(EViewModeIndex::VMI_Wireframe);

	// 좌하단 (Index 1): Front (Orthographic)
	Clients[1]->SetViewType(EViewType::OrthoFront);
	Clients[1]->SetViewMode(EViewModeIndex::VMI_Wireframe);

	// 우상단 (Index 2): Perspective
	Clients[2]->SetViewType(EViewType::Perspective);
	Clients[2]->SetViewMode(EViewModeIndex::VMI_BlinnPhong);

	// 우하단 (Index 3): Right (Orthographic)
	Clients[3]->SetViewType(EViewType::OrthoRight);
	Clients[3]->SetViewMode(EViewModeIndex::VMI_Wireframe);

	// 오쏘 뷰 초기 오프셋 설정 (공유 중심점 + 초기 오프셋 = 각 뷰의 위치)
	InitialOffsets.Add(FVector(0.0f, 0.0f, 100.0f));   // Top
	InitialOffsets.Add(FVector(0.0f, 0.0f, -100.0f));  // Bottom
	InitialOffsets.Add(FVector(0.0f, 100.0f, 0.0f));   // Left
	InitialOffsets.Add(FVector(0.0f, -100.0f, 0.0f));  // Right
	InitialOffsets.Add(FVector(-100.0f, 0.0f, 0.0f));  // Front
	InitialOffsets.Add(FVector(100.0f, 0.0f, 0.0f));   // Back

	UE_LOG("ViewportManager: 총 %d개 Viewport, %d개 Client 생성", Viewports.Num(), Clients.Num());
}

/**
 * @brief 뷰포트 레이아웃을 설정합니다.
 * @param InViewportLayout 설정할 레이아웃 (Single 또는 Quad)
 */
void UViewportManager::SetViewportLayout(EViewportLayout InViewportLayout)
{
	ViewportLayout = InViewportLayout;
}

/**
 * @brief 레이아웃 전환 애니메이션을 매 프레임 업데이트합니다.
 */
void UViewportManager::UpdateViewportAnimation()
{
	if (!ViewportAnimation.bIsAnimating) return;

	ViewportAnimation.AnimationTime += DT;

	float t = ViewportAnimation.AnimationTime / ViewportAnimation.AnimationDuration;
	float k = EaseInOutCubic(std::clamp(t, 0.0f, 1.0f));

	// 보간
	SplitterValueV = Lerp(ViewportAnimation.StartVRatio, ViewportAnimation.TargetVRatio, k);
	SplitterValueH = Lerp(ViewportAnimation.StartHRatio, ViewportAnimation.TargetHRatio, k);

	if (auto* RootSplit = Cast(QuadRoot))
	{
		RootSplit->SetEffectiveRatio(SplitterValueV);
		if (auto* HLeft = Cast(RootSplit->SideLT))  HLeft->SetEffectiveRatio(SplitterValueH);
		if (auto* HRight = Cast(RootSplit->SideRB)) HRight->SetEffectiveRatio(SplitterValueH);
		RootSplit->OnResize(ActiveViewportRect);
	}

	SyncRectsToViewports();

	if (t >= 1.0f)
	{
		if (ViewportAnimation.bSingleToQuad)
			FinalizeFourSplitLayoutFromAnimation();
		else
			FinalizeSingleLayoutFromAnimation();
	}
}

/**
 * @brief Cubic ease-in-out 보간 함수입니다.
 * @param InT 입력 시간 (0~1 범위)
 * @return 보간된 값 (0~1 범위)
 */
float UViewportManager::EaseInOutCubic(float InT)
{
	// 0~1 범위 입력 보장
	InT = std::clamp(InT, 0.0f, 1.0f);
	return (InT < 0.5f) ? (4.0f * InT * InT * InT) : (1.0f - std::pow(-2.0f * InT + 2.0f, 3.0f) / 2.0f);
}

/**
 * @brief Quad->Single 애니메이션 완료 후 최종 상태를 설정합니다.
 */
void UViewportManager::FinalizeSingleLayoutFromAnimation()
{
	ViewportAnimation.bIsAnimating = false;

	// (1) 드래그 캡처 중이면 해제
	Capture = nullptr;

	// (2) MinChildSize 원복
	if (auto* RootSplit = Cast(QuadRoot))
	{
		RootSplit->MinChildSize = ViewportAnimation.SavedMinChildSizeV;
		if (auto* HLeft = Cast(RootSplit->SideLT)) HLeft->MinChildSize = ViewportAnimation.SavedMinChildSizeH;
		if (auto* HRight = Cast(RootSplit->SideRB)) HRight->MinChildSize = ViewportAnimation.SavedMinChildSizeH;
	}

	// (3) 싱글 모드 전환
	ActiveIndex = ViewportAnimation.PromotedViewportIndex;
	ViewportLayout = EViewportLayout::Single;

	// (4) 싱글 루트 지정 + 전체 크기로 리사이즈
	Root = Leaves[ActiveIndex];
	if (Root)
		Root->OnResize(ActiveViewportRect);

	// (5) QuadRoot의 비율을 정상 범위로 복원
	if (auto* RootSplit = Cast(QuadRoot))
	{
		RootSplit->SetEffectiveRatio(IniSaveSharedV);
		if (auto* HLeft = Cast(RootSplit->SideLT))
		{
			HLeft->SetEffectiveRatio(IniSaveSharedH);
		}
		if (auto* HRight = Cast(RootSplit->SideRB))
		{
			HRight->SetEffectiveRatio(IniSaveSharedH);
		}
	}

	// (6) 뷰포트 동기화 (싱글만 활성)
	SyncRectsToViewports();
}

/**
 * @brief Single->Quad 애니메이션 완료 후 최종 상태를 설정합니다.
 */
void UViewportManager::FinalizeFourSplitLayoutFromAnimation()
{
	ViewportAnimation.bIsAnimating = false;
	Capture = nullptr;

	// 최소 사이즈 원복
	if (auto* RootSplit = Cast(QuadRoot))
	{
		RootSplit->MinChildSize = ViewportAnimation.SavedMinChildSizeV;
		if (auto* HLeft = Cast(RootSplit->SideLT))  HLeft->MinChildSize = ViewportAnimation.SavedMinChildSizeH;
		if (auto* HRight = Cast(RootSplit->SideRB)) HRight->MinChildSize = ViewportAnimation.SavedMinChildSizeH;
	}

	// 최종 비율 고정
	SplitterValueV = ViewportAnimation.TargetVRatio;
	SplitterValueH = ViewportAnimation.TargetHRatio;

	// 쿼드 레이아웃 확정
	ViewportLayout = EViewportLayout::Quad;
	Root = QuadRoot;

	// 한 번 더 적용/배치
	if (auto* RootSplit = Cast(QuadRoot))
	{
		RootSplit->SetEffectiveRatio(SplitterValueV);
		if (auto* HLeft = Cast(RootSplit->SideLT))  HLeft->SetEffectiveRatio(SplitterValueH);
		if (auto* HRight = Cast(RootSplit->SideRB)) HRight->SetEffectiveRatio(SplitterValueH);
		RootSplit->OnResize(ActiveViewportRect);
	}

	SyncRectsToViewports();

	// 다음 전환 대비 저장(선택)
	IniSaveSharedV = SplitterValueV;
	IniSaveSharedH = SplitterValueH;
}

/**
 * @brief 뷰포트 설정을 JSON으로 직렬화 또는 역직렬화합니다.
 * @param bInIsLoading true면 로드, false면 저장
 * @param InOutHandle JSON 핸들
 */
void UViewportManager::SerializeViewports(const bool bInIsLoading, JSON& InOutHandle)
{
	// 저장/로드 모두에서 쓸 공통 키
	const char* ViewportSystemKey = "ViewportSystem";

	if (bInIsLoading)
	{
		JSON ViewportSystemJson;
		if (!FJsonSerializer::ReadObject(InOutHandle, ViewportSystemKey, ViewportSystemJson))
		{
			return; // 저장된 뷰포트 정보가 없으면 무시
		}

		// 뷰포트 배열 (ViewType, ViewMode만 로드)
		JSON ViewportsArray;
		if (FJsonSerializer::ReadArray(ViewportSystemJson, "Viewports", ViewportsArray))
		{
			// 최대 4개 기준
			const int32 SavedCount = ViewportsArray.size();
			const int32 ClientCount = Clients.Num();
			const int32 RestoreCount = std::min(SavedCount, ClientCount);

			for (int32 Index = 0; Index < RestoreCount; ++Index)
			{
				JSON& ViewportJson = ViewportsArray[Index];

				// View type
				int32 ViewTypeInt = 0;
				FJsonSerializer::ReadInt32(ViewportJson, "ViewType", ViewTypeInt, 0);

				if (FViewportClient* Client = Clients[Index])
				{
					Client->SetViewType(static_cast<EViewType>(ViewTypeInt));

					// ViewMode(있으면)
					int32 ViewModeInt = static_cast<int32>(Client->GetViewMode());
					FJsonSerializer::ReadInt32(ViewportJson, "ViewMode", ViewModeInt, ViewModeInt);
					Client->SetViewMode(static_cast<EViewModeIndex>(ViewModeInt));
				}
			}
		}

		// 카메라 정보 로드
		JSON CameraDataJson;
		if (FJsonSerializer::ReadObject(InOutHandle, "PerspectiveCamera", CameraDataJson))
		{
			const int32 ClientCount = static_cast<int32>(Clients.Num());
			for (int32 Index = 0; Index < ClientCount; ++Index)
			{
				FViewportClient* Client = Clients[Index];
				if (!Client)
				{
					continue;
				}

				UCamera* Cam = Client->GetCamera();
				if (!Cam)
				{
					continue;
				}

				// 인덱스 문자열로 접근
				FString IndexStr = std::to_string(Index);
				JSON CamJson;
				if (!FJsonSerializer::ReadObject(CameraDataJson, IndexStr.data(), CamJson))
				{
					// 해당 인덱스의 카메라 정보가 없으면 기본값 유지
					continue;
				}

				// CameraType
				int32 CameraType = 0;
				FJsonSerializer::ReadInt32(CamJson, "CameraType", CameraType, 0);

				// Location
				FVector Location;
				if (!FJsonSerializer::ReadVector(CamJson, "Location", Location))
				{
					// 적절하지 않으면 기본값 유지
					continue;
				}

				// Rotation
				FVector Rotation;
				if (!FJsonSerializer::ReadVector(CamJson, "Rotation", Rotation))
				{
					// 적절하지 않으면 기본값 유지
					continue;
				}

				// NaN/Inf 체크
				if (!std::isfinite(Location.X) || !std::isfinite(Location.Y) || !std::isfinite(Location.Z) ||
					!std::isfinite(Rotation.X) || !std::isfinite(Rotation.Y) || !std::isfinite(Rotation.Z))
				{
					// 적절하지 않으면 기본값 유지
					continue;
				}

				// FovY, NearClip, FarClip
				float FovY = 90.0f;
				float NearClip = 0.1f;
				float FarClip = 1000.0f;
				FJsonSerializer::ReadFloat(CamJson, "FovY", FovY, 90.0f);
				FJsonSerializer::ReadFloat(CamJson, "NearClip", NearClip, 0.1f);
				FJsonSerializer::ReadFloat(CamJson, "FarClip", FarClip, 1000.0f);

				// FocusLocation (optional)
				FVector FocusLocation(0, 0, 0);
				FJsonSerializer::ReadVector(CamJson, "FocusLocation", FocusLocation);

				// 카메라에 적용
				Cam->SetLocation(Location);
				Cam->SetRotation(Rotation);
				Cam->SetFovY(FovY);
				Cam->SetNearZ(NearClip);
				Cam->SetFarZ(FarClip);
			}
		}

		// 로드 후 모든 Ortho 카메라에 SharedOrthoZoom 적용 (OrthoWidth 무시)
		for (FViewportClient* Client : Clients)
		{
			if (Client && Client->IsOrtho())
			{
				if (UCamera* Cam = Client->GetCamera())
				{
					Cam->SetOrthoZoom(SharedOrthoZoom);
				}
			}
		}
	}
	else
	{
		JSON ViewportSystemJson = json::Object();

		// 각 뷰포트 상태 저장 (ViewType, ViewMode만 저장)
		JSON ViewportsArray = json::Array();
		const int32 ClientCount = Clients.Num();
		for (int32 Index = 0; Index < ClientCount; ++Index)
		{
			FViewportClient* Client = Clients[Index];
			if (!Client)
			{
				continue;
			}

			JSON ViewportJson = json::Object();
			ViewportJson["Index"] = Index;
			ViewportJson["ViewType"] = static_cast<int32>(Client->GetViewType());
			ViewportJson["ViewMode"] = static_cast<int32>(Client->GetViewMode());

			ViewportsArray.append(ViewportJson);
		}

		ViewportSystemJson["Viewports"] = ViewportsArray;

		InOutHandle[ViewportSystemKey] = ViewportSystemJson;

		// 3) 카메라 정보 저장
		JSON CameraDataJson = json::Object();
		for (int32 Index = 0; Index < ClientCount; ++Index)
		{
			FViewportClient* Client = Clients[Index];
			if (!Client)
			{
				continue;
			}

			UCamera* Cam = Client->GetCamera();
			if (!Cam)
			{
				continue;
			}

			JSON CamJson = json::Object();
			CamJson["CameraType"] = 0; // 현재는 모두 Perspective로 저장
			CamJson["Location"] = FJsonSerializer::VectorToJson(Cam->GetLocation());
			CamJson["Rotation"] = FJsonSerializer::VectorToJson(Cam->GetRotation());
			CamJson["FovY"] = Cam->GetFovY();
			CamJson["NearClip"] = Cam->GetNearZ();
			CamJson["FarClip"] = Cam->GetFarZ();
			CamJson["OrthoWidth"] = 90.0f; // 호환성을 위해 고정값
			CamJson["FocusLocation"] = FJsonSerializer::VectorToJson(FVector(0, 0, 0)); // 현재 미사용

			// 인덱스 문자열을 키로 사용
			FString IndexStr = std::to_string(Index);
			CameraDataJson[IndexStr.data()] = CamJson;
		}

		InOutHandle["PerspectiveCamera"] = CameraDataJson;
	}
}

/**
 * @brief 뷰포트 레이아웃 설정을 Config 파일에 저장합니다.
 */
void UViewportManager::SaveViewportLayoutToConfig()
{
	// 저장 시점에 현재 스플리터 비율을 IniSaveSharedV/H에 동기화
	UpdateIniSaveSharedRatios();

	// 극단값 검증: 싱글 모드에서 저장 시 애니메이션 값이 극단값일 수 있으므로 폴백
	constexpr float MinValidRatio = 0.1f;
	constexpr float MaxValidRatio = 0.9f;
	constexpr float FallbackRatio = 0.5f;

	float SavedV = IniSaveSharedV;
	float SavedH = IniSaveSharedH;

	if (SavedV < MinValidRatio || SavedV > MaxValidRatio)
	{
		SavedV = FallbackRatio;
	}
	if (SavedH < MinValidRatio || SavedH > MaxValidRatio)
	{
		SavedH = FallbackRatio;
	}

	// ConfigManager에 현재 값 동기화
	// 주의: Gizmo는 이미 SetGizmoMode 호출 시 자동 동기화되므로 여기서 접근하지 않음
	auto& ConfigManager = UConfigManager::GetInstance();
	ConfigManager.SetCachedSharedOrthoZoom(SharedOrthoZoom);
	ConfigManager.SetCachedEditorCameraSpeed(EditorCameraSpeed);
	ConfigManager.SetCachedRotationSnapEnabled(bRotationSnapEnabled);
	ConfigManager.SetCachedRotationSnapAngle(RotationSnapAngle);

	JSON LayoutJson = json::Object();

	// 레이아웃 / 활성뷰 / 스플리터 비율 저장
	LayoutJson["Layout"] = static_cast<int32>(GetViewportLayout());
	LayoutJson["ActiveIndex"] = GetActiveIndex();
	LayoutJson["SplitterV"] = SavedV;
	LayoutJson["SplitterH"] = SavedH;
	LayoutJson["SharedOrthoZoom"] = ConfigManager.GetCachedSharedOrthoZoom();
	LayoutJson["RotationSnapEnabled"] = ConfigManager.GetCachedRotationSnapEnabled() ? 1 : 0;
	LayoutJson["RotationSnapAngle"] = ConfigManager.GetCachedRotationSnapAngle();
	LayoutJson["EditorCameraSpeed"] = ConfigManager.GetCachedEditorCameraSpeed();
	LayoutJson["GizmoMode"] = static_cast<int32>(ConfigManager.GetCachedGizmoMode());

	// 각 뷰포트의 ViewType, ViewMode 저장
	JSON ViewportsArray = json::Array();
	const int32 ClientCount = Clients.Num();
	for (int32 Index = 0; Index < ClientCount; ++Index)
	{
		FViewportClient* Client = Clients[Index];
		if (!Client)
		{
			continue;
		}

		JSON ViewportJson = json::Object();
		ViewportJson["Index"] = Index;
		ViewportJson["ViewType"] = static_cast<int32>(Client->GetViewType());
		ViewportJson["ViewMode"] = static_cast<int32>(Client->GetViewMode());

		ViewportsArray.append(ViewportJson);
	}

	LayoutJson["Viewports"] = ViewportsArray;

	// ConfigManager에 저장
	UConfigManager::GetInstance().SaveViewportLayoutSettings(LayoutJson);
}

/**
 * @brief Config 파일에서 뷰포트 레이아웃 설정을 로드합니다.
 */
void UViewportManager::LoadViewportLayoutFromConfig()
{
	// ConfigManager에서 레이아웃 설정 로드
	JSON LayoutJson = UConfigManager::GetInstance().LoadViewportLayoutSettings();

	if (LayoutJson.IsNull())
	{
		return; // 저장된 레이아웃 설정이 없으면 무시
	}

	// 레이아웃/활성뷰/스플리터 비율
	int32 LayoutInt = 0;
	int32 LoadedActiveIndex = 2;
	float LoadedSplitterV = 0.5f;
	float LoadedSplitterH = 0.5f;
	float LoadedSharedOrthoZoom = 500.0f;

	FJsonSerializer::ReadInt32(LayoutJson, "Layout", LayoutInt, 0);
	FJsonSerializer::ReadInt32(LayoutJson, "ActiveIndex", LoadedActiveIndex, 2);
	FJsonSerializer::ReadFloat(LayoutJson, "SplitterV", LoadedSplitterV, 0.5f);
	FJsonSerializer::ReadFloat(LayoutJson, "SplitterH", LoadedSplitterH, 0.5f);
	FJsonSerializer::ReadFloat(LayoutJson, "SharedOrthoZoom", LoadedSharedOrthoZoom, 500.0f);

	// Rotation Snap Settings
	int32 LoadedRotationSnapEnabledInt = 1;
	float LoadedRotationSnapAngle = DEFAULT_ROTATION_SNAP_ANGLE;
	FJsonSerializer::ReadInt32(LayoutJson, "RotationSnapEnabled", LoadedRotationSnapEnabledInt, 1);
	FJsonSerializer::ReadFloat(LayoutJson, "RotationSnapAngle", LoadedRotationSnapAngle, DEFAULT_ROTATION_SNAP_ANGLE);
	bool LoadedRotationSnapEnabled = (LoadedRotationSnapEnabledInt != 0);

	// Editor Camera Speed
	float LoadedEditorCameraSpeed = DEFAULT_CAMERA_SPEED;
	FJsonSerializer::ReadFloat(LayoutJson, "EditorCameraSpeed", LoadedEditorCameraSpeed, DEFAULT_CAMERA_SPEED);

	SharedOrthoZoom = LoadedSharedOrthoZoom;
	bRotationSnapEnabled = LoadedRotationSnapEnabled;
	RotationSnapAngle = LoadedRotationSnapAngle;
	EditorCameraSpeed = LoadedEditorCameraSpeed;

	// Gizmo 설정 로드
	int32 LoadedGizmoModeInt = 0;
	FJsonSerializer::ReadInt32(LayoutJson, "GizmoMode", LoadedGizmoModeInt, 0);

	if (GEditor)
	{
		if (UEditor* Editor = GEditor->GetEditorModule())
		{
			if (UGizmo* Gizmo = Editor->GetGizmo())
			{
				Gizmo->SetGizmoMode(static_cast<EGizmoMode>(LoadedGizmoModeInt));
			}
		}
	}

	SetViewportLayout(static_cast<EViewportLayout>(LayoutInt));
	SetActiveIndex(LoadedActiveIndex);

	// 극단값 검증: 로드한 값이 극단값이면 폴백
	constexpr float MinValidRatio = 0.1f;
	constexpr float MaxValidRatio = 0.9f;
	constexpr float FallbackRatio = 0.5f;

	if (LoadedSplitterV < MinValidRatio || LoadedSplitterV > MaxValidRatio)
	{
		LoadedSplitterV = FallbackRatio;
	}
	if (LoadedSplitterH < MinValidRatio || LoadedSplitterH > MaxValidRatio)
	{
		LoadedSplitterH = FallbackRatio;
	}

	IniSaveSharedV = LoadedSplitterV;
	IniSaveSharedH = LoadedSplitterH;

	// 뷰포트 배열
	JSON ViewportsArray;
	if (FJsonSerializer::ReadArray(LayoutJson, "Viewports", ViewportsArray))
	{
		const int32 SavedCount = ViewportsArray.size();
		const int32 ClientCount = Clients.Num();
		const int32 RestoreCount = std::min(SavedCount, ClientCount);

		for (int32 Index = 0; Index < RestoreCount; ++Index)
		{
			JSON& ViewportJson = ViewportsArray[Index];

			// View type
			int32 ViewTypeInt = 0;
			FJsonSerializer::ReadInt32(ViewportJson, "ViewType", ViewTypeInt, 0);

			if (FViewportClient* Client = Clients[Index])
			{
				Client->SetViewType(static_cast<EViewType>(ViewTypeInt));

				// ViewMode
				int32 ViewModeInt = static_cast<int32>(Client->GetViewMode());
				FJsonSerializer::ReadInt32(ViewportJson, "ViewMode", ViewModeInt, ViewModeInt);
				Client->SetViewMode(static_cast<EViewModeIndex>(ViewModeInt));
			}
		}
	}

	// 로드 후 모든 Ortho 카메라에 SharedOrthoZoom 적용
	for (FViewportClient* Client : Clients)
	{
		if (Client && Client->IsOrtho())
		{
			if (UCamera* Cam = Client->GetCamera())
			{
				Cam->SetOrthoZoom(SharedOrthoZoom);
			}
		}
	}
}

/**
 * @brief 모든 뷰포트의 카메라 설정을 Config 파일에 저장합니다.
 */
void UViewportManager::SaveCameraSettingsToConfig()
{
	JSON ViewportCameraJson = json::Object();

	// 각 뷰포트의 카메라 설정을 저장
	JSON CamerasArray = json::Array();
	const int32 ClientCount = Clients.Num();

	for (int32 Index = 0; Index < ClientCount; ++Index)
	{
		FViewportClient* Client = Clients[Index];
		if (!Client)
		{
			continue;
		}

		UCamera* Camera = Client->GetCamera();
		if (!Camera)
		{
			continue;
		}

		JSON CameraJson = json::Object();
		CameraJson["Index"] = Index;
		CameraJson["CameraType"] = static_cast<int32>(Camera->GetCameraType());

		// Location, Rotation
		const FVector& Location = Camera->GetLocation();
		const FVector& Rotation = Camera->GetRotation();
		CameraJson["Location"] = FJsonSerializer::VectorToJson(Location);
		CameraJson["Rotation"] = FJsonSerializer::VectorToJson(Rotation);

		// Camera parameters
		CameraJson["FovY"] = Camera->GetFovY();
		CameraJson["NearZ"] = Camera->GetNearZ();
		CameraJson["FarZ"] = Camera->GetFarZ();
		CameraJson["OrthoZoom"] = Camera->GetOrthoZoom();
		CameraJson["MoveSpeed"] = Camera->GetMoveSpeed();

		CamerasArray.append(CameraJson);
	}

	ViewportCameraJson["Cameras"] = CamerasArray;
	ViewportCameraJson["EditorCameraSpeed"] = EditorCameraSpeed;

	// ConfigManager에 저장
	UConfigManager::GetInstance().SaveViewportCameraSettings(ViewportCameraJson);
}

/**
 * @brief Config 파일에서 모든 뷰포트의 카메라 설정을 로드합니다.
 */
void UViewportManager::LoadCameraSettingsFromConfig()
{
	// ConfigManager에서 카메라 설정 로드
	JSON ViewportCameraJson = UConfigManager::GetInstance().LoadViewportCameraSettings();

	if (ViewportCameraJson.IsNull())
	{
		return; // 저장된 카메라 설정이 없으면 무시
	}

	// EditorCameraSpeed 로드
	float LoadedEditorCameraSpeed = EditorCameraSpeed;
	if (FJsonSerializer::ReadFloat(ViewportCameraJson, "EditorCameraSpeed", LoadedEditorCameraSpeed, EditorCameraSpeed))
	{
		SetEditorCameraSpeed(LoadedEditorCameraSpeed);
	}

	if (!ViewportCameraJson.hasKey("Cameras"))
	{
		return; // 카메라 배열이 없으면 종료
	}

	JSON CamerasArray = ViewportCameraJson["Cameras"];
	if (CamerasArray.JSONType() != JSON::Class::Array)
	{
		return;
	}

	const int32 SavedCount = CamerasArray.size();
	const int32 ClientCount = Clients.Num();

	for (int32 SavedIndex = 0; SavedIndex < SavedCount; ++SavedIndex)
	{
		JSON& CameraJson = CamerasArray[SavedIndex];

		int32 Index = 0;
		FJsonSerializer::ReadInt32(CameraJson, "Index", Index, 0);

		if (Index < 0 || Index >= ClientCount)
		{
			continue;
		}

		FViewportClient* Client = Clients[Index];
		if (!Client)
		{
			continue;
		}

		UCamera* Camera = Client->GetCamera();
		if (!Camera)
		{
			continue;
		}

		// CameraType
		int32 CameraTypeInt = static_cast<int32>(Camera->GetCameraType());
		FJsonSerializer::ReadInt32(CameraJson, "CameraType", CameraTypeInt, CameraTypeInt);
		Camera->SetCameraType(static_cast<ECameraType>(CameraTypeInt));

		// Location, Rotation
		FVector SavedLocation, SavedRotation;
		FJsonSerializer::ReadVector(CameraJson, "Location", SavedLocation, Camera->GetLocation());
		FJsonSerializer::ReadVector(CameraJson, "Rotation", SavedRotation, Camera->GetRotation());
		Camera->SetLocation(SavedLocation);
		Camera->SetRotation(SavedRotation);

		// Camera parameters
		float SavedFovY = Camera->GetFovY();
		float SavedNearZ = Camera->GetNearZ();
		float SavedFarZ = Camera->GetFarZ();
		float SavedOrthoZoom = Camera->GetOrthoZoom();
		float SavedMoveSpeed = Camera->GetMoveSpeed();

		FJsonSerializer::ReadFloat(CameraJson, "FovY", SavedFovY, SavedFovY);
		FJsonSerializer::ReadFloat(CameraJson, "NearZ", SavedNearZ, SavedNearZ);
		FJsonSerializer::ReadFloat(CameraJson, "FarZ", SavedFarZ, SavedFarZ);
		FJsonSerializer::ReadFloat(CameraJson, "OrthoZoom", SavedOrthoZoom, SavedOrthoZoom);
		FJsonSerializer::ReadFloat(CameraJson, "MoveSpeed", SavedMoveSpeed, SavedMoveSpeed);

		Camera->SetFovY(SavedFovY);
		Camera->SetNearZ(SavedNearZ);
		Camera->SetFarZ(SavedFarZ);
		Camera->SetOrthoZoom(SavedOrthoZoom);
		Camera->SetMoveSpeed(SavedMoveSpeed);
	}
}

/**
 * @brief 에디터 카메라의 이동 속도를 설정합니다.
 * @param InSpeed 설정할 카메라 속도 (MIN_CAMERA_SPEED ~ MAX_CAMERA_SPEED 범위로 클램프됨)
 */
void UViewportManager::SetEditorCameraSpeed(float InSpeed)
{
	EditorCameraSpeed = clamp(InSpeed, MIN_CAMERA_SPEED, MAX_CAMERA_SPEED);

	// ConfigManager에 동기화
	UConfigManager::GetInstance().SetCachedEditorCameraSpeed(EditorCameraSpeed);

	// 모든 ViewportClient의 카메라에 전역 스피드 동기화
	for (FViewportClient* Client : Clients)
	{
		if (Client && Client->GetCamera())
		{
			Client->GetCamera()->SetMoveSpeed(EditorCameraSpeed);
		}
	}
}

/**
 * @brief 현재 드래그 중인 스플리터가 있는지 확인합니다.
 * @return 드래그 중인 스플리터가 있으면 true, 없으면 false
 */
bool UViewportManager::IsAnySplitterDragging() const
{
	// Recursively check if any splitter is currently being dragged
	TFunction<bool(SWindow*)> CheckSplitter = [&](SWindow* Window) -> bool
	{
		if (!Window) return false;

		// Check if this window is a splitter and is dragging
		if (SSplitter* Splitter = Window->AsSplitter())
		{
			if (Splitter->IsDragging())
			{
				return true;
			}

			// Recursively check children
			if (CheckSplitter(Splitter->SideLT))
			{
				return true;
			}
			if (CheckSplitter(Splitter->SideRB))
			{
				return true;
			}
		}

		return false;
	};

	return CheckSplitter(Root) || CheckSplitter(QuadRoot);
}
