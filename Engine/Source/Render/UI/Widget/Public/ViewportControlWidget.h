#pragma once
#include "Widget.h"

class UTexture;


// UE 스타일 아이콘 타입
enum class EUEViewportIcon : uint8
{
	Single,
	Quad
};

struct FUEImgui
{
	// 팔레트 (언리얼 톤 느낌)
	static ImU32 ColBG() { return IM_COL32(45, 45, 45, 255); }
	static ImU32 ColBGHover() { return IM_COL32(58, 58, 58, 255); }
	static ImU32 ColBGActive() { return IM_COL32(74, 74, 74, 255); }
	static ImU32 ColBorder() { return IM_COL32(96, 96, 96, 255); }
	static ImU32 ColBorderHot() { return IM_COL32(110, 110, 110, 255); }
	static ImU32 ColIcon() { return IM_COL32(205, 205, 205, 255); }
	static ImU32 ColIconActive() { return IM_COL32(46, 163, 255, 255); } // 액티브 포커스 블루

	// 언리얼풍 툴바 아이콘 버튼
	static bool ToolbarIconButton(const char* InId, EUEViewportIcon InIcon, bool bInActive,
		const ImVec2& InSize = ImVec2(32.f, 22.f), float InRounding = 6.f)
	{
		ImGui::PushID(InId);
		ImGui::InvisibleButton("##btn", InSize);
		bool bPressed = ImGui::IsItemClicked();
		bool bHovered = ImGui::IsItemHovered();

		ImDrawList* DL = ImGui::GetWindowDrawList();
		ImVec2 Min = ImGui::GetItemRectMin();
		ImVec2 Max = ImGui::GetItemRectMax();

		// 배경
		ImU32 bg = bInActive ? ColBGActive() : (bHovered ? ColBGHover() : ColBG());
		DL->AddRectFilled(Min, Max, bg, InRounding);

		// 외곽 테두리
		ImU32 bd = bHovered ? ColBorderHot() : ColBorder();
		DL->AddRect(Min, Max, bd, InRounding, 0, 1.0f);

		// 콘텐츠 패딩
		const float Pad = 5.f;
		ImVec2 CMin = ImVec2(Min.x + Pad, Min.y + Pad);
		ImVec2 CMax = ImVec2(Max.x - Pad, Max.y - Pad);

		// 아이콘 색
		ImU32 ic = bInActive ? ColIconActive() : ColIcon();

		// 아이콘 그리기
		switch (InIcon)
		{
		case EUEViewportIcon::Single:
		{
			// 안쪽 얇은 사각 (언리얼 뷰포트 단일 레이아웃 아이콘 느낌)
			const float r = 3.f;
			DL->AddRect(CMin, CMax, ic, r, 0, 1.5f);
		}
		break;
		case EUEViewportIcon::Quad:
		{
			// 2x2 그리드
			ImVec2 C = ImVec2((CMin.x + CMax.x) * 0.5f, (CMin.y + CMax.y) * 0.5f);
			DL->AddLine(ImVec2(CMin.x, C.y), ImVec2(CMax.x, C.y), ic, 1.2f);
			DL->AddLine(ImVec2(C.x, CMin.y), ImVec2(C.x, CMax.y), ic, 1.2f);
			// 바깥 테두리도 살짝
			DL->AddRect(CMin, CMax, ic, 3.f, 0, 1.2f);
		}
		break;
		default: break;
		}
		ImGui::PopID();
		return bPressed;
	}
};


class UViewportControlWidget : public UWidget
{
public:
	UViewportControlWidget();
	~UViewportControlWidget();

	void Initialize() override;
	void Update() override;
	void RenderWidget() override;

private:
	void RenderViewportToolbar(int32 ViewportIndex);
	void LoadViewIcons();
	//static void RenderSplitterLines();
	//static void RenderCameraSpeedControl(int32 ViewportIndex);
	//static void RenderGridSizeControl();
	//static void HandleCameraBinding(int32 ViewportIndex, EViewType NewType, int32 TypeIndex);

private:
	// ViewMode와 ViewType 라벨들
	static const char* ViewModeLabels[];
	static const char* ViewTypeLabels[];

	// 현재 레이아웃 상태
	enum class ELayout : uint8 { Single, Quad };
	ELayout CurrentLayout = ELayout::Single;

	// View Type 아이콘들
	UTexture* IconPerspective = nullptr;
	UTexture* IconTop = nullptr;
	UTexture* IconBottom = nullptr;
	UTexture* IconLeft = nullptr;
	UTexture* IconRight = nullptr;
	UTexture* IconFront = nullptr;
	UTexture* IconBack = nullptr;

	// ViewMode 아이콘
	UTexture* IconLitCube = nullptr;

	// 레이아웃 전환 아이콘
	UTexture* IconQuad = nullptr;
	UTexture* IconSquare = nullptr;

	// 카메라 설정 아이콘
	UTexture* IconCamera = nullptr;

	// Gizmo Mode 아이콘들
	UTexture* IconSelect = nullptr;
	UTexture* IconTranslate = nullptr;
	UTexture* IconRotate = nullptr;
	UTexture* IconScale = nullptr;

	bool bIconsLoaded = false;

	// Pilot Mode UI 헬퍼
	void RenderPilotModeExitButton(int32 ViewportIndex, ImVec2& InOutCursorPos);
	bool IsPilotModeActive(int32 ViewportIndex) const;
};


