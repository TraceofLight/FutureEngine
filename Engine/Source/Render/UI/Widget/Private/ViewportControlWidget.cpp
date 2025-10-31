#include "pch.h"
#include "Render/UI/Widget/Public/ViewportControlWidget.h"
#include "Manager/UI/Public/ViewportManager.h"
#include "Render/UI/Viewport/Public/Viewport.h"
#include "Render/UI/Viewport/Public/ViewportClient.h"
#include "Editor/Public/Editor.h"
#include "Editor/Public/EditorEngine.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Manager/Path/Public/PathManager.h"
#include "Texture/Public/Texture.h"
#include "Actor/Public/Actor.h"

// 정적 멤버 정의 - KTLWeek07의 뷰 모드 기능 사용
const char* UViewportControlWidget::ViewModeLabels[] = {
	"Gouraud", "Lambert", "BlinnPhong", "Unlit", "Wireframe", "SceneDepth", "WorldNormal"
};

const char* UViewportControlWidget::ViewTypeLabels[] = {
	"Perspective", "Top", "Bottom", "Left", "Right", "Front", "Back"
};

UViewportControlWidget::UViewportControlWidget()
	: UWidget("Viewport Control Widget")
{
}

UViewportControlWidget::~UViewportControlWidget() = default;

void UViewportControlWidget::Initialize()
{
	LoadViewIcons();
	UE_LOG("ViewportControlWidget: Initialized");
}

void UViewportControlWidget::LoadViewIcons()
{
	if (bIconsLoaded) return;

	UE_LOG("ViewportControlWidget: 아이콘 로드 시작...");
	UAssetManager& AssetManager = UAssetManager::GetInstance();
	UPathManager& PathManager = UPathManager::GetInstance();
	FString IconBasePath = PathManager.GetAssetPath().string() + "\\Icon\\";

	int32 LoadedCount = 0;

	IconPerspective = AssetManager.LoadTexture(IconBasePath + "ViewPerspective.png");
	if (IconPerspective) {
		UE_LOG("ViewportControlWidget: 아이콘 로드 성공: 'ViewPerspective' -> %p", IconPerspective);
		LoadedCount++;
	} else {
		UE_LOG_WARNING("ViewportControlWidget: 아이콘 로드 실패: %s", (IconBasePath + "ViewPerspective.png").c_str());
	}

	IconTop = AssetManager.LoadTexture(IconBasePath + "ViewTop.png");
	if (IconTop) {
		UE_LOG("ViewportControlWidget: 아이콘 로드 성공: 'ViewTop' -> %p", IconTop);
		LoadedCount++;
	} else {
		UE_LOG_WARNING("ViewportControlWidget: 아이콘 로드 실패: %s", (IconBasePath + "ViewTop.png").c_str());
	}

	IconBottom = AssetManager.LoadTexture(IconBasePath + "ViewBottom.png");
	if (IconBottom) {
		UE_LOG("ViewportControlWidget: 아이콘 로드 성공: 'ViewBottom' -> %p", IconBottom);
		LoadedCount++;
	} else {
		UE_LOG_WARNING("ViewportControlWidget: 아이콘 로드 실패: %s", (IconBasePath + "ViewBottom.png").c_str());
	}

	IconLeft = AssetManager.LoadTexture(IconBasePath + "ViewLeft.png");
	if (IconLeft) {
		UE_LOG("ViewportControlWidget: 아이콘 로드 성공: 'ViewLeft' -> %p", IconLeft);
		LoadedCount++;
	} else {
		UE_LOG_WARNING("ViewportControlWidget: 아이콘 로드 실패: %s", (IconBasePath + "ViewLeft.png").c_str());
	}

	IconRight = AssetManager.LoadTexture(IconBasePath + "ViewRight.png");
	if (IconRight) {
		UE_LOG("ViewportControlWidget: 아이콘 로드 성공: 'ViewRight' -> %p", IconRight);
		LoadedCount++;
	} else {
		UE_LOG_WARNING("ViewportControlWidget: 아이콘 로드 실패: %s", (IconBasePath + "ViewRight.png").c_str());
	}

	IconFront = AssetManager.LoadTexture(IconBasePath + "ViewFront.png");
	if (IconFront) {
		UE_LOG("ViewportControlWidget: 아이콘 로드 성공: 'ViewFront' -> %p", IconFront);
		LoadedCount++;
	} else {
		UE_LOG_WARNING("ViewportControlWidget: 아이콘 로드 실패: %s", (IconBasePath + "ViewFront.png").c_str());
	}

	IconBack = AssetManager.LoadTexture(IconBasePath + "ViewBack.png");
	if (IconBack) {
		UE_LOG("ViewportControlWidget: 아이콘 로드 성공: 'ViewBack' -> %p", IconBack);
		LoadedCount++;
	} else {
		UE_LOG_WARNING("ViewportControlWidget: 아이콘 로드 실패: %s", (IconBasePath + "ViewBack.png").c_str());
	}

	// ViewMode 아이콘 로드
	IconLitCube = AssetManager.LoadTexture(IconBasePath + "LitCube.png");
	if (IconLitCube) {
		UE_LOG("ViewportControlWidget: 아이콘 로드 성공: 'LitCube' -> %p", IconLitCube);
		LoadedCount++;
	} else {
		UE_LOG_WARNING("ViewportControlWidget: 아이콘 로드 실패: %s", (IconBasePath + "LitCube.png").c_str());
	}

	// 레이아웃 전환 아이콘 로드
	IconQuad = AssetManager.LoadTexture(IconBasePath + "quad.png");
	if (IconQuad) {
		UE_LOG("ViewportControlWidget: 아이콘 로드 성공: 'quad' -> %p", IconQuad);
		LoadedCount++;
	} else {
		UE_LOG_WARNING("ViewportControlWidget: 아이콘 로드 실패: %s", (IconBasePath + "quad.png").c_str());
	}

	IconSquare = AssetManager.LoadTexture(IconBasePath + "square.png");
	if (IconSquare) {
		UE_LOG("ViewportControlWidget: 아이콘 로드 성공: 'square' -> %p", IconSquare);
		LoadedCount++;
	} else {
		UE_LOG_WARNING("ViewportControlWidget: 아이콘 로드 실패: %s", (IconBasePath + "square.png").c_str());
	}

	// 카메라 설정 아이콘 로드
	IconCamera = AssetManager.LoadTexture(IconBasePath + "Camera.png");
	if (IconCamera) {
		UE_LOG("ViewportControlWidget: 아이콘 로드 성공: 'Camera' -> %p", IconCamera);
		LoadedCount++;
	} else {
		UE_LOG_WARNING("ViewportControlWidget: 아이콘 로드 실패: %s", (IconBasePath + "Camera.png").c_str());
	}

	UE_LOG_SUCCESS("ViewportControlWidget: 아이콘 로드 완료 (%d/11)", LoadedCount);
	bIconsLoaded = true;
}

void UViewportControlWidget::Update()
{
	// 필요시 업데이트 로직 추가
	
}

void UViewportControlWidget::RenderWidget()
{
	auto& ViewportManager = UViewportManager::GetInstance();
	if (!ViewportManager.GetRoot())
	{
		return;
	}

	// 먼저 스플리터 선 렌더링 (UI 뒤에서)
	//RenderSplitterLines();

	// 싱글 모드에서는 하나만 렌더링
	if (ViewportManager.GetViewportLayout() == EViewportLayout::Single)
	{
		int32 ActiveViewportIndex = ViewportManager.GetActiveIndex();
		RenderViewportToolbar(ActiveViewportIndex);
	}
	else
	{
		for (int32 i = 0;i < 4;++i)
		{
			RenderViewportToolbar(i);
		}
	}
}

void UViewportControlWidget::RenderViewportToolbar(int32 ViewportIndex)
{
	auto& ViewportManager = UViewportManager::GetInstance();
	const auto& Viewports = ViewportManager.GetViewports();
	const auto& Clients = ViewportManager.GetClients();

	// 뷰포트 범위가 벗어날 경우 종료
	if (ViewportIndex >= static_cast<int32>(Viewports.size()) || ViewportIndex >= static_cast<int32>(Clients.size()))
	{
		return;
	}

	// FutureEngine: null 체크 추가
	if (!Viewports[ViewportIndex] || !Clients[ViewportIndex])
	{
		return;
	}

	const FRect& Rect = Viewports[ViewportIndex]->GetRect();
	if (Rect.Width <= 0 || Rect.Height <= 0)
	{
		return;
	}


	constexpr int32 ToolbarH = 32;
	const ImVec2 Vec1{ static_cast<float>(Rect.Left), static_cast<float>(Rect.Top) };
	const ImVec2 Vec2{ static_cast<float>(Rect.Left + Rect.Width), static_cast<float>(Rect.Top + ToolbarH) };

	// 1) 툴바 배경 그리기
	ImDrawList* DrawLine = ImGui::GetBackgroundDrawList();
	DrawLine->AddRectFilled(Vec1, Vec2, IM_COL32(30, 30, 30, 100));
	DrawLine->AddLine(ImVec2(Vec1.x, Vec2.y), ImVec2(Vec2.x, Vec2.y), IM_COL32(70, 70, 70, 120), 1.0f);

	// 2) 툴바 UI 요소들을 직접 배치 (별도 창 생성하지 않음)
	// UI 요소들을 화면 좌표계에서 직접 배치
	ImGui::SetNextWindowPos(ImVec2(Vec1.x, Vec1.y));
	ImGui::SetNextWindowSize(ImVec2(Vec2.x - Vec1.x, Vec2.y - Vec1.y));

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoTitleBar;

	// 스타일 설정
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.f, 3.f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 3.f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 0.f));

	char WinName[64];
	(void)snprintf(WinName, sizeof(WinName), "##ViewportToolbar%d", ViewportIndex);

	ImGui::PushID(ViewportIndex);
	if (ImGui::Begin(WinName, nullptr, flags))
	{
		// ViewMode 커스텀 버튼
		EViewModeIndex CurrentMode = Clients[ViewportIndex]->GetViewMode();
		int32 CurrentModeIndex = static_cast<int32>(CurrentMode);

		// 버튼 스타일 설정
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

		// 커스텀 버튼 (LitCube 아이콘 + 텍스트)
		constexpr float ViewModeButtonWidth = 140.0f;
		constexpr float ViewModeButtonHeight = 24.0f;
		constexpr float ViewModeIconSize = 16.0f;
		constexpr float ViewModePadding = 4.0f;

		ImVec2 ViewModeButtonPos = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("##ViewModeButton", ImVec2(ViewModeButtonWidth, ViewModeButtonHeight));
		bool bViewModeClicked = ImGui::IsItemClicked();
		bool bViewModeHovered = ImGui::IsItemHovered();

		// 버튼 배경 그리기
		ImDrawList* ViewModeDrawList = ImGui::GetWindowDrawList();
		ImU32 ViewModeBgColor = bViewModeHovered ? IM_COL32(26, 26, 26, 255) : IM_COL32(0, 0, 0, 255);
		if (ImGui::IsItemActive())
		{
			ViewModeBgColor = IM_COL32(38, 38, 38, 255);
		}
		ViewModeDrawList->AddRectFilled(ViewModeButtonPos, ImVec2(ViewModeButtonPos.x + ViewModeButtonWidth, ViewModeButtonPos.y + ViewModeButtonHeight), ViewModeBgColor, 4.0f);
		ViewModeDrawList->AddRect(ViewModeButtonPos, ImVec2(ViewModeButtonPos.x + ViewModeButtonWidth, ViewModeButtonPos.y + ViewModeButtonHeight), IM_COL32(96, 96, 96, 255), 4.0f);

		// LitCube 아이콘 그리기
		if (IconLitCube && IconLitCube->GetTextureSRV())
		{
			const ImVec2 ViewModeIconPos = ImVec2(ViewModeButtonPos.x + ViewModePadding, ViewModeButtonPos.y + (ViewModeButtonHeight - ViewModeIconSize) * 0.5f);
			ViewModeDrawList->AddImage(
				IconLitCube->GetTextureSRV(),
				ViewModeIconPos,
				ImVec2(ViewModeIconPos.x + ViewModeIconSize, ViewModeIconPos.y + ViewModeIconSize)
			);
		}

		// 텍스트 그리기
		const ImVec2 ViewModeTextPos = ImVec2(ViewModeButtonPos.x + ViewModePadding + ViewModeIconSize + ViewModePadding, ViewModeButtonPos.y + (ViewModeButtonHeight - ImGui::GetTextLineHeight()) * 0.5f);
		ViewModeDrawList->AddText(ViewModeTextPos, IM_COL32(220, 220, 220, 255), ViewModeLabels[CurrentModeIndex]);

		// 버튼 클릭 시 팝업 열기
		if (bViewModeClicked)
		{
			ImGui::OpenPopup("##ViewModePopup");
		}

		// 팝업 메뉴
		if (ImGui::BeginPopup("##ViewModePopup"))
		{
			for (int i = 0; i < IM_ARRAYSIZE(ViewModeLabels); ++i)
			{
				// 각 항목에 LitCube 아이콘 표시
				if (IconLitCube && IconLitCube->GetTextureSRV())
				{
					ImGui::Image(IconLitCube->GetTextureSRV(), ImVec2(16, 16));
					ImGui::SameLine();
				}

				if (ImGui::MenuItem(ViewModeLabels[i], nullptr, i == CurrentModeIndex))
				{
					Clients[ViewportIndex]->SetViewMode(static_cast<EViewModeIndex>(i));
					UE_LOG("ViewportControlWidget: Viewport[%d]의 ViewMode를 %s로 변경",
						ViewportIndex, ViewModeLabels[i]);
				}
			}
			ImGui::EndPopup();
		}

		ImGui::PopStyleColor(4);
		ImGui::SameLine(0.0f, 5.0f);

		// 회전 스냅 토글 버튼 (아이콘)
		{
			constexpr float SnapToggleButtonSize = 24.0f;
			constexpr float SnapToggleIconSize = 16.0f;

			bool bSnapEnabled = ViewportManager.IsRotationSnapEnabled();

			ImVec2 SnapToggleButtonPos = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##RotationSnapToggle", ImVec2(SnapToggleButtonSize, SnapToggleButtonSize));
			bool bSnapToggleClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
			bool bSnapToggleHovered = ImGui::IsItemHovered();

			// 버튼 배경 그리기
			ImDrawList* SnapToggleDrawList = ImGui::GetWindowDrawList();
			ImU32 SnapToggleBgColor;
			if (bSnapEnabled)
			{
				SnapToggleBgColor = bSnapToggleHovered ? IM_COL32(40, 40, 40, 255) : IM_COL32(20, 20, 20, 255);
			}
			else
			{
				SnapToggleBgColor = bSnapToggleHovered ? IM_COL32(26, 26, 26, 255) : IM_COL32(0, 0, 0, 255);
			}
			if (ImGui::IsItemActive())
			{
				SnapToggleBgColor = IM_COL32(50, 50, 50, 255);
			}
			SnapToggleDrawList->AddRectFilled(SnapToggleButtonPos, ImVec2(SnapToggleButtonPos.x + SnapToggleButtonSize, SnapToggleButtonPos.y + SnapToggleButtonSize), SnapToggleBgColor, 4.0f);

			// 테두리
			ImU32 SnapToggleBorderColor = bSnapEnabled ? IM_COL32(150, 150, 150, 255) : IM_COL32(96, 96, 96, 255);
			SnapToggleDrawList->AddRect(SnapToggleButtonPos, ImVec2(SnapToggleButtonPos.x + SnapToggleButtonSize, SnapToggleButtonPos.y + SnapToggleButtonSize), SnapToggleBorderColor, 4.0f);

			// 회전 아이콘 (중앙 정렬)
			ImVec2 SnapToggleIconCenter = ImVec2(
				SnapToggleButtonPos.x + SnapToggleButtonSize * 0.5f,
				SnapToggleButtonPos.y + SnapToggleButtonSize * 0.5f
			);
			float SnapToggleIconRadius = SnapToggleIconSize * 0.4f;
			ImU32 SnapToggleIconColor = bSnapEnabled ? IM_COL32(220, 220, 220, 255) : IM_COL32(120, 120, 120, 255);
			SnapToggleDrawList->AddCircle(SnapToggleIconCenter, SnapToggleIconRadius, SnapToggleIconColor, 12, 1.5f);
			SnapToggleDrawList->PathArcTo(SnapToggleIconCenter, SnapToggleIconRadius + 2.0f, 0.0f, 1.5f, 8);
			SnapToggleDrawList->PathStroke(SnapToggleIconColor, 0, 1.5f);

			if (bSnapToggleClicked)
			{
				ViewportManager.SetRotationSnapEnabled(!bSnapEnabled);
			}

			if (bSnapToggleHovered)
			{
				ImGui::SetTooltip("Toggle rotation snap");
			}
		}

		ImGui::SameLine(0.0f, 4.0f);

		// 회전 스냅 각도 선택 버튼
		{
			char SnapAngleText[16];
			(void)snprintf(SnapAngleText, sizeof(SnapAngleText), "%.0f°", ViewportManager.GetRotationSnapAngle());

			constexpr float SnapAngleButtonHeight = 24.0f;
			constexpr float SnapAnglePadding = 8.0f;
			const ImVec2 SnapAngleTextSize = ImGui::CalcTextSize(SnapAngleText);
			const float SnapAngleButtonWidth = SnapAngleTextSize.x + SnapAnglePadding * 2;

			ImVec2 SnapAngleButtonPos = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##RotationSnapAngle", ImVec2(SnapAngleButtonWidth, SnapAngleButtonHeight));
			bool bSnapAngleClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
			bool bSnapAngleHovered = ImGui::IsItemHovered();

			// 버튼 배경 그리기
			ImDrawList* SnapAngleDrawList = ImGui::GetWindowDrawList();
			ImU32 SnapAngleBgColor = bSnapAngleHovered ? IM_COL32(26, 26, 26, 255) : IM_COL32(0, 0, 0, 255);
			if (ImGui::IsItemActive())
			{
				SnapAngleBgColor = IM_COL32(38, 38, 38, 255);
			}
			SnapAngleDrawList->AddRectFilled(SnapAngleButtonPos, ImVec2(SnapAngleButtonPos.x + SnapAngleButtonWidth, SnapAngleButtonPos.y + SnapAngleButtonHeight), SnapAngleBgColor, 4.0f);
			SnapAngleDrawList->AddRect(SnapAngleButtonPos, ImVec2(SnapAngleButtonPos.x + SnapAngleButtonWidth, SnapAngleButtonPos.y + SnapAngleButtonHeight), IM_COL32(96, 96, 96, 255), 4.0f);

			// 텍스트 그리기
			ImVec2 SnapAngleTextPos = ImVec2(
				SnapAngleButtonPos.x + SnapAnglePadding,
				SnapAngleButtonPos.y + (SnapAngleButtonHeight - ImGui::GetTextLineHeight()) * 0.5f
			);
			SnapAngleDrawList->AddText(SnapAngleTextPos, IM_COL32(220, 220, 220, 255), SnapAngleText);

			if (bSnapAngleClicked)
			{
				ImGui::OpenPopup("##RotationSnapAnglePopup");
			}

			// 각도 선택 팝업
			if (ImGui::BeginPopup("##RotationSnapAnglePopup"))
			{
				ImGui::Text("Rotation Snap Angle");
				ImGui::Separator();

				const float CurrentSnapAngle = ViewportManager.GetRotationSnapAngle();
				constexpr float SnapAngles[] = { 5.0f, 10.0f, 15.0f, 22.5f, 30.0f, 45.0f, 60.0f, 90.0f };
				constexpr const char* SnapAngleLabels[] = { "5°", "10°", "15°", "22.5°", "30°", "45°", "60°", "90°" };

				for (int i = 0; i < IM_ARRAYSIZE(SnapAngles); ++i)
				{
					const bool bIsSelected = (std::abs(CurrentSnapAngle - SnapAngles[i]) < 0.1f);
					if (ImGui::MenuItem(SnapAngleLabels[i], nullptr, bIsSelected))
					{
						ViewportManager.SetRotationSnapAngle(SnapAngles[i]);
					}
				}

				ImGui::EndPopup();
			}

			if (bSnapAngleHovered)
			{
				ImGui::SetTooltip("Choose rotation snap angle");
			}
		}

		// 우측 정렬할 버튼들의 총 너비 계산
		bool bInPilotMode = IsPilotModeActive(ViewportIndex);
		UEditor* Editor = GEditor ? GEditor->GetEditorModule() : nullptr;
		AActor* PilotedActor = (Editor && bInPilotMode) ? Editor->GetPilotedActor() : nullptr;

		// ViewType 버튼 폭 계산
		constexpr float RightViewTypeButtonWidthDefault = 110.0f;
		constexpr float RightViewTypeIconSize = 16.0f;
		constexpr float RightViewTypePadding = 4.0f;
		float RightViewTypeButtonWidth = RightViewTypeButtonWidthDefault;

		// Actor 이름 저장용
		static FString CachedActorName;

		if (bInPilotMode && PilotedActor)
		{
			CachedActorName = PilotedActor->GetName().ToString();
			const ImVec2 ActorNameTextSize = ImGui::CalcTextSize(CachedActorName.c_str());
			// 아이콘 + 패딩 + 텍스트 + 패딩
			RightViewTypeButtonWidth = RightViewTypePadding + RightViewTypeIconSize + RightViewTypePadding + ActorNameTextSize.x + RightViewTypePadding;
			// 최소 / 최대 폭 제한
			RightViewTypeButtonWidth = Clamp(RightViewTypeButtonWidth, 110.0f, 250.0f);
		}

		constexpr float CameraSpeedButtonWidth = 70.0f; // 아이콘 + 숫자 표시를 위해 확장
		constexpr float LayoutToggleButtonSize = 24.0f;
		constexpr float RightButtonSpacing = 6.0f;
		constexpr float PilotExitButtonSize = 24.0f;
		const float PilotModeExtraWidth = bInPilotMode ? (PilotExitButtonSize + RightButtonSpacing) : 0.0f;
		const float TotalRightButtonsWidth = RightViewTypeButtonWidth + RightButtonSpacing + PilotModeExtraWidth + CameraSpeedButtonWidth + RightButtonSpacing + LayoutToggleButtonSize;

		// 우측 정렬 시작
		{
			const float ContentRegionRight = ImGui::GetWindowContentRegionMax().x;
			float RightAlignedX = ContentRegionRight - TotalRightButtonsWidth - 6.0f;
			RightAlignedX = std::max(RightAlignedX, ImGui::GetCursorPosX());

			ImGui::SameLine();
			ImGui::SetCursorPosX(RightAlignedX);
		}

		// 우측 버튼 1: 카메라 뷰 타입 (파일럿 모드 지원)
		{
			// 현재 뷰 타입 정보
			EViewType CurrentViewType = Clients[ViewportIndex]->GetViewType();
			int32 CurrentViewTypeIndex = static_cast<int32>(CurrentViewType);
			UTexture* ViewTypeIcons[7] = { IconPerspective, IconTop, IconBottom, IconLeft, IconRight, IconFront, IconBack };

			// 현재 뷰 타입에 맞는 아이콘 가져오기
			UTexture* RightViewTypeIcon = nullptr;
			switch (CurrentViewType)
			{
				case EViewType::Perspective: RightViewTypeIcon = IconPerspective; break;
				case EViewType::OrthoTop: RightViewTypeIcon = IconTop; break;
				case EViewType::OrthoBottom: RightViewTypeIcon = IconBottom; break;
				case EViewType::OrthoLeft: RightViewTypeIcon = IconLeft; break;
				case EViewType::OrthoRight: RightViewTypeIcon = IconRight; break;
				case EViewType::OrthoFront: RightViewTypeIcon = IconFront; break;
				case EViewType::OrthoBack: RightViewTypeIcon = IconBack; break;
			}

			// 표시할 텍스트 (파일럿 모드면 Actor 이름, 아니면 ViewType)
			static char TruncatedActorName[128] = "";
			const char* DisplayText = ViewTypeLabels[CurrentViewTypeIndex];

			if (bInPilotMode && PilotedActor && !CachedActorName.empty())
			{
				// 최대 표시 가능한 텍스트 폭 계산 (버튼 폭 - 아이콘 - 패딩들)
				const float MaxTextWidth = 250.0f - RightViewTypePadding - RightViewTypeIconSize - RightViewTypePadding * 2;
				const ImVec2 ActorNameSize = ImGui::CalcTextSize(CachedActorName.c_str());

				if (ActorNameSize.x > MaxTextWidth)
				{
					// 텍스트가 너무 길면 "..." 축약
					size_t Len = CachedActorName.length();

					// "..." 제외한 텍스트 길이를 줄여가며 맞춤
					while (Len > 3)
					{
						std::string Truncated = CachedActorName.substr(0, Len - 3) + "...";
						ImVec2 TruncatedSize = ImGui::CalcTextSize(Truncated.c_str());
						if (TruncatedSize.x <= MaxTextWidth)
						{
							(void)snprintf(TruncatedActorName, sizeof(TruncatedActorName), "%s", Truncated.c_str());
							break;
						}
						Len--;
					}
					DisplayText = TruncatedActorName;
				}
				else
				{
					DisplayText = CachedActorName.c_str();
				}
			}

			constexpr float RightViewTypeButtonHeight = 24.0f;

			ImVec2 RightViewTypeButtonPos = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##RightViewTypeButton", ImVec2(RightViewTypeButtonWidth, RightViewTypeButtonHeight));
			bool bRightViewTypeClicked = ImGui::IsItemClicked();
			bool bRightViewTypeHovered = ImGui::IsItemHovered();

			// 버튼 배경 그리기 (파일럿 모드면 강조 색상)
			ImDrawList* RightViewTypeDrawList = ImGui::GetWindowDrawList();
			ImU32 RightViewTypeBgColor;
			if (bInPilotMode)
			{
				RightViewTypeBgColor = bRightViewTypeHovered ? IM_COL32(40, 60, 80, 255) : IM_COL32(30, 50, 70, 255);
			}
			else
			{
				RightViewTypeBgColor = bRightViewTypeHovered ? IM_COL32(26, 26, 26, 255) : IM_COL32(0, 0, 0, 255);
			}
			if (ImGui::IsItemActive())
			{
				RightViewTypeBgColor = bInPilotMode ? IM_COL32(50, 70, 90, 255) : IM_COL32(38, 38, 38, 255);
			}
			RightViewTypeDrawList->AddRectFilled(RightViewTypeButtonPos, ImVec2(RightViewTypeButtonPos.x + RightViewTypeButtonWidth, RightViewTypeButtonPos.y + RightViewTypeButtonHeight), RightViewTypeBgColor, 4.0f);
			RightViewTypeDrawList->AddRect(RightViewTypeButtonPos, ImVec2(RightViewTypeButtonPos.x + RightViewTypeButtonWidth, RightViewTypeButtonPos.y + RightViewTypeButtonHeight), bInPilotMode ? IM_COL32(100, 150, 200, 255) : IM_COL32(96, 96, 96, 255), 4.0f);

			// 아이콘 그리기
			if (RightViewTypeIcon && RightViewTypeIcon->GetTextureSRV())
			{
				const ImVec2 RightViewTypeIconPos = ImVec2(RightViewTypeButtonPos.x + RightViewTypePadding, RightViewTypeButtonPos.y + (RightViewTypeButtonHeight - RightViewTypeIconSize) * 0.5f);
				RightViewTypeDrawList->AddImage(
					RightViewTypeIcon->GetTextureSRV(),
					RightViewTypeIconPos,
					ImVec2(RightViewTypeIconPos.x + RightViewTypeIconSize, RightViewTypeIconPos.y + RightViewTypeIconSize)
				);
			}

			// 텍스트 그리기
			const ImVec2 RightViewTypeTextPos = ImVec2(RightViewTypeButtonPos.x + RightViewTypePadding + RightViewTypeIconSize + RightViewTypePadding, RightViewTypeButtonPos.y + (RightViewTypeButtonHeight - ImGui::GetTextLineHeight()) * 0.5f);
			RightViewTypeDrawList->AddText(RightViewTypeTextPos, bInPilotMode ? IM_COL32(180, 220, 255, 255) : IM_COL32(220, 220, 220, 255), DisplayText);

			if (bRightViewTypeClicked)
			{
				ImGui::OpenPopup("##RightViewTypeDropdown");
			}

			// ViewType 드롭다운 팝업
			if (ImGui::BeginPopup("##RightViewTypeDropdown"))
			{
				ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

				// 파일럿 모드 항목 (최상단, 선택된 상태로 표시)
				if (bInPilotMode && PilotedActor && !CachedActorName.empty())
				{
					if (IconPerspective && IconPerspective->GetTextureSRV())
					{
						ImGui::Image((ImTextureID)IconPerspective->GetTextureSRV(), ImVec2(16, 16));
						ImGui::SameLine();
					}

					// 선택된 상태로 표시 (체크마크) - CachedActorName 사용
					ImGui::MenuItem(CachedActorName.c_str(), nullptr, true, false); // 선택됨, 비활성화

					ImGui::Separator();
				}

				// 일반 ViewType 항목들
				for (int i = 0; i < IM_ARRAYSIZE(ViewTypeLabels); ++i)
				{
					if (ViewTypeIcons[i] && ViewTypeIcons[i]->GetTextureSRV())
					{
						ImGui::Image((ImTextureID)ViewTypeIcons[i]->GetTextureSRV(), ImVec2(16, 16));
						ImGui::SameLine();
					}

					bool bIsCurrentViewType = (i == CurrentViewTypeIndex && !bInPilotMode);
					if (ImGui::MenuItem(ViewTypeLabels[i], nullptr, bIsCurrentViewType))
					{
						// ViewType 변경 시 파일럿 모드 종료
						if (bInPilotMode && Editor)
						{
							Editor->RequestExitPilotMode();
						}

						EViewType NewType = static_cast<EViewType>(i);
						Clients[ViewportIndex]->SetViewType(NewType);
						UE_LOG("ViewportControlWidget: Viewport[%d]의 ViewType을 %s로 변경",
							ViewportIndex, ViewTypeLabels[i]);
					}
				}

				ImGui::PopStyleColor();
				ImGui::EndPopup();
			}
		}

		ImGui::SameLine(0.0f, RightButtonSpacing);

		// 파일럿 모드 Exit 버튼 (ViewType 버튼 옆)
		if (IsPilotModeActive(ViewportIndex))
		{
			ImVec2 ExitButtonCursorPos = ImGui::GetCursorScreenPos();
			RenderPilotModeExitButton(ViewportIndex, ExitButtonCursorPos);
			ImGui::SetCursorScreenPos(ExitButtonCursorPos);
			ImGui::SameLine(0.0f, RightButtonSpacing);
		}

		// 우측 버튼 2: 카메라 속도 (아이콘 + 숫자 표시)
		if (IconCamera && IconCamera->GetTextureSRV())
		{
			float EditorCameraSpeed = ViewportManager.GetEditorCameraSpeed();
			char CameraSpeedText[16];
			(void)snprintf(CameraSpeedText, sizeof(CameraSpeedText), "%.1f", EditorCameraSpeed);

			constexpr float CameraSpeedButtonHeight = 24.0f;
			constexpr float CameraSpeedPadding = 8.0f;
			constexpr float CameraSpeedIconSize = 16.0f;

			ImVec2 CameraSpeedButtonPos = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##CameraSpeedButton", ImVec2(CameraSpeedButtonWidth, CameraSpeedButtonHeight));
			bool bCameraSpeedClicked = ImGui::IsItemClicked();
			bool bCameraSpeedHovered = ImGui::IsItemHovered();

			// 버튼 배경 그리기
			ImDrawList* CameraSpeedDrawList = ImGui::GetWindowDrawList();
			ImU32 CameraSpeedBgColor = bCameraSpeedHovered ? IM_COL32(26, 26, 26, 255) : IM_COL32(0, 0, 0, 255);
			if (ImGui::IsItemActive())
			{
				CameraSpeedBgColor = IM_COL32(38, 38, 38, 255);
			}
			CameraSpeedDrawList->AddRectFilled(CameraSpeedButtonPos, ImVec2(CameraSpeedButtonPos.x + CameraSpeedButtonWidth, CameraSpeedButtonPos.y + CameraSpeedButtonHeight), CameraSpeedBgColor, 4.0f);
			CameraSpeedDrawList->AddRect(CameraSpeedButtonPos, ImVec2(CameraSpeedButtonPos.x + CameraSpeedButtonWidth, CameraSpeedButtonPos.y + CameraSpeedButtonHeight), IM_COL32(96, 96, 96, 255), 4.0f);

			// 아이콘 그리기 (왼쪽)
			const ImVec2 CameraSpeedIconPos = ImVec2(
				CameraSpeedButtonPos.x + CameraSpeedPadding,
				CameraSpeedButtonPos.y + (CameraSpeedButtonHeight - CameraSpeedIconSize) * 0.5f
			);
			CameraSpeedDrawList->AddImage(
				IconCamera->GetTextureSRV(),
				CameraSpeedIconPos,
				ImVec2(CameraSpeedIconPos.x + CameraSpeedIconSize, CameraSpeedIconPos.y + CameraSpeedIconSize)
			);

			// 텍스트 그리기 (아이콘 오른쪽, 우측 정렬)
			const ImVec2 CameraSpeedTextSize = ImGui::CalcTextSize(CameraSpeedText);
			const ImVec2 CameraSpeedTextPos = ImVec2(
				CameraSpeedButtonPos.x + CameraSpeedButtonWidth - CameraSpeedTextSize.x - CameraSpeedPadding,
				CameraSpeedButtonPos.y + (CameraSpeedButtonHeight - ImGui::GetTextLineHeight()) * 0.5f
			);
			CameraSpeedDrawList->AddText(CameraSpeedTextPos, IM_COL32(220, 220, 220, 255), CameraSpeedText);

			if (bCameraSpeedClicked)
			{
				ImGui::OpenPopup("##CameraSettingsPopup");
			}

			// 카메라 설정 팝업
			if (ImGui::BeginPopup("##CameraSettingsPopup"))
			{
				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

				if (UCamera* Camera = Clients[ViewportIndex]->GetCamera())
				{
					EViewType PopupViewType = Clients[ViewportIndex]->GetViewType();
					const bool bIsPerspective = (PopupViewType == EViewType::Perspective);
					ImGui::Text(bIsPerspective ? "Perspective Camera Settings" : "Orthographic Camera Settings");
					ImGui::Separator();

					// 전역 카메라 이동 속도
					float editorCameraSpeed = ViewportManager.GetEditorCameraSpeed();
					if (ImGui::DragFloat("Camera Speed", &editorCameraSpeed, 0.1f,
						UViewportManager::MIN_CAMERA_SPEED, UViewportManager::MAX_CAMERA_SPEED, "%.1f"))
					{
						ViewportManager.SetEditorCameraSpeed(editorCameraSpeed);
					}

					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Applies to all viewports");
					}

					// 카메라 위치
					FVector location = Camera->GetLocation();
					if (ImGui::DragFloat3("Location", &location.X, 0.1f))
					{
						Camera->SetLocation(location);
					}

					// 카메라 회전 (Perspective만 표시)
					if (bIsPerspective)
					{
						static FVector CachedRotation = FVector::ZeroVector();
						static bool bIsDraggingRotation = false;

						// 드래그 시작 시 또는 비활성 상태일 때 현재 값 캐싱
						if (!bIsDraggingRotation)
						{
							CachedRotation = Camera->GetRotation();
						}

						bool bRotationChanged = ImGui::DragFloat3("Rotation", &CachedRotation.X, 0.5f);

						// 드래그 상태 추적
						if (ImGui::IsItemActive())
						{
							bIsDraggingRotation = true;

							// 값이 변경되었으면 카메라에 반영
							if (bRotationChanged)
							{
								Camera->SetRotation(CachedRotation);
								// SetRotation 후 wrapping된 값으로 즉시 재동기화
								CachedRotation = Camera->GetRotation();
							}
						}
						else if (bIsDraggingRotation)
						{
							// 드래그 종료 시 최종 동기화
							bIsDraggingRotation = false;
							CachedRotation = Camera->GetRotation();
						}
					}
					// Orthographic 뷰는 회전 항목 없음 (고정된 방향)

					ImGui::Separator();

					if (bIsPerspective)
					{
						// Perspective: FOV 표시
						float Fov = Camera->GetFovY();
						if (ImGui::SliderFloat("FOV", &Fov, 1.0f, 170.0f, "%.1f"))
						{
							Camera->SetFovY(Fov);
						}
					}
					else
					{
						// Orthographic: Zoom Level (OrthoZoom) 표시 및 SharedOrthoZoom 동기화
						float OrthoZoom = Camera->GetOrthoZoom();
						if (ImGui::DragFloat("Zoom Level", &OrthoZoom, 10.0f, 10.0f, 10000.0f, "%.1f"))
						{
							Camera->SetOrthoZoom(OrthoZoom);

							// 모든 Ortho 카메라에 동일한 줌 적용 (SharedOrthoZoom 갱신)
							for (FViewportClient* OtherClient : ViewportManager.GetClients())
							{
								if (OtherClient && OtherClient->IsOrtho())
								{
									if (UCamera* OtherCam = OtherClient->GetCamera())
									{
										OtherCam->SetOrthoZoom(OrthoZoom);
									}
								}
							}
						}

						// 정보: Aspect는 자동 계산됨
						float Aspect = Camera->GetAspect();
						ImGui::BeginDisabled();
						ImGui::DragFloat("Aspect Ratio", &Aspect, 0.01f, 0.1f, 10.0f, "%.3f");
						ImGui::EndDisabled();
					}

					// Near/Far Plane
					float NearZ = Camera->GetNearZ();
					if (ImGui::DragFloat("Near Z", &NearZ, 0.01f, 0.01f, 100.0f, "%.3f"))
					{
						Camera->SetNearZ(NearZ);
					}

					float FarZ = Camera->GetFarZ();
					if (ImGui::DragFloat("Far Z", &FarZ, 1.0f, 1.0f, 10000.0f, "%.1f"))
					{
						Camera->SetFarZ(FarZ);
					}
				}

				ImGui::PopStyleColor(4);
				ImGui::EndPopup();
			}
		}

		ImGui::SameLine(0.0f, RightButtonSpacing);

		// 우측 버튼 3: 레이아웃 토글 버튼
		constexpr float LayoutToggleIconSize = 16.0f;

		if (ViewportManager.GetViewportLayout() == EViewportLayout::Single)
		{
			// Quad 레이아웃으로 전환 버튼
			if (IconQuad && IconQuad->GetTextureSRV())
			{
				const ImVec2 LayoutToggleQuadButtonPos = ImGui::GetCursorScreenPos();
				ImGui::InvisibleButton("##LayoutToggleQuadButton", ImVec2(LayoutToggleButtonSize, LayoutToggleButtonSize));
				const bool bLayoutToggleQuadClicked = ImGui::IsItemClicked();
				const bool bLayoutToggleQuadHovered = ImGui::IsItemHovered();

				// 버튼 배경 그리기
				ImDrawList* LayoutToggleQuadDrawList = ImGui::GetWindowDrawList();
				ImU32 LayoutToggleQuadBgColor = bLayoutToggleQuadHovered ? IM_COL32(26, 26, 26, 255) : IM_COL32(0, 0, 0, 255);
				if (ImGui::IsItemActive())
				{
					LayoutToggleQuadBgColor = IM_COL32(38, 38, 38, 255);
				}
				LayoutToggleQuadDrawList->AddRectFilled(LayoutToggleQuadButtonPos, ImVec2(LayoutToggleQuadButtonPos.x + LayoutToggleButtonSize, LayoutToggleQuadButtonPos.y + LayoutToggleButtonSize), LayoutToggleQuadBgColor, 4.0f);
				LayoutToggleQuadDrawList->AddRect(LayoutToggleQuadButtonPos, ImVec2(LayoutToggleQuadButtonPos.x + LayoutToggleButtonSize, LayoutToggleQuadButtonPos.y + LayoutToggleButtonSize), IM_COL32(96, 96, 96, 255), 4.0f);

				// 아이콘 그리기 (중앙 정렬)
				const ImVec2 LayoutToggleQuadIconPos = ImVec2(
					LayoutToggleQuadButtonPos.x + (LayoutToggleButtonSize - LayoutToggleIconSize) * 0.5f,
					LayoutToggleQuadButtonPos.y + (LayoutToggleButtonSize - LayoutToggleIconSize) * 0.5f
				);
				LayoutToggleQuadDrawList->AddImage(
					IconQuad->GetTextureSRV(),
					LayoutToggleQuadIconPos,
					ImVec2(LayoutToggleQuadIconPos.x + LayoutToggleIconSize, LayoutToggleQuadIconPos.y + LayoutToggleIconSize)
				);

				if (bLayoutToggleQuadClicked)
				{
					CurrentLayout = ELayout::Quad;
					ViewportManager.SetViewportLayout(EViewportLayout::Quad);
					ViewportManager.StartLayoutAnimation(true, ViewportIndex);
				}
			}
		}

		if (ViewportManager.GetViewportLayout() == EViewportLayout::Quad)
		{
			// Single 레이아웃으로 전환 버튼
			if (IconSquare && IconSquare->GetTextureSRV())
			{
				const ImVec2 LayoutToggleSingleButtonPos = ImGui::GetCursorScreenPos();
				ImGui::InvisibleButton("##LayoutToggleSingleButton", ImVec2(LayoutToggleButtonSize, LayoutToggleButtonSize));
				const bool bLayoutToggleSingleClicked = ImGui::IsItemClicked();
				const bool bLayoutToggleSingleHovered = ImGui::IsItemHovered();

				// 버튼 배경 그리기
				ImDrawList* LayoutToggleSingleDrawList = ImGui::GetWindowDrawList();
				ImU32 LayoutToggleSingleBgColor = bLayoutToggleSingleHovered ? IM_COL32(26, 26, 26, 255) : IM_COL32(0, 0, 0, 255);
				if (ImGui::IsItemActive())
				{
					LayoutToggleSingleBgColor = IM_COL32(38, 38, 38, 255);
				}
				LayoutToggleSingleDrawList->AddRectFilled(LayoutToggleSingleButtonPos, ImVec2(LayoutToggleSingleButtonPos.x + LayoutToggleButtonSize, LayoutToggleSingleButtonPos.y + LayoutToggleButtonSize), LayoutToggleSingleBgColor, 4.0f);
				LayoutToggleSingleDrawList->AddRect(LayoutToggleSingleButtonPos, ImVec2(LayoutToggleSingleButtonPos.x + LayoutToggleButtonSize, LayoutToggleSingleButtonPos.y + LayoutToggleButtonSize), IM_COL32(96, 96, 96, 255), 4.0f);

				// 아이콘 그리기 (중앙 정렬)
				const ImVec2 LayoutToggleSingleIconPos = ImVec2(
					LayoutToggleSingleButtonPos.x + (LayoutToggleButtonSize - LayoutToggleIconSize) * 0.5f,
					LayoutToggleSingleButtonPos.y + (LayoutToggleButtonSize - LayoutToggleIconSize) * 0.5f
				);
				LayoutToggleSingleDrawList->AddImage(
					IconSquare->GetTextureSRV(),
					LayoutToggleSingleIconPos,
					ImVec2(LayoutToggleSingleIconPos.x + LayoutToggleIconSize, LayoutToggleSingleIconPos.y + LayoutToggleIconSize)
				);

				if (bLayoutToggleSingleClicked)
				{
					CurrentLayout = ELayout::Single;
					ViewportManager.SetViewportLayout(EViewportLayout::Single);
					// 스플리터 비율을 저장하고 애니메이션 시작: Quad → Single
					ViewportManager.PersistSplitterRatios();
					ViewportManager.StartLayoutAnimation(false, ViewportIndex);
				}
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar(3);
	ImGui::PopID();
}

bool UViewportControlWidget::IsPilotModeActive(int32 ViewportIndex) const
{
	if (!GEditor)
	{
		return false;
	}

	UEditor* Editor = GEditor->GetEditorModule();
	if (!Editor || !Editor->IsPilotMode())
	{
		return false;
	}

	// 현재 뷰포트가 파일럿 모드를 사용 중인지 확인
	auto& ViewportManager = UViewportManager::GetInstance();
	return (ViewportManager.GetLastClickedViewportIndex() == ViewportIndex);
}

void UViewportControlWidget::RenderPilotModeExitButton(int32 ViewportIndex, ImVec2& InOutCursorPos)
{
	if (!GEditor)
	{
		return;
	}

	UEditor* Editor = GEditor->GetEditorModule();
	if (!Editor || !Editor->IsPilotMode())
	{
		return;
	}

	// △ 아이콘 버튼 (파일럿 모드 종료)
	constexpr float ExitButtonSize = 24.0f;
	constexpr float TriangleSize = 8.0f;

	ImVec2 ExitButtonPos = InOutCursorPos;
	ImGui::SetCursorScreenPos(ExitButtonPos);
	ImGui::InvisibleButton("##PilotModeExit", ImVec2(ExitButtonSize, ExitButtonSize));
	bool bExitClicked = ImGui::IsItemClicked();
	bool bExitHovered = ImGui::IsItemHovered();

	// 버튼 배경
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImU32 BgColor = bExitHovered ? IM_COL32(60, 40, 40, 255) : IM_COL32(40, 20, 20, 255);
	if (ImGui::IsItemActive())
	{
		BgColor = IM_COL32(80, 50, 50, 255);
	}
	DrawList->AddRectFilled(ExitButtonPos, ImVec2(ExitButtonPos.x + ExitButtonSize, ExitButtonPos.y + ExitButtonSize), BgColor, 4.0f);
	DrawList->AddRect(ExitButtonPos, ImVec2(ExitButtonPos.x + ExitButtonSize, ExitButtonPos.y + ExitButtonSize), IM_COL32(160, 80, 80, 255), 4.0f);

	// △ 아이콘 그리기 (Eject 느낌)
	ImVec2 Center = ImVec2(ExitButtonPos.x + ExitButtonSize * 0.5f, ExitButtonPos.y + ExitButtonSize * 0.5f);
	ImVec2 P1 = ImVec2(Center.x, Center.y - TriangleSize * 0.6f);
	ImVec2 P2 = ImVec2(Center.x - TriangleSize * 0.5f, Center.y + TriangleSize * 0.4f);
	ImVec2 P3 = ImVec2(Center.x + TriangleSize * 0.5f, Center.y + TriangleSize * 0.4f);
	DrawList->AddTriangleFilled(P1, P2, P3, IM_COL32(220, 180, 180, 255));

	if (bExitClicked)
	{
		Editor->RequestExitPilotMode();
		UE_LOG_INFO("ViewportControlWidget: Pilot mode exited via UI button");
	}

	if (bExitHovered)
	{
		ImGui::SetTooltip("Exit Pilot Mode (Alt + G)");
	}

	// 커서 위치 업데이트 (다음 버튼 배치용)
	InOutCursorPos.x += ExitButtonSize + 6.0f;
}
