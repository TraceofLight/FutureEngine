#include "pch.h"
#include "Manager/Input/Public/InputManager.h"
#include "Core/Public/AppWindow.h"
#include "Render/UI/Window/Public/ConsoleWindow.h"

IMPLEMENT_SINGLETON_CLASS(UInputManager, UObject)

UInputManager::UInputManager()
	: MouseWheelDelta(0.0f)
	  , bIsWindowFocused(true)
	  , DoubleClickTime(0.25f)
{
	InitializeKeyMapping();
	InitializeMouseClickStatus();
}

UInputManager::~UInputManager() = default;

void UInputManager::InitializeKeyMapping()
{
	// 알파벳 키 매핑
	VirtualKeyMap['W'] = EKeyInput::W;
	VirtualKeyMap['A'] = EKeyInput::A;
	VirtualKeyMap['S'] = EKeyInput::S;
	VirtualKeyMap['D'] = EKeyInput::D;
	VirtualKeyMap['Q'] = EKeyInput::Q;
	VirtualKeyMap['E'] = EKeyInput::E;
	VirtualKeyMap['R'] = EKeyInput::R;
	VirtualKeyMap['F'] = EKeyInput::F;
	VirtualKeyMap['P'] = EKeyInput::P;
	VirtualKeyMap['G'] = EKeyInput::G;

	// 화살표 키 매핑
	VirtualKeyMap[VK_UP] = EKeyInput::Up;
	VirtualKeyMap[VK_DOWN] = EKeyInput::Down;
	VirtualKeyMap[VK_LEFT] = EKeyInput::Left;
	VirtualKeyMap[VK_RIGHT] = EKeyInput::Right;

	// 액션 키 매핑
	VirtualKeyMap[VK_SPACE] = EKeyInput::Space;
	VirtualKeyMap[VK_RETURN] = EKeyInput::Enter;
	VirtualKeyMap[VK_ESCAPE] = EKeyInput::Esc;
	VirtualKeyMap[VK_TAB] = EKeyInput::Tab;
	VirtualKeyMap[VK_SHIFT] = EKeyInput::Shift;
	VirtualKeyMap[VK_CONTROL] = EKeyInput::Ctrl;
	VirtualKeyMap[VK_MENU] = EKeyInput::Alt;

	// 숫자 키 매핑
	VirtualKeyMap['0'] = EKeyInput::Num0;
	VirtualKeyMap['1'] = EKeyInput::Num1;
	VirtualKeyMap['2'] = EKeyInput::Num2;
	VirtualKeyMap['3'] = EKeyInput::Num3;
	VirtualKeyMap['4'] = EKeyInput::Num4;
	VirtualKeyMap['5'] = EKeyInput::Num5;
	VirtualKeyMap['6'] = EKeyInput::Num6;
	VirtualKeyMap['7'] = EKeyInput::Num7;
	VirtualKeyMap['8'] = EKeyInput::Num8;
	VirtualKeyMap['9'] = EKeyInput::Num9;

	// 마우스 버튼
	VirtualKeyMap[VK_LBUTTON] = EKeyInput::MouseLeft;
	VirtualKeyMap[VK_RBUTTON] = EKeyInput::MouseRight;
	VirtualKeyMap[VK_MBUTTON] = EKeyInput::MouseMiddle;

	// 기타 키 매핑
	VirtualKeyMap[VK_OEM_3] = EKeyInput::Backtick;
	VirtualKeyMap[VK_F1] = EKeyInput::F1;
	VirtualKeyMap[VK_F2] = EKeyInput::F2;
	VirtualKeyMap[VK_F3] = EKeyInput::F3;
	VirtualKeyMap[VK_F4] = EKeyInput::F4;
	VirtualKeyMap[VK_BACK] = EKeyInput::Backspace;
	VirtualKeyMap[VK_DELETE] = EKeyInput::Delete;

	// 모든 키 상태를 false로 초기화
	for (int32 i = 0; i < static_cast<int32>(EKeyInput::End); ++i)
	{
		EKeyInput Key = static_cast<EKeyInput>(i);
		CurrentKeyState[Key] = false;
		PreviousKeyState[Key] = false;
		PendingMouseState[Key] = false;
	}
}

/**
 * @brief 더블클릭 상태 초기화하는 함수
 */
void UInputManager::InitializeMouseClickStatus()
{
	LastClickTime[EKeyInput::MouseLeft] = 0.0f;
	LastClickTime[EKeyInput::MouseRight] = 0.0f;
	LastClickTime[EKeyInput::MouseMiddle] = 0.0f;

	DoubleClickState[EKeyInput::MouseLeft] = false;
	DoubleClickState[EKeyInput::MouseRight] = false;
	DoubleClickState[EKeyInput::MouseMiddle] = false;

	ClickCount[EKeyInput::MouseLeft] = 0;
	ClickCount[EKeyInput::MouseRight] = 0;
	ClickCount[EKeyInput::MouseMiddle] = 0;
}

void UInputManager::Update(const FAppWindow* InWindow)
{
	// 이전 프레임 상태를 현재 프레임 상태로 복사
	PreviousKeyState = CurrentKeyState;

	// 대기 중인 마우스 상태를 현재 프레임에 적용
	CurrentKeyState[EKeyInput::MouseLeft] = PendingMouseState[EKeyInput::MouseLeft];
	CurrentKeyState[EKeyInput::MouseRight] = PendingMouseState[EKeyInput::MouseRight];
	CurrentKeyState[EKeyInput::MouseMiddle] = PendingMouseState[EKeyInput::MouseMiddle];

	// 윈도우가 포커스를 잃었을 때는 입력 처리를 중단
	if (!bIsWindowFocused)
	{
		// 모든 키 상태를 false로 유지
		for (auto& Pair : CurrentKeyState)
		{
			Pair.second = false;
		}
		return;
	}

	// ImGui가 초기화되었고 Input 받는 경우 다른 시스템 입력 중단
	if (ImGui::GetCurrentContext() != nullptr)
	{
		ImGuiIO& IO = ImGui::GetIO();
		if (IO.WantCaptureKeyboard)
		{
			return;
		}
	}

	// 마우스 위치 업데이트
	UpdateMousePosition(InWindow);


	// 더블클릭 감지 업데이트
	UpdateDoubleClickDetection();

	// GetAsyncKeyState를 사용하여 현재 키 상태를 업데이트
	for (auto& Pair : VirtualKeyMap)
	{
		int32 VirtualKey = Pair.first;
		EKeyInput KeyInput = Pair.second;

		if (KeyInput == EKeyInput::MouseLeft || KeyInput == EKeyInput::MouseRight ||
			KeyInput == EKeyInput::MouseMiddle)
		{
			// 마우스 버튼은 마우스 버튼은 GetAsyncKeyState가 잘 작동하지 않을 수 있으므로 ProcessKeyMessage에서 처리
			continue;
		}

		// GetAsyncKeyState의 반환값에서 최상위 비트가 1이면 키가 눌린 상태
		bool IsKeyDown = (GetAsyncKeyState(VirtualKey) & 0x8000) != 0;
		CurrentKeyState[KeyInput] = IsKeyDown;
	}

	HandleConsoleShortcut();
}

void UInputManager::UpdateMousePosition(const FAppWindow* InWindow)
{
	PreviousMousePosition = CurrentMousePosition;

	// 윈도우가 포커스를 잃었을 때는 마우스 위치 업데이트를 중단
	if (!bIsWindowFocused)
	{
		// 마우스 델타를 0으로 리셋
		MouseDelta = FVector(0.0f, 0.0f, 0.0f);
		return;
	}

	int32 ViewportWidth;
	int32 ViewportHeight;
	InWindow->GetClientSize(ViewportWidth, ViewportHeight);

	POINT MousePoint;
	if (GetCursorPos(&MousePoint))
	{
		ScreenToClient(GetActiveWindow(), &MousePoint);
		CurrentMousePosition.X = static_cast<float>(MousePoint.x);
		CurrentMousePosition.Y = static_cast<float>(MousePoint.y);
	}

	NDCMousePosition.X = (CurrentMousePosition.X / static_cast<float>(ViewportWidth)) * 2.0f - 1.0f;
	NDCMousePosition.Y = 1.0f - (CurrentMousePosition.Y / static_cast<float>(ViewportHeight)) * 2.0f;

	MouseDelta = CurrentMousePosition - PreviousMousePosition;
}

bool UInputManager::IsKeyDown(EKeyInput InKey) const
{
	auto Iter = CurrentKeyState.find(InKey);
	if (Iter != CurrentKeyState.end())
	{
		return Iter->second;
	}
	return false;
}

bool UInputManager::IsKeyPressed(EKeyInput InKey) const
{
	auto CurrentIter = CurrentKeyState.find(InKey);
	auto PrevIter = PreviousKeyState.find(InKey);

	if (CurrentIter != CurrentKeyState.end() && PrevIter != PreviousKeyState.end())
	{
		// 이전 프레임에는 안 눌렸고, 현재 프레임에는 눌림
		return CurrentIter->second && !PrevIter->second;
	}

	return false;
}

bool UInputManager::IsKeyReleased(EKeyInput InKey) const
{
	auto CurrentIter = CurrentKeyState.find(InKey);
	auto PrevIter = PreviousKeyState.find(InKey);

	if (CurrentIter != CurrentKeyState.end() && PrevIter != PreviousKeyState.end())
	{
		// 이전 프레임에는 눌렸고, 현재 프레임에는 안 눌림
		return !CurrentIter->second && PrevIter->second;
	}
	return false;
}

void UInputManager::ProcessKeyMessage(uint32 InMessage, WPARAM WParam, LPARAM LParam)
{
	// 윈도우가 포커스를 잃었을 때는 입력 처리를 중단
	if (!bIsWindowFocused)
	{
		return;
	}

	// ImGui가 초기화되었고 마우스나 키보드를 캡처하고 있는 경우 입력 처리를 중단
	if (ImGui::GetCurrentContext() != nullptr)
	{
		ImGuiIO& IO = ImGui::GetIO();
		if (IO.WantCaptureMouse || IO.WantCaptureKeyboard)
		{
			return;
		}
	}

	switch (InMessage)
	{
	case WM_LBUTTONDOWN:
		{
			PendingMouseState[EKeyInput::MouseLeft] = true;

			// Double click
			float CurrentTime = static_cast<float>(GetTickCount64()) / 1000.0f;
			float TimeSinceLastClick = CurrentTime - LastClickTime[EKeyInput::MouseLeft];

			if (TimeSinceLastClick <= DoubleClickTime && TimeSinceLastClick > 0.01f) // 최소 0.01초 간격
			{
				++ClickCount[EKeyInput::MouseLeft];
				if (ClickCount[EKeyInput::MouseLeft] >= 2)
				{
					DoubleClickState[EKeyInput::MouseLeft] = true;
					ClickCount[EKeyInput::MouseLeft] = 0; // 리셋
				}
			}
			else
			{
				// 첫 번째 클릭 또는 시간 초과
				ClickCount[EKeyInput::MouseLeft] = 1;
				DoubleClickState[EKeyInput::MouseLeft] = false;
			}

			LastClickTime[EKeyInput::MouseLeft] = CurrentTime;
		}
		break;

	case WM_LBUTTONUP:
		PendingMouseState[EKeyInput::MouseLeft] = false;
		break;

	case WM_RBUTTONDOWN:
		{
			PendingMouseState[EKeyInput::MouseRight] = true;

			// Double click
			float CurrentTime = static_cast<float>(GetTickCount64()) / 1000.0f;
			float TimeSinceLastClick = CurrentTime - LastClickTime[EKeyInput::MouseRight];

			if (TimeSinceLastClick <= DoubleClickTime && TimeSinceLastClick > 0.01f)
			{
				++ClickCount[EKeyInput::MouseRight];
				if (ClickCount[EKeyInput::MouseRight] >= 2)
				{
					DoubleClickState[EKeyInput::MouseRight] = true;
					ClickCount[EKeyInput::MouseRight] = 0;
				}
			}
			else
			{
				// 첫 번째 클릭 또는 시간 초과
				ClickCount[EKeyInput::MouseRight] = 1;
				DoubleClickState[EKeyInput::MouseRight] = false;
			}

			LastClickTime[EKeyInput::MouseRight] = CurrentTime;
		}
		break;

	case WM_RBUTTONUP:
		PendingMouseState[EKeyInput::MouseRight] = false;
		break;

	case WM_MBUTTONDOWN:
		{
			PendingMouseState[EKeyInput::MouseMiddle] = true;

			// Double click
			float CurrentTime = static_cast<float>(GetTickCount64()) / 1000.0f;
			float TimeSinceLastClick = CurrentTime - LastClickTime[EKeyInput::MouseMiddle];

			if (TimeSinceLastClick <= DoubleClickTime && TimeSinceLastClick > 0.01f)
			{
				++ClickCount[EKeyInput::MouseMiddle];
				if (ClickCount[EKeyInput::MouseMiddle] >= 2)
				{
					DoubleClickState[EKeyInput::MouseMiddle] = true;
					ClickCount[EKeyInput::MouseMiddle] = 0;
				}
			}
			else
			{
				// 첫 번째 클릭 또는 시간 초과
				ClickCount[EKeyInput::MouseMiddle] = 1;
				DoubleClickState[EKeyInput::MouseMiddle] = false;
			}

			LastClickTime[EKeyInput::MouseMiddle] = CurrentTime;
		}
		break;

	case WM_MBUTTONUP:
		PendingMouseState[EKeyInput::MouseMiddle] = false;
		break;

	case WM_MOUSEWHEEL:
		{
			// WParam의 상위 16비트에 휠 델타 값이 들어있음
			// WHEEL_DELTA(120)로 정규화하여 -1.0f ~ 1.0f 범위로 변환
			short WheelDelta = GET_WHEEL_DELTA_WPARAM(WParam);
			MouseWheelDelta = static_cast<float>(WheelDelta) / WHEEL_DELTA;
		}
		break;

	default:
		break;
	}
}

const TArray<EKeyInput>& UInputManager::GetKeysByStatus(EKeyStatus InStatus)
{
	KeysInStatus.Empty();

	for (int32 i = 0; i < static_cast<int32>(EKeyInput::End); ++i)
	{
		EKeyInput Key = static_cast<EKeyInput>(i);
		if (GetKeyStatus(Key) == InStatus)
		{
			KeysInStatus.Add(Key);
		}
	}

	return KeysInStatus;
}

EKeyStatus UInputManager::GetKeyStatus(EKeyInput InKey) const
{
	auto CurrentIter = CurrentKeyState.find(InKey);
	auto PrevIter = PreviousKeyState.find(InKey);

	if (CurrentIter == CurrentKeyState.end() || PrevIter == PreviousKeyState.end())
	{
		return EKeyStatus::Unknown;
	}

	// Pressed -> Released -> Down -> Up
	if (CurrentIter->second && !PrevIter->second)
	{
		return EKeyStatus::Pressed;
	}
	if (!CurrentIter->second && PrevIter->second)
	{
		return EKeyStatus::Released;
	}
	if (CurrentIter->second)
	{
		return EKeyStatus::Down;
	}
	return EKeyStatus::Up;
}

const TArray<EKeyInput>& UInputManager::GetPressedKeys()
{
	return GetKeysByStatus(EKeyStatus::Down);
}

const TArray<EKeyInput>& UInputManager::GetNewlyPressedKeys()
{
	// Pressed 상태의 키들을 반환
	return GetKeysByStatus(EKeyStatus::Pressed);
}

const TArray<EKeyInput>& UInputManager::GetReleasedKeys()
{
	// Released 상태의 키들을 반환
	return GetKeysByStatus(EKeyStatus::Released);
}

FString UInputManager::KeyInputToString(EKeyInput InKey)
{
	return EnumToString<EKeyInput>(InKey);
}

void UInputManager::SetWindowFocus(bool bInFocused)
{
	bIsWindowFocused = bInFocused;

	if (!bInFocused)
	{
		for (auto& Pair : CurrentKeyState)
		{
			Pair.second = false;
		}

		for (auto& Pair : PreviousKeyState)
		{
			Pair.second = false;
		}

		// 포커스를 잃었을 때 더블클릭 상태도 리셋
		for (auto& Pair : DoubleClickState)
		{
			Pair.second = false;
		}
	}
}

/**
 * @brief 더블클릭 감지 업데이트
 * 타임아웃 기반 ClickCount 리셋 (DoubleClickState는 소비 기반)
 */
void UInputManager::UpdateDoubleClickDetection()
{
	float CurrentTime = static_cast<float>(GetTickCount64()) / 1000.0f;
	for (auto& Pair : ClickCount)
	{
		EKeyInput MouseButton = Pair.first;
		if (CurrentTime - LastClickTime[MouseButton] > DoubleClickTime)
		{
			Pair.second = 0;
		}
	}
}

void UInputManager::HandleConsoleShortcut()
{
	if (IsKeyPressed(EKeyInput::Backtick) && IsKeyDown(EKeyInput::Alt))
	{
		try
		{
			UConsoleWindow::GetInstance().ToggleConsoleVisibility();
		}
		catch (...)
		{
			// 콘솔 윈도우 초기화 이전 상태 등 예외 상황 무시
		}
	}
}

/**
 * @brief 마우스 더블클릭 감지 (소비 기반, 읽으면 즉시 리셋)
 * @param InMouseButton 확인할 마우스 버튼
 * @return 더블클릭 되었으면 true, 아니면 false
 */
bool UInputManager::IsMouseDoubleClicked(EKeyInput InMouseButton) const
{
	auto Iter = DoubleClickState.find(InMouseButton);
	if (Iter != DoubleClickState.end() && Iter->second)
	{
		const_cast<UInputManager*>(this)->DoubleClickState[InMouseButton] = false;
		return true;
	}
	return false;
}

