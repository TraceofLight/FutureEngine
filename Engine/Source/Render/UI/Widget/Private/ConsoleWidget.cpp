#include "pch.h"
#include "Render/UI/Widget/Public/ConsoleWidget.h"

#include "Component/Public/LightComponentBase.h"
#include "Level/Public/Level.h"
#include "Manager/Render/Public/CascadeManager.h"
#include "Render/UI/Overlay/Public/StatOverlay.h"
#include "Utility/Public/UELogParser.h"
#include "Utility/Public/ScopeCycleCounter.h"
#include "Utility/Public/LogFileWriter.h"

// #define IMGUI_DEFINE_MATH_OPERATORS
// #include "ImGui/imgui_internal.h"

IMPLEMENT_SINGLETON_CLASS(UConsoleWidget, UWidget)

UConsoleWidget::UConsoleWidget()
	: LogFileWriter(nullptr)
{
}

UConsoleWidget::~UConsoleWidget()
{
	CleanupSystemRedirect();
	ClearLog();

	if (LogFileWriter)
	{
		LogFileWriter->Shutdown();
		delete LogFileWriter;
		LogFileWriter = nullptr;
	}
}

void UConsoleWidget::Initialize()
{
	// 초기화
	ClearLog();
	memset(InputBuf, 0, sizeof(InputBuf));
	HistoryPosition = -1;
	bIsAutoScroll = true;
	bIsScrollToBottom = false;
	bPendingScrollToBottom = false;
	bIsDragging = false;
	TextSelection.Clear();

	// Stream Redirection 초기화
	ConsoleOutputBuffer = nullptr;
	ConsoleErrorBuffer = nullptr;
	OriginalConsoleOutput = nullptr;
	OriginalConsoleError = nullptr;

	AddLog(ELogType::Success, "ConsoleWindow: Game Console 초기화 성공");
	AddLog(ELogType::System, "ConsoleWindow: Logging System Ready");

	// Log File Writer 초기화
	LogFileWriter = new FLogFileWriter();
	LogFileWriter->Initialize();

	if (LogFileWriter && LogFileWriter->IsInitialized())
	{
		// 초기화 성공 시 임시 버퍼의 로그들을 파일에 기록
		for (const auto& PendingLog : PendingLogs)
		{
			FString FileLog = GetLogTypePrefix(PendingLog.Type);
			FileLog += " ";
			FileLog += PendingLog.Message;
			LogFileWriter->AddLog(FileLog);
		}

		// 임시 버퍼 비우기
		PendingLogs.clear();

		AddLog(ELogType::Success, "LogFileWriter: Initialized: %s", LogFileWriter->GetCurrentLogFileName().data());
	}
	else
	{
		AddLog(ELogType::Error, "LogFileWriter: Failed to initialize");
	}
}

/**
 * @brief StreamBuffer Override 함수
 * @param InCharacter classic type char
 * @return 입력 받은 문자
 */
int ConsoleStreamBuffer::overflow(int InCharacter)
{
	if (InCharacter != EOF)
	{
		Buffer += static_cast<char>(InCharacter);
		if (InCharacter == '\n')
		{
			if (Console)
			{
				Console->AddSystemLog(Buffer.c_str(), bIsError);
			}
			Buffer.clear();
		}
	}
	return InCharacter;
}

streamsize ConsoleStreamBuffer::xsputn(const char* InString, streamsize InCount)
{
	for (std::streamsize i = 0; i < InCount; ++i)
	{
		overflow(InString[i]);
	}
	return InCount;
}

void UConsoleWidget::RenderWidget()
{
	// 버튼 색상을 검은색으로 설정
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
	
	// 제어 버튼들
	if (ImGui::Button("Clear"))
	{
		ClearLog();
		TextSelection.Clear();
		bIsDragging = false;
	}

	ImGui::SameLine();
	if (ImGui::Button("Copy"))
	{
		ImGui::LogToClipboard();
	}
	
	ImGui::PopStyleColor(3);

	// ImGui::SameLine();
	// ImGui::Checkbox("AutoScroll", &bIsAutoScroll);

	ImGui::Separator();

	// 로그 출력 영역 미리 예약
	const float ReservedHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();

	// 배경색 설정
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));

	if (ImGui::BeginChild("##LogOutput", ImVec2(0, -ReservedHeight), true, ImGuiWindowFlags_HorizontalScrollbar))
	{
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		float line_height = ImGui::GetTextLineHeight();

		// 마우스 입력 처리
		ImVec2 mouse_pos = ImGui::GetMousePos();
		bool is_window_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

		// ESC 또는 다른 곳 클릭 시 선택 해제
		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			TextSelection.Clear();
			bIsDragging = false;
		}

		// 윈도우 밖 클릭 시 선택 해제
		if (!is_window_hovered && ImGui::IsMouseClicked(0))
		{
			TextSelection.Clear();
			bIsDragging = false;
		}

		// 선택 영역 계산
		int start_line = TextSelection.IsActive() ? std::min(TextSelection.StartLine, TextSelection.EndLine) : -1;
		int end_line = TextSelection.IsActive() ? std::max(TextSelection.StartLine, TextSelection.EndLine) : -1;

		// 1단계: 로그 렌더링하면서 위치 정보 수집
		struct FRenderedLine
		{
			ImVec2 Min;
			ImVec2 Max;
			int Index;
		};
		std::vector<FRenderedLine> RenderedLines;
		RenderedLines.reserve(LogItems.size());

		int logIndex = 0;
		for (const auto& LogEntry : LogItems)
		{
			const FString& fullText = LogEntry.Message;

			// 로그 타입에 따른 색상 적용
			ImVec4 Color = GetColorByLogType(LogEntry.Type);
			bool bShouldApplyColor = (LogEntry.Type != ELogType::Info);

			if (bShouldApplyColor)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, Color);
			}

			ImVec2 line_min = ImGui::GetCursorScreenPos();
			ImGui::TextUnformatted(fullText.c_str());
			ImVec2 line_max = ImGui::GetCursorScreenPos();

			if (bShouldApplyColor)
			{
				ImGui::PopStyleColor();
			}

			// 렌더링된 라인 정보 저장
			ImVec2 text_size = ImGui::CalcTextSize(fullText.c_str());
			RenderedLines.push_back({
				line_min,
				ImVec2(line_min.x + text_size.x, line_max.y),
				logIndex
			});

			logIndex++;
		}

		// 마우스 드래그 처리
		static int ClickedLineOnPress = -1;
		static bool bWasSingleLineSelected = false;
		static int PreviouslySelectedLine = -1;

		if (is_window_hovered && ImGui::IsMouseClicked(0))
		{
			// 클릭 전 선택 상태 저장
			bWasSingleLineSelected = (TextSelection.IsActive() && TextSelection.StartLine == TextSelection.EndLine);
			PreviouslySelectedLine = bWasSingleLineSelected ? TextSelection.StartLine : -1;

			// 클릭한 라인 찾기
			ClickedLineOnPress = -1;
			for (const auto& line : RenderedLines)
			{
				if (mouse_pos.y >= line.Min.y && mouse_pos.y < line.Min.y + line_height)
				{
					ClickedLineOnPress = line.Index;
					break;
				}
			}

			// 다중 라인 선택 상태에서 클릭 시 선택 해제만
			if (TextSelection.IsActive() && !bWasSingleLineSelected)
			{
				TextSelection.Clear();
				bIsDragging = false;
			}
			// 단일 라인 또는 선택 없는 상태에서 클릭 시 새로운 선택 시작
			else if (ClickedLineOnPress >= 0)
			{
				bIsDragging = true;
				TextSelection.StartLine = ClickedLineOnPress;
				TextSelection.EndLine = ClickedLineOnPress;
			}
		}

		if (bIsDragging && ImGui::IsMouseDown(0))
		{
			// 드래그 중 - EndLine 업데이트
			for (const auto& line : RenderedLines)
			{
				if (mouse_pos.y >= line.Min.y && mouse_pos.y < line.Min.y + line_height)
				{
					TextSelection.EndLine = line.Index;
					break;
				}
			}
		}

		if (ImGui::IsMouseReleased(0))
		{
			// 단일 라인 토글: 드래그 없이 같은 라인을 다시 클릭한 경우
			if (bIsDragging && bWasSingleLineSelected &&
			    TextSelection.IsActive() &&
			    TextSelection.StartLine == TextSelection.EndLine &&
			    PreviouslySelectedLine == ClickedLineOnPress &&
			    !ImGui::IsMouseDragging(0, 1.0f))
			{
				TextSelection.Clear();
			}

			bIsDragging = false;
		}

		// 선택 영역 하이라이트
		if (TextSelection.IsActive())
		{
			const ImU32 highlight_color = IM_COL32(70, 100, 200, 128);

			if (bIsDragging)
			{
				// 드래그 중: 화면 밖 선택 영역도 표시
				ImVec2 window_pos = ImGui::GetWindowPos();
				float virtual_y = window_pos.y + ImGui::GetStyle().WindowPadding.y - ImGui::GetScrollY();

				for (int idx = 0; idx < (int)LogItems.size(); ++idx)
				{
					if (idx < start_line || idx > end_line) continue;

					// 화면에 렌더링된 라인 찾기
					bool found = false;
					for (const auto& line : RenderedLines)
					{
						if (line.Index == idx)
						{
							draw_list->AddRectFilled(line.Min, ImVec2(line.Max.x, line.Min.y + line_height), highlight_color);
							found = true;
							break;
						}
					}

					// 화면 밖 라인은 가상 위치에 표시
					if (!found)
					{
						ImVec2 sel_min = ImVec2(window_pos.x, virtual_y + idx * line_height);
						ImVec2 sel_max = ImVec2(sel_min.x + 1000.0f, sel_min.y + line_height);
						draw_list->AddRectFilled(sel_min, sel_max, highlight_color);
					}
				}
			}
			else
			{
				// 드래그 완료: 화면에 보이는 것만 하이라이트
				for (const auto& line : RenderedLines)
				{
					if (line.Index >= start_line && line.Index <= end_line)
					{
						draw_list->AddRectFilled(line.Min, ImVec2(line.Max.x, line.Min.y + line_height), highlight_color);
					}
				}
			}
		}

		// Ctrl+C로 복사
		if (TextSelection.IsActive() && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_C))
		{
			FString selected_text;
			int start = std::min(TextSelection.StartLine, TextSelection.EndLine);
			int end = std::max(TextSelection.StartLine, TextSelection.EndLine);

			if (start >= 0 && end >= 0 && start < (int)LogItems.size() && end < (int)LogItems.size())
			{
				int idx = 0;
				for (const auto& LogEntry : LogItems)
				{
					if (idx >= start && idx <= end)
					{
						selected_text += LogEntry.Message + "\n";
					}
					idx++;
				}

				ImGui::SetClipboardText(selected_text.c_str());
			}
			// 복사 후 선택 유지
		}

		// Auto Scroll 처리
		if (bIsScrollToBottom || bPendingScrollToBottom)
		{
			ImGui::SetScrollHereY(1.0f);
			bIsScrollToBottom = false;
			bPendingScrollToBottom = false;
		}
	}

	ImGui::EndChild();
	ImGui::PopStyleColor(2);

	ImGui::Separator();

	// Input Command with History Navigation
	bool ReclaimFocus = false;
	ImGuiInputTextFlags InputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll |
		ImGuiInputTextFlags_CallbackHistory;
	
	// 입력 필드 색상을 검은색으로 설정
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
	
	if (ImGui::InputText("Input", InputBuf, sizeof(InputBuf), InputFlags,
	                     [](ImGuiInputTextCallbackData* data) -> int
	                     {
		                     return static_cast<UConsoleWidget*>(data->UserData)->HandleHistoryCallback(data);
	                     }, this))
	{
		if (strlen(InputBuf) > 0)
		{
			ProcessCommand(InputBuf);
			strcpy_s(InputBuf, sizeof(InputBuf), "");
			ReclaimFocus = true;
			// 새로운 명령어 입력 후 히스토리 위치 초기화
			HistoryPosition = -1;
		}
	}
	
	ImGui::PopStyleColor(3);

	// 입력 포커스 설정
	ImGui::SetItemDefaultFocus();
	if (ReclaimFocus)
	{
		ImGui::SetKeyboardFocusHere(-1);
	}
}

void UConsoleWidget::Update()
{
	// 필요한 경우 여기에 업데이트 로직 추가할 것
	// ImGui 위주라서 필요하지 않을 것으로 보이긴 함...
}

void UConsoleWidget::ClearLog()
{
	LogItems.clear();
}

/**
 * @brief ELogType에 따른 색상 반환
 */
ImVec4 UConsoleWidget::GetColorByLogType(ELogType InType)
{
	switch (InType)
	{
	case ELogType::Info:
		return {1.0f, 1.0f, 1.0f, 1.0f}; // 흰색
	case ELogType::Warning:
		return {1.0f, 1.0f, 0.4f, 1.0f}; // 노란색
	case ELogType::Error:
	case ELogType::TerminalError:
		return {1.0f, 0.4f, 0.4f, 1.0f}; // 빨간색
	case ELogType::Success:
	case ELogType::UELog:
		return {0.4f, 1.0f, 0.4f, 1.0f}; // 초록색
	case ELogType::System:
		return {0.7f, 0.7f, 0.7f, 1.0f}; // 회색
	case ELogType::Debug:
		return {0.4f, 0.6f, 1.0f, 1.0f}; // 파란색
	case ELogType::Terminal:
		return {0.6f, 0.8f, 1.0f, 1.0f}; // 하늘색
	case ELogType::Command:
		return {1.0f, 0.8f, 0.6f, 1.0f}; // 주황색
	default:
		return {1.0f, 1.0f, 1.0f, 1.0f}; // 기본 흰색
	}
}

/**
 * @brief ELogType에 따른 프리픽스 문자열 반환
 */
const char* UConsoleWidget::GetLogTypePrefix(ELogType InType)
{
	switch (InType)
	{
	case ELogType::Info:
		return "[INFO]";
	case ELogType::Warning:
		return "[WARN]";
	case ELogType::Error:
		return "[ERRO]";
	case ELogType::TerminalError:
		return "[TERR]";
	case ELogType::Success:
		return "[SUCC]";
	case ELogType::UELog:
		return "[ULOG]";
	case ELogType::System:
		return "[SYST]";
	case ELogType::Debug:
		return "[DBUG]";
	case ELogType::Terminal:
		return "[TERM]";
	case ELogType::Command:
		return "[COMM]";
	default:
		return "[INFO]";
	}
}

/**
 * @brief 타입 없는 로그 작성 함수
 * 따로 LogType을 정의하지 않았다면, Info 타입의 로그로 추가한다
 * LogType을 뒤에 놓지 않는다면 기본값 설정이 불가능한데 다중 인자를 받고 있어 기본형을 이런 방식으로 처리
 */
void UConsoleWidget::AddLog(const char* fmt, ...)
{
	va_list args;

	// 가변 인자를 파라미터로 받는 Internal 함수 호출
	va_start(args, fmt);
	AddLogInternal(ELogType::Info, fmt, args);
	va_end(args);
}

/**
 * @brief 타입을 추가한 로그 작성 함수
 * 내부적으로 타입 없는 로그 함수와 동일하게 Internal 함수를 호출한다
 */
void UConsoleWidget::AddLog(ELogType InType, const char* fmt, ...)
{
	va_list args;

	// 가변 인자를 파라미터로 받는 Internal 함수 호출
	va_start(args, fmt);
	AddLogInternal(InType, fmt, args);
	va_end(args);
}

/**
 * @brief 로그를 내부적으로 처리하는 함수
 * 로그가 잘리는 현상을 방지하기 위해 동적 버퍼를 활용하여 로그를 입력 받음
 */
void UConsoleWidget::AddLogInternal(ELogType InType, const char* fmt, va_list InArguments)
{
	va_list ArgumentsCopy;

	// Get log length
	va_copy(ArgumentsCopy, InArguments);
	int LogLength = vsnprintf(nullptr, 0, fmt, ArgumentsCopy);
	va_end(ArgumentsCopy);

	// 필요한 크기만큼 동적 할당
	// malloc 대신 overloading 함수의 영향을 받을 수 있도록 new 할당 사용
	char* Buffer = new char[LogLength + 1];

	// Make full string
	va_copy(ArgumentsCopy, InArguments);
	(void)vsnprintf(Buffer, LogLength + 1, fmt, ArgumentsCopy);
	va_end(ArgumentsCopy);

	// 기본 Info 타입으로 log 추가
	FLogEntry LogEntry;
	LogEntry.Type = InType;

	// Log buffer 복사 후 제거
	LogEntry.Message = FString(Buffer);

	// 파일에 로그 작성 또는 임시 버퍼에 저장
	if (LogFileWriter && LogFileWriter->IsInitialized())
	{
		// LogFileWriter가 초기화되었으면 파일에 작성
		FString FileLog = GetLogTypePrefix(InType);
		FileLog += " ";
		FileLog += Buffer;
		LogFileWriter->AddLog(FileLog);
	}
	else
	{
		// 아직 초기화되지 않았으면 임시 버퍼에 저장
		PendingLogs.push_back(LogEntry);
	}

	delete[] Buffer;

	// 200개 초과 시 가장 오래된 로그 제거
	if (LogItems.size() >= 200)
	{
		LogItems.pop_front();
	}

	LogItems.push_back(LogEntry);

	// Auto Scroll
	bIsScrollToBottom = true;
}

/**
 * @brief 시스템 로그들을 처리하기 위한 멤버 함수
 * @param InText log text
 * @param bInIsError 에러 여부
 */
void UConsoleWidget::AddSystemLog(const char* InText, bool bInIsError)
{
	if (!InText || strlen(InText) == 0)
	{
		return;
	}

	FLogEntry LogEntry;

	if (bInIsError)
	{
		LogEntry.Message = "Error: " + FString(InText);
		LogEntry.Type = ELogType::Error;
	}
	else
	{
		LogEntry.Message = "System: " + FString(InText);
		LogEntry.Type = ELogType::System;
	}

	// 끝에 있는 개행 문자 제거
	if (!LogEntry.Message.empty() && LogEntry.Message.back() == '\n')
	{
		LogEntry.Message.pop_back();
	}

	// 파일에 로그 작성 또는 임시 버퍼에 저장
	if (LogFileWriter && LogFileWriter->IsInitialized())
	{
		// LogFileWriter가 초기화되었으면 파일에 작성
		FString FileLog = GetLogTypePrefix(LogEntry.Type);
		FileLog += " ";
		FileLog += LogEntry.Message;
		LogFileWriter->AddLog(FileLog);
	}
	else
	{
		// 아직 초기화되지 않았으면 임시 버퍼에 저장
		PendingLogs.push_back(LogEntry);
	}

	// 200개 초과 시 가장 오래된 로그 제거
	if (LogItems.size() >= 200)
	{
		LogItems.pop_front();
	}

	LogItems.push_back(LogEntry);

	// Auto Scroll
	bIsScrollToBottom = true;
}

/**
 * @brief 명령어 히스토리 탐색 콜백 함수
 * @param InData ImGui InputText 콜백 데이터
 * @return 콜백 처리 결과 (0: 성공)
 */
int UConsoleWidget::HandleHistoryCallback(ImGuiInputTextCallbackData* InData)
{
	switch (InData->EventFlag)
	{
	case ImGuiInputTextFlags_CallbackHistory:
		{
			// 비어있는 히스토리는 처리하지 않음
			if (CommandHistory.IsEmpty())
			{
				return 0;
			}

			const int HistorySize = CommandHistory.Num();
			int PreviousHistoryPos = HistoryPosition;

			if (InData->EventKey == ImGuiKey_UpArrow)
			{
				// 위 화살표: 이전 명령어로 이동
				if (HistoryPosition == -1)
				{
					// 처음에는 가장 최근 명령어로 이동
					HistoryPosition = HistorySize - 1;
				}
				else if (HistoryPosition > 0)
				{
					// 더 이전 명령어로 이동
					--HistoryPosition;
				}
			}
			else if (InData->EventKey == ImGuiKey_DownArrow)
			{
				// 아래 화살표: 다음 명령어로 이동
				if (HistoryPosition != -1)
				{
					++HistoryPosition;
					if (HistoryPosition >= HistorySize)
					{
						// 범위를 벗어나면 입력창을 비우고 현재 상태로 돌아감
						HistoryPosition = -1;
					}
				}
			}

			// 히스토리 위치가 변경되었을 때만 입력창 업데이트
			if (PreviousHistoryPos != HistoryPosition)
			{
				if (HistoryPosition >= 0 && HistoryPosition < HistorySize)
				{
					// 선택된 히스토리 명령어로 입력창 채우기
					const FString& SelectedCommand = CommandHistory[HistoryPosition];
					InData->DeleteChars(0, InData->BufTextLen);
					InData->InsertChars(0, SelectedCommand.c_str());
				}
				else
				{
					// HistoryPosition == -1: 입력창 비우기
					InData->DeleteChars(0, InData->BufTextLen);
				}
			}
		}
		break;

	default:
		break;
	}

	return 0;
}

void UConsoleWidget::ProcessCommand(const char* InCommand)
{
	if (!InCommand || strlen(InCommand) == 0)
	{
		return;
	}

	// 명령 히스토리에 추가
	CommandHistory.Add(FString(InCommand));
	HistoryPosition = -1;

	FString Input = InCommand;

	// UE_Log Parsing
	size_t StartPosition = Input.find("UE_LOG(");
	if (StartPosition != FString::npos && StartPosition == 0)
	{
		try
		{
			UELogParser::ParseResult Result = ParseUELogFromString(Input);

			if (Result.bSuccess)
			{
				// 파싱 성공
				FLogEntry LogEntry;
				LogEntry.Type = ELogType::UELog;
				LogEntry.Message = FString(Result.FormattedMessage);

				// deque 크기 제한
				if (LogItems.size() >= 200)
				{
					LogItems.pop_front();
				}

				LogItems.push_back(LogEntry);
				bIsScrollToBottom = true;
			}
			else
			{
				// 파싱 실패
				FLogEntry ErrorEntry;
				ErrorEntry.Type = ELogType::Error;
				ErrorEntry.Message = "UELogParser: UE_LOG 파싱 오류: " + FString(Result.ErrorMessage);

				// deque 크기 제한
				if (LogItems.size() >= 200)
				{
					LogItems.pop_front();
				}

				LogItems.push_back(ErrorEntry);
				bIsScrollToBottom = true;
			}
		}
		catch (const std::exception& e)
		{
			FLogEntry ErrorEntry;
			ErrorEntry.Type = ELogType::Error;
			ErrorEntry.Message = "UELogParser: 예외 발생: " + FString(e.what());

			// deque 크기 제한
			if (LogItems.size() >= 200)
			{
				LogItems.pop_front();
			}

			LogItems.push_back(ErrorEntry);
			bIsScrollToBottom = true;
		}
		catch (...)
		{
			FLogEntry ErrorEntry;
			ErrorEntry.Type = ELogType::Error;
			ErrorEntry.Message = "UELogParser: 알 수 없는 오류가 발생했습니다.";

			// deque 크기 제한
			if (LogItems.size() >= 200)
			{
				LogItems.pop_front();
			}

			LogItems.push_back(ErrorEntry);
			bIsScrollToBottom = true;
		}
	}

	// Clear 명령어 입력
	else if (FString CommandLower = InCommand;
		std::transform(CommandLower.begin(), CommandLower.end(), CommandLower.begin(), ::tolower),
		CommandLower == "clear")
	{
		ClearLog();
	}

	// Stat 명령어 처리
	else if (FString CommandLower = InCommand;
		std::transform(CommandLower.begin(), CommandLower.end(), CommandLower.begin(), ::tolower),
		CommandLower.length() > 5 && CommandLower.substr(0, 5) == "stat ")
	{
		FString StatCommand = CommandLower.substr(5);
		HandleStatCommand(StatCommand);
	}

	// shadow_filter 명령어 처리
	else if (FString CommandLower = InCommand;
		std::transform(CommandLower.begin(), CommandLower.end(), CommandLower.begin(), ::tolower),
		CommandLower.length() > 14 && CommandLower.substr(0, 14) == "shadow_filter ")
	{
		FString FilterName = CommandLower.substr(14);
		std::transform(FilterName.begin(), FilterName.end(), FilterName.begin(), ::tolower);

		EShadowModeIndex NewMode = EShadowModeIndex::SMI_UnFiltered;
		bool bValidFilter = false;

		if (FilterName == "vsm")
		{
			NewMode = EShadowModeIndex::SMI_VSM;
			bValidFilter = true;
		}
		else if (FilterName == "pcf")
		{
			NewMode = EShadowModeIndex::SMI_PCF;
			bValidFilter = true;
		}
		else if (FilterName == "unfiltered" || FilterName == "none")
		{
			NewMode = EShadowModeIndex::SMI_UnFiltered;
			bValidFilter = true;
		}
		else if (FilterName == "vsm_box")
		{
			NewMode = EShadowModeIndex::SMI_VSM_BOX;
			bValidFilter = true;
		}
		else if (FilterName == "vsm_gaussian")
		{
			NewMode = EShadowModeIndex::SMI_VSM_GAUSSIAN;
			bValidFilter = true;
		}
		else if (FilterName == "savsm")
		{
			NewMode = EShadowModeIndex::SMI_SAVSM;
			bValidFilter = true;
		}

		if (bValidFilter)
		{
			// 전체 Light Component에 일괄 적용
			ULevel* CurrentLevel = GWorld->GetLevel();
			if (CurrentLevel)
			{
				int32 LightCount = 0;
				for (AActor* Actor : CurrentLevel->GetLevelActors())
				{
					if (!Actor)
					{
						continue;
					}

					for (UActorComponent* Component : Actor->GetOwnedComponents())
					{
						if (ULightComponentBase* LightComp = Cast<ULightComponentBase>(Component))
						{
							LightComp->SetShadowModeIndex(NewMode);
							LightCount++;
						}
					}
				}

				const char* FilterNameStr = FilterName.data();
				AddLog(ELogType::Success, "Shadow filter '%s' applied to %d light(s)", FilterNameStr, LightCount);
			}
		}
		else
		{
			AddLog(ELogType::Error, "Invalid shadow filter: %s", FilterName.data());
			AddLog(ELogType::Info, "Available filters: VSM, PCF, UnFiltered, VSM_BOX, VSM_GAUSSIAN, SAVSM");
		}
	}

	// shadow.csm 명령어 처리
	else if (FString CommandLower = InCommand;
		std::transform(CommandLower.begin(), CommandLower.end(), CommandLower.begin(), ::tolower),
		CommandLower.length() > 11 && CommandLower.substr(0, 11) == "shadow.csm.")
	{
		FString SubCommand = CommandLower.substr(11);

		// shadow.csm.numcascades <value>
		if (SubCommand.length() > 13 && SubCommand.substr(0, 13) == "numcascades ")
		{
			FString ValueStr = SubCommand.substr(13);
			try
			{
				int32 NewSplitNum = std::stoi(ValueStr);
				UCascadeManager& CascadeMgr = UCascadeManager::GetInstance();

				if (NewSplitNum >= UCascadeManager::SPLIT_NUM_MIN && NewSplitNum <= UCascadeManager::SPLIT_NUM_MAX)
				{
					CascadeMgr.SetSplitNum(NewSplitNum);
					AddLog(ELogType::Success, "Cascade split number set to %d", NewSplitNum);
				}
				else
				{
					AddLog(ELogType::Error, "Invalid split number. Valid range: %d ~ %d",
						UCascadeManager::SPLIT_NUM_MIN, UCascadeManager::SPLIT_NUM_MAX);
				}
			}
			catch (...)
			{
				AddLog(ELogType::Error, "Invalid number format: %s", ValueStr.data());
			}
		}
		// shadow.csm.distribution <value>
		else if (SubCommand.length() > 13 && SubCommand.substr(0, 13) == "distribution ")
		{
			FString ValueStr = SubCommand.substr(13);
			try
			{
				float NewBlendFactor = std::stof(ValueStr);
				UCascadeManager& CascadeMgr = UCascadeManager::GetInstance();

				if (NewBlendFactor >= UCascadeManager::SPLIT_BLEND_FACTOR_MIN &&
					NewBlendFactor <= UCascadeManager::SPLIT_BLEND_FACTOR_MAX)
				{
					CascadeMgr.SetSplitBlendFactor(NewBlendFactor);
					AddLog(ELogType::Success, "Cascade distribution factor set to %.2f", NewBlendFactor);
				}
				else
				{
					AddLog(ELogType::Error, "Invalid distribution factor. Valid range: %.1f ~ %.1f",
						UCascadeManager::SPLIT_BLEND_FACTOR_MIN, UCascadeManager::SPLIT_BLEND_FACTOR_MAX);
				}
			}
			catch (...)
			{
				AddLog(ELogType::Error, "Invalid number format: %s", ValueStr.data());
			}
		}
		// shadow.csm.nearbias <value>
		else if (SubCommand.length() > 10 && SubCommand.substr(0, 10) == "nearbias ")
		{
			FString ValueStr = SubCommand.substr(10);
			try
			{
				float NewNearBias = std::stof(ValueStr);
				UCascadeManager& CascadeMgr = UCascadeManager::GetInstance();

				if (NewNearBias >= UCascadeManager::LIGHT_VIEW_VOLUME_ZNEAR_BIAS_MIN &&
					NewNearBias <= UCascadeManager::LIGHT_VIEW_VOLUME_ZNEAR_BIAS_MAX)
				{
					CascadeMgr.SetLightViewVolumeZNearBias(NewNearBias);
					AddLog(ELogType::Success, "Cascade near plane bias set to %.1f", NewNearBias);
				}
				else
				{
					AddLog(ELogType::Error, "Invalid near bias. Valid range: %.1f ~ %.1f",
						UCascadeManager::LIGHT_VIEW_VOLUME_ZNEAR_BIAS_MIN,
						UCascadeManager::LIGHT_VIEW_VOLUME_ZNEAR_BIAS_MAX);
				}
			}
			catch (...)
			{
				AddLog(ELogType::Error, "Invalid number format: %s", ValueStr.data());
			}
		}
		else
		{
			AddLog(ELogType::Error, "Unknown CSM command: %s", SubCommand.data());
			AddLog(ELogType::Info, "Available CSM commands:");
			AddLog(ELogType::Info, "  shadow.csm.numcascades <1-8>");
			AddLog(ELogType::Info, "  shadow.csm.distribution <0.0-1.0>");
			AddLog(ELogType::Info, "  shadow.csm.nearbias <0.0-1000.0>");
		}
	}

	// Help 명령어 입력
	else if (FString CommandLower = InCommand;
		std::transform(CommandLower.begin(), CommandLower.end(), CommandLower.begin(), ::tolower),
		CommandLower == "help")
	{
		AddLog(ELogType::System, "Available Commands:");
		AddLog(ELogType::Info, "  CLEAR - Clear The Console");
		AddLog(ELogType::Info, "  HELP - Show This Help");
		AddLog(ELogType::Info, "  STAT FPS - Show FPS overlay");
		AddLog(ELogType::Info, "  STAT MEMORY - Show memory overlay");
		AddLog(ELogType::Info, "  STAT PICK - Show picking performance overlay");
		AddLog(ELogType::Info, "  STAT SHADOW - Show light and shadow map stats");
		AddLog(ELogType::Info, "  STAT NONE - Hide all overlays");
		AddLog(ELogType::Info, "  SHADOW_FILTER <filter> - Apply shadow filter to all lights");
		AddLog(ELogType::Debug, "    Available filters: VSM, PCF, UnFiltered, VSM_BOX, VSM_GAUSSIAN, SAVSM");
		AddLog(ELogType::Debug, "    Example: shadow_filter VSM");
		AddLog(ELogType::Info, "  SHADOW.CSM.NUMCASCADES <1-8> - Set cascade split number");
		AddLog(ELogType::Info, "  SHADOW.CSM.DISTRIBUTION <0.0-1.0> - Set cascade distribution factor");
		AddLog(ELogType::Info, "  SHADOW.CSM.NEARBIAS <0.0-1000.0> - Set cascade near plane bias");
		AddLog(ELogType::Info, "  UE_LOG(\"String with format\", Args...) - Enhanced printf Formatting");
		AddLog(ELogType::Debug, "    기본 예제: UE_LOG(\"Hello World %%d\", 2025)");
		AddLog(ELogType::Debug, "    문자열: UE_LOG(\"User: %%s\", \"John\")");
		AddLog(ELogType::Debug, "    혼합형: UE_LOG(\"Player %%s has %%d points\", \"Alice\", 1500)");
		AddLog(ELogType::Debug, "    다중 인자: UE_LOG(\"Score: %%d, Lives: %%d\", 2500, 3)");
		AddLog(ELogType::Info, "");
		AddLog(ELogType::System, "Camera Controls:");
		AddLog(ELogType::Info, "  우클릭 + WASD - 카메라 이동");
		AddLog(ELogType::Info, "  우클릭 + Q/E - 위/아래 이동");
		AddLog(ELogType::Info, "  우클릭 + 마우스 이동 - 카메라 회전");
		AddLog(ELogType::Success, "  우클릭 + 마우스 휠 - 이동속도 조절 (20 ~ 50)");
		AddLog(ELogType::Info, "");
		AddLog(ELogType::System, "Terminal Commands:");
		AddLog(ELogType::Info, "  Any Windows command will be executed directly");
	}
	else
	{
		// 실제 터미널 명령어 실행
		ExecuteTerminalCommand(InCommand);
	}

	// 스크롤 하단으로 이동
	bIsScrollToBottom = true;
}

void UConsoleWidget::HandleStatCommand(const FString& StatCommand)
{
	auto& StatOverlay = UStatOverlay::GetInstance();

	if (StatCommand == "fps")
	{
		StatOverlay.ShowFPS();
		AddLog(ELogType::Success, "FPS overlay enabled");
	}
	else if (StatCommand == "memory")
	{
		StatOverlay.ShowMemory();
		AddLog(ELogType::Success, "Memory overlay enabled");
	}
	else if (StatCommand == "pick" || StatCommand == "picking")
	{
		StatOverlay.ShowPicking();
		AddLog(ELogType::Success, "Picking overlay enabled");
	}
	else if (StatCommand == "time")
	{
		StatOverlay.ShowTime();
		AddLog(ELogType::Success, "Time overlay enabled");
	}
	else if (StatCommand == "decal")
	{
		StatOverlay.ShowDecal();
		AddLog(ELogType::Success, "Decal overlay enabled");
	}
	else if (StatCommand == "shadow")
	{
		StatOverlay.ShowShadow();
		AddLog(ELogType::Success, "Shadow overlay enabled");
	}
	else if (StatCommand == "all")
	{
		StatOverlay.ShowAll();
		AddLog(ELogType::Success, "All overlays enabled");
	}
	else if (StatCommand == "none")
	{
		StatOverlay.HideAll();
		AddLog(ELogType::Success, "All overlays disabled");
	}
	else
	{
		AddLog(ELogType::Error, "Unknown stat command: %s", StatCommand.data());
		AddLog(ELogType::Info, "Available: fps, memory, pick, time, decal, shadow, all, none");
	}
}

/**
 * @brief 실제 터미널 명령어를 실행하고 결과를 콘솔에 표시하는 함수
 * @param InCommand 실행할 터미널 명령어
 */
void UConsoleWidget::ExecuteTerminalCommand(const char* InCommand)
{
	if (!InCommand || strlen(InCommand) == 0)
	{
		return;
	}

	AddLog(ELogType::UELog, ("Terminal: Execute Command: " + FString(InCommand)).c_str());

	try
	{
		FString FullCommand = "powershell /c " + FString(InCommand);

		// Prepare Pipe for Create Process
		HANDLE PipeReadHandle, PipeWriteHandle;

		SECURITY_ATTRIBUTES SecurityAttribute = {};
		SecurityAttribute.nLength = sizeof(SECURITY_ATTRIBUTES);
		SecurityAttribute.bInheritHandle = TRUE;
		SecurityAttribute.lpSecurityDescriptor = nullptr;

		if (!CreatePipe(&PipeReadHandle, &PipeWriteHandle, &SecurityAttribute, 0))
		{
			AddLog("Terminal: Error: Failed To Create Pipe For Command Execution.");
			return;
		}

		// Set STARTUPINFO
		STARTUPINFOA StartUpInfo = {};
		StartUpInfo.cb = sizeof(STARTUPINFOA);
		StartUpInfo.dwFlags |= STARTF_USESTDHANDLES;
		StartUpInfo.hStdInput = nullptr;

		// stderr & stdout을 동일 파이프에서 처리
		StartUpInfo.hStdError = PipeWriteHandle;
		StartUpInfo.hStdOutput = PipeWriteHandle;

		PROCESS_INFORMATION ProcessInformation = {};

		// lpCommandLine Param Setting
		TArray<char> CommandLine;
		CommandLine.Append(FullCommand.data(), FullCommand.size());
		CommandLine.Add('\0');

		// Create Process
		if (!CreateProcessA(nullptr, CommandLine.GetData(), nullptr,
		                    nullptr, TRUE, CREATE_NO_WINDOW,
		                    nullptr, nullptr, &StartUpInfo, &ProcessInformation))
		{
			AddLog("ConsoleWindow: Error: Failed To Execute Command: %s", InCommand);
			CloseHandle(PipeReadHandle);
			CloseHandle(PipeWriteHandle);
			return;
		}

		// 부모 프로세스에서는 쓰기 핸들이 불필요하므로 바로 닫도록 처리
		CloseHandle(PipeWriteHandle);

		// Read Execution Process With Timeout
		char Buffer[256];
		DWORD ReadDoubleWord;
		FString Output;
		bool bHasOutput = false;

		DWORD StartTime = GetTickCount();
		const DWORD TimeoutMsec = 2000;
		while (true)
		{
			// 타임아웃 체크
			if (GetTickCount() - StartTime > TimeoutMsec)
			{
				// 프로세스가 아직 실행 중이면 강제 종료
				DWORD ProcessExitCode;
				if (GetExitCodeProcess(ProcessInformation.hProcess, &ProcessExitCode) && ProcessExitCode ==
					STILL_ACTIVE)
				{
					TerminateProcess(ProcessInformation.hProcess, 1);
				}
				AddLog("ConsoleWindow: Error: Reading Timeout");
				break;
			}

			// 프로세스가 아직 실행 중인지 확인
			DWORD ProcessExitCode;
			if (GetExitCodeProcess(ProcessInformation.hProcess, &ProcessExitCode) && ProcessExitCode != STILL_ACTIVE)
			{
				// 프로세스가 종료되었으면 남은 모든 데이터를 읽고 종료
				DWORD AvailableBytes = 0;
				while (PeekNamedPipe(PipeReadHandle, nullptr, 0, nullptr, &AvailableBytes, nullptr) && AvailableBytes >
					0)
				{
					if (ReadFile(PipeReadHandle, Buffer, sizeof(Buffer) - 1,
					             &ReadDoubleWord, nullptr) && ReadDoubleWord > 0)
					{
						Buffer[ReadDoubleWord] = '\0';
						Output += Buffer;
						bHasOutput = true;
					}
					else
					{
						break; // ReadFile 실패 시 종료
					}
				}
				break;
			}

			// 파이프에 데이터가 있는지 확인
			DWORD AvailableBytes = 0;
			if (!PeekNamedPipe(PipeReadHandle, nullptr, 0, nullptr, &AvailableBytes, nullptr) || AvailableBytes == 0)
			{
				// 데이터가 없으면 잠시 대기
				Sleep(100);
				continue;
			}

			// 데이터 읽기
			if (ReadFile(PipeReadHandle, Buffer, sizeof(Buffer) - 1, &ReadDoubleWord, nullptr) && ReadDoubleWord > 0)
			{
				Buffer[ReadDoubleWord] = '\0'; // Null 문자로 String 끝 세팅
				Output += Buffer;
				bHasOutput = true;
				StartTime = GetTickCount();
			}
			else
			{
				// ReadFile 실패시 종료
				break;
			}
		}

		// Process 종료 대기
		DWORD WaitResult = WaitForSingleObject(ProcessInformation.hProcess, 2000);

		// 종료 코드 확인
		DWORD ExitCode = 0;
		if (WaitResult == WAIT_TIMEOUT)
		{
			// 타임아웃 발생 시 프로세스 강제 종료
			TerminateProcess(ProcessInformation.hProcess, 1);
			AddLog("ConsoleWindow: Error: Command Timeout & Terminated");
			ExitCode = 1;
		}
		else
		{
			GetExitCodeProcess(ProcessInformation.hProcess, &ExitCode);
		}

		// Release Handle
		CloseHandle(PipeReadHandle);
		CloseHandle(ProcessInformation.hProcess);
		CloseHandle(ProcessInformation.hThread);

		// Print Result
		if (bHasOutput)
		{
			FString Utf8Output = ConvertCP949ToUTF8(Output.c_str());

			std::istringstream Stream(Utf8Output);
			FString Line;
			while (std::getline(Stream, Line))
			{
				// istringstream이 \r을 남길 수 있으므로 제거
				if (!Line.empty() && Line.back() == '\r')
				{
					Line.pop_back();
				}
				if (!Line.empty())
				{
					AddLog("%s", Line.c_str());
				}
			}
		}
		else if (ExitCode == 0)
		{
			// 출력은 없지만 성공적으로 실행된 경우
			AddLog("Terminal: Command 실행 성공 (No Output)");
		}

		// 에러 코드가 있는 경우 표시
		if (ExitCode != 0)
		{
			AddLog("Terminal: Error: Command failed with exit code: %d", ExitCode);
		}
	}
	catch (const exception& Exception)
	{
		AddLog("Terminal: Error: Exception occurred: %s", Exception.what());
	}
	catch (...)
	{
		AddLog("Terminal: Error: Unknown Error Occurred While Executing Command");
	}
}

/**
 * @brief System output redirection implementation
 */
void UConsoleWidget::InitializeSystemRedirect()
{
	try
	{
		// 유효성 체크
		if (OriginalConsoleOutput != nullptr || OriginalConsoleError != nullptr)
		{
			return;
		}

		if (!cout.rdbuf() || !cerr.rdbuf())
		{
			AddLog(ELogType::Error, "ConsoleWindow: cout/cerr streams are not available");
			return;
		}

		// Save Original Stream Buffers
		OriginalConsoleOutput = cout.rdbuf();
		OriginalConsoleError = cerr.rdbuf();

		// Create Custom Stream Buffers
		ConsoleOutputBuffer = new ConsoleStreamBuffer(this, false);
		ConsoleErrorBuffer = new ConsoleStreamBuffer(this, true);

		// Redirect ConsoleOutput and ConsoleError To Our Custom Buffers
		cout.rdbuf(ConsoleOutputBuffer);
		cerr.rdbuf(ConsoleErrorBuffer);

		AddLog(ELogType::System, "ConsoleWindow: Console Output Redirection Initialized");
	}
	catch (const exception& Exception)
	{
		AddLog(ELogType::Error, "ConsoleWindow: Failed To Initialize Console Output Redirection: %s", Exception.what());
	}
	catch (...)
	{
		AddLog(ELogType::Error, "ConsoleWindow: Failed To Initialize Console Output Redirection: Unknown Error");
	}
}

void UConsoleWidget::CleanupSystemRedirect()
{
	try
	{
		// Restore Original Stream Buffers
		if (OriginalConsoleOutput)
		{
			cout.rdbuf(OriginalConsoleOutput);
			OriginalConsoleOutput = nullptr;
		}

		if (OriginalConsoleError)
		{
			cerr.rdbuf(OriginalConsoleError);
			OriginalConsoleError = nullptr;
		}

		// Delete Custom Stream Buffers
		if (ConsoleOutputBuffer)
		{
			delete ConsoleOutputBuffer;
			ConsoleOutputBuffer = nullptr;
		}

		if (ConsoleErrorBuffer)
		{
			delete ConsoleErrorBuffer;
			ConsoleErrorBuffer = nullptr;
		}
	}
	catch (...)
	{
		// Ignore Cleanup Errors
	}
}

void UConsoleWidget::OnConsoleShown()
{
	bPendingScrollToBottom = true;
}

void UConsoleWidget::ClearSelection()
{
	TextSelection.Clear();
	bIsDragging = false;
}
