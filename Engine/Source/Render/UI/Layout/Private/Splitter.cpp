#include "pch.h"
#include "Render/UI/Layout/Public/Splitter.h"
#include "Editor/Public/Editor.h"
#include "Editor/Public/EditorEngine.h"
#include "Level/Public//World.h"
#include "Manager/UI/Public//ViewportManager.h"
#include "Manager/Input/Public/InputManager.h"

void SSplitter::SetChildren(SWindow* InLT, SWindow* InRB)
{
	SideLT = InLT;
	SideRB = InRB;
}

FRect SSplitter::GetHandleRect() const
{
	const float R = GetEffectiveRatio(); // ★ 핵심: 공유/개별 비율 통일 진입점

	if (Orientation == EOrientation::Vertical)
	{
		int32 splitX = Rect.Left + int32(std::lround(Rect.Width * R));
		int32 half = Thickness / 2;
		int32 hx = splitX - half;
		return FRect{ hx, Rect.Top, Thickness, Rect.Height };
	}
	else
	{
		int32 splitY = Rect.Top + int32(std::lround(Rect.Height * R));
		int32 half = Thickness / 2;
		int32 hy = splitY - half;
		return FRect{ Rect.Left, hy, Rect.Width, Thickness };
	}
}

bool SSplitter::IsHandleHover(FPoint Coord) const
{
	// Slightly extend the hit area to make grabbing the handle easier
	   // and to be more forgiving at the cross intersection.
	const FRect h = GetHandleRect();
	const int32 extend = 4; // pixels of tolerance on each side
	const int32 x0 = h.Left - extend;
	const int32 y0 = h.Top - extend;
	const int32 x1 = h.Left + h.Width + extend;
	const int32 y1 = h.Top + h.Height + extend;
	return (Coord.X >= x0) && (Coord.X < x1) && (Coord.Y >= y0) && (Coord.Y < y1);
}



void SSplitter::LayoutChildren()
{
	if (!SideLT || !SideRB)
		return;

	const FRect h = GetHandleRect();
	if (Orientation == EOrientation::Vertical)
	{
		FRect left{ Rect.Left, Rect.Top, max(0L, h.Left - Rect.Left), Rect.Height };
		int32 rightX = h.Left + h.Width;
		FRect right{ rightX, Rect.Top, max(0L, (Rect.Left + Rect.Width) - rightX), Rect.Height };
		SideLT->OnResize(left);
		SideRB->OnResize(right);
	}
	else
	{
		FRect top{ Rect.Left, Rect.Top, Rect.Width, max(0L, h.Top - Rect.Top) };
		int32 bottomY = h.Top + h.Height;
		FRect bottom{ Rect.Left, bottomY, Rect.Width, max(0L, (Rect.Top + Rect.Height) - bottomY) };
		SideLT->OnResize(top);
		SideRB->OnResize(bottom);
	}

}

bool SSplitter::OnMouseDown(FPoint Coord, int Button)
{
	// FutureEngine 철학: GEditor(UEditorEngine) -> EditorModule(UEditor) -> Gizmo
	if (GEditor)
	{
		if (UEditor* Editor = GEditor->GetEditorModule())
		{
			if (Editor->GetGizmo() && Editor->GetGizmo()->IsDragging())
			{
				return false;
			}
		}
	}

	if (Button == 0 && IsHandleHover(Coord))
	{
		bDragging = true;
		// Determine if we are dragging at the cross (both handles overlap)
		bCrossDragging = false;
		if (Orientation == EOrientation::Vertical)
		{
			if (SSplitter* HLeft = Cast(SideLT))
			{
				if (HLeft->Orientation == EOrientation::Horizontal && HLeft->IsHandleHover(Coord)) bCrossDragging = true;
			}
			if (!bCrossDragging)
			{
				if (SSplitter* HRight = Cast(SideRB))
				{
					if (HRight->Orientation == EOrientation::Horizontal && HRight->IsHandleHover(Coord)) bCrossDragging = true;
				}
			}
		}
		else // Horizontal
		{
			if (auto* Root = Cast(UViewportManager::GetInstance().GetQuadRoot()))
			{
				if (Root->Orientation == EOrientation::Vertical && Root->IsHandleHover(Coord))
				{
					bCrossDragging = true;
					UE_LOG("bCrossDragging");
				}
			}
		}
		return true; // handled
	}
	return false;
}

bool SSplitter::OnMouseMove(FPoint Coord)
{
	//UE_LOG("0");
	if (!bDragging)
	{
		return IsHandleHover(Coord);
	}

	// Cross-drag: update both axes
	if (bCrossDragging)
	{
		//UE_LOG("1");
		if (auto* Root = Cast(UViewportManager::GetInstance().GetQuadRoot()))
		{
			// Vertical ratio from root rect
			const int32 spanX = std::max(1L, Root->Rect.Width);
			float rX = float(Coord.X - Root->Rect.Left) / float(spanX);
			float limitX = float(Root->MinChildSize) / float(spanX);
			Root->SetEffectiveRatio(std::clamp(rX, limitX, 1.0f - limitX));

			// Horizontal ratio from one of the horizontal splitters (they share a ratio)
			SSplitter* H = Cast(Root->SideLT);
			if (!H || H->Orientation != EOrientation::Horizontal)
			{
				H = Cast(Root->SideRB);
			}
			if (H && H->Orientation == EOrientation::Horizontal)
			{
				const int32 spanY = std::max(1L, H->Rect.Height);
				float rY = float(Coord.Y - H->Rect.Top) / float(spanY);
				float limitY = float(H->MinChildSize) / float(spanY);
				H->SetEffectiveRatio(std::clamp(rY, limitY, 1.0f - limitY));
			}

			// Re-layout whole tree
			const FRect current = Root->GetRect();
			Root->OnResize(current);
		}
		return true;
	}

	if (Orientation == EOrientation::Vertical)
	{
		//UE_LOG("v");
		const int32 span = std::max(1L, Rect.Width);
		float r = float(Coord.X - Rect.Left) / float(span);
		float limit = float(MinChildSize) / float(span);
		SetEffectiveRatio(std::clamp(r, limit, 1.0f - limit)); // ★ 핵심
	}
	else
	{
		//UE_LOG("h");
		const int32 span = std::max(1L, Rect.Height);
		float r = float(Coord.Y - Rect.Top) / float(span);
		float limit = float(MinChildSize) / float(span);
		SetEffectiveRatio(std::clamp(r, limit, 1.0f - limit)); // ★ 핵심
	}

	// Re-layout entire viewport tree so siblings using shared ratio update too
	if (auto* Root = UViewportManager::GetInstance().GetQuadRoot())
	{
		const FRect current = Root->GetRect();
		Root->OnResize(current);
	}

	return true;
}

bool SSplitter::OnMouseUp(FPoint, int Button)
{
	if (Button == 0 && bDragging)
	{
		bDragging = false;
		bCrossDragging = false;

		// 드래그 종료 시 현재 스플리터 비율을 동기화
		UViewportManager::GetInstance().UpdateIniSaveSharedRatios();

		return true;
	}
	return false;
}

void SSplitter::OnPaint()
{

	if (SideLT) SideLT->OnPaint();
	if (SideRB) SideRB->OnPaint();
	// Draw handle line for visual feedback (ImGui overlay)
	const FRect h = GetHandleRect();
	//ImDrawList * dl = ImGui::GetBackgroundDrawList();
	//ImVec2 p0{ (float)h.X, (float)h.Y };
	//ImVec2 p1{ (float)(h.X + h.W), (float)(h.Y + h.H) };
	//dl->AddRectFilled(p0, p1, IM_COL32(80, 80, 80, 160));
	auto& InputManager = UInputManager::GetInstance();
	const FVector& mp = InputManager.GetMousePosition();
	FPoint P{ LONG(mp.X), LONG(mp.Y) };
	bool hovered = IsHandleHover(P);

	if (Orientation == EOrientation::Horizontal)
	{
		// 1) If sibling horizontal handle is hovered, mirror-hover this one too
		if (!hovered)
		{
			if (auto* Root = Cast(UViewportManager::GetInstance().GetQuadRoot()))
			{
				if (Root->Orientation == EOrientation::Vertical)
				{
					SSplitter* leftH = Cast(Root->SideLT);
					SSplitter* rightH = Cast(Root->SideRB);
					if (leftH && leftH->Orientation == EOrientation::Horizontal && leftH != this && leftH->IsHandleHover(P))
						hovered = true;
					if (!hovered && rightH && rightH->Orientation == EOrientation::Horizontal && rightH != this && rightH->IsHandleHover(P))
						hovered = true;
				}
			}
		}

		// 2) If vertical handle (root) is hovered and Y matches our handle band, also hover
		if (!hovered)
		{
			if (auto* Root = Cast(UViewportManager::GetInstance().GetQuadRoot()))
			{
				if (Root->Orientation == EOrientation::Vertical && Root->IsHandleHover(P))
				{
					const FRect hh = GetHandleRect();
					if (P.Y >= hh.Top && P.Y < hh.Top + hh.Height)
						hovered = true;
				}
			}
		}
	}

	// FutureEngine 철학: 호버링 시 마우스 커서 변경
	if (hovered)
	{
		if (Orientation == EOrientation::Vertical)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}
		else
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
		}
	}

	ImDrawList* dl = ImGui::GetBackgroundDrawList();

	// Only highlight the splitter handle, not the full rect
	ImVec2 p0{ (float)h.Left, (float)h.Top };
	ImVec2 p1{ (float)(h.Left + h.Width), (float)(h.Top + h.Height) };
	const ImU32 col = hovered ? IM_COL32(255, 255, 255, 200) : IM_COL32(80, 80, 80, 160);
	dl->AddRectFilled(p0, p1, col);
}

SWindow* SSplitter::HitTest(FPoint ScreenCoord)
{
	// FutureEngine 철학: GEditor(UEditorEngine) -> EditorModule(UEditor) -> Gizmo
	if (GEditor)
	{
		if (UEditor* Editor = GEditor->GetEditorModule())
		{
			if (Editor->GetGizmo() && Editor->GetGizmo()->IsDragging())
			{
				// 기즈모 드래그 중에는 스플리터 hit test를 바이패스하고 자식에게 위임
				// Handle has priority so splitter can capture drag
				// 기즈모 드래그 중에는 스플리터가 캡처하지 않음
			}
			else if (IsHandleHover(ScreenCoord))
			{
				return this;
			}
		}
	}
	else
	{
		// Editor가 없는 경우 기존 로직 유지
		if (IsHandleHover(ScreenCoord))
			return this;
	}

	// Delegate to children based on rect containment
	if (SideLT)
	{
		const FRect& r = SideLT->GetRect();
		if (ScreenCoord.X >= r.Left && ScreenCoord.X < r.Left + r.Width &&
			ScreenCoord.Y >= r.Top && ScreenCoord.Y < r.Top + r.Height)
		{
			if (auto* hit = SideLT->HitTest(ScreenCoord)) return hit;
			return SideLT;
		}
	}
	if (SideRB)
	{
		const FRect& r = SideRB->GetRect();
		if (ScreenCoord.X >= r.Left && ScreenCoord.X < r.Left + r.Width &&
			ScreenCoord.Y >= r.Top && ScreenCoord.Y < r.Top + r.Height)
		{
			if (auto* hit = SideRB->HitTest(ScreenCoord)) return hit;
			return SideRB;
		}
	}
	return SWindow::HitTest(ScreenCoord);
}
