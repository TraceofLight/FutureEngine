#pragma once
#include "Macro.h"
#include "EnumReflection.h"

UENUM()
enum class EKeyInput : uint8
{
	// 이동 키
	W,
	A,
	S,
	D,
	Q,
	E,
	R,
	F,
	P,
	G,

	// 화살표 키
	Up,
	Down,
	Left,
	Right,

	// 액션 키
	Space,
	Enter,
	Esc,
	Tab,
	Shift,
	Ctrl,
	Alt,

	// 숫자 키
	Num0,
	Num1,
	Num2,
	Num3,
	Num4,
	Num5,
	Num6,
	Num7,
	Num8,
	Num9,

	// 마우스
	MouseLeft,
	MouseRight,
	MouseMiddle,

	// 기타
	Backtick,
	F1,
	F2,
	F3,
	F4,
	Backspace,
	Delete,

	End
};
DECLARE_UINT8_ENUM_REFLECTION(EKeyInput)

enum class EKeyStatus : uint8
{
	Up, // 누려있지 않은 상태 (현재 false)
	Pressed, // 이번 프레임에 누림 (이전 false, 현재 true)
	Down, // 누려있는 상태 (현재 true)
	Released, // 이번 프레임에 떼어짐 (이전 true, 현재 false)
	Unknown, // 알 수 없음

	End
};
DECLARE_UINT8_ENUM_REFLECTION(EKeyStatus)

/**
 * @brief 로그 타입 열거형
 * 콘솔 윈도우에서 로그의 종류와 색상을 결정하는데 사용
 */
enum class ELogType : uint8
{
	// 기본 로그 타입
	Info, // 일반 정보 (흰색)
	Warning, // 경고 (노란색)
	Error, // 에러 (빨간색)
	Success, // 성공 (초록색)

	// 시스템 로그 타입
	System, // 시스템 메시지 (회색)
	Debug, // 디버그 메시지 (파란색)

	// 사용자 정의 로그 타입
	UELog, // UE_LOG 명령어 (초록색)
	Terminal, // 터미널 명령어 (하늘색)
	TerminalError, // 터미널 에러 (빨간색)
	Command, // 사용자 입력 명령어 (주황색)

	End
};
DECLARE_UINT8_ENUM_REFLECTION(ELogType)

/**
 * @brief UObject Primitive Type Enum
 */
UENUM()
enum class EPrimitiveType : uint8
{
	None = 0,
	MovingCube,
	StaticMesh,
	Sprite,
	Text,
	Decal,
	Torus,
	Arrow,
	CubeArrow,
	Ring,
	Line,
	UUID,

	End = 0xFF
};
DECLARE_UINT8_ENUM_REFLECTION(EPrimitiveType)

/**
 * @brief RasterizerState Enum
 */
enum class ECullMode : uint8_t
{
	Back,
	Front,
	None,

	End = 0xFF
};

enum class EFillMode : uint8_t
{
	WireFrame,
	Solid,

	End = 0xFF
};

enum class EViewModeIndex : uint32
{
	VMI_Gouraud,
	VMI_Lambert,
	VMI_BlinnPhong,
	VMI_Unlit,
	VMI_Wireframe,
	VMI_SceneDepth,
	VMI_WorldNormal
};

UENUM()
enum class EShadowModeIndex : uint32
{
	SMI_UnFiltered = 0,
	SMI_PCF,
	SMI_VSM,
	SMI_VSM_BOX,
	SMI_VSM_GAUSSIAN,
	SMI_SAVSM
};
DECLARE_ENUM_REFLECTION(EShadowModeIndex)

/**
 * @brief Level Show Flag Enum
 */
enum class EEngineShowFlags : uint64
{
	SF_Billboard = 1 << 0,
	SF_Bounds = 1 << 1,
	SF_StaticMesh = 1 << 2,
	SF_Text = 1 << 3,
	SF_Decal = 1 << 4,
	SF_FXAA = 1 << 5,
	SF_Fog = 1 << 6,
	SF_Octree = 1 << 7,
	SF_UUID = 1 << 8,
	SF_Collision = 1 << 9,  // Shape component collision visualization
};

inline uint64 operator|(EEngineShowFlags lhs, EEngineShowFlags rhs)
{
	return static_cast<uint64>(lhs) | static_cast<uint64>(rhs);
}

inline uint64 operator&(uint64 lhs, EEngineShowFlags rhs)
{
	return lhs & static_cast<uint64>(rhs);
}

// Viewport layout modes
enum class EViewportLayout : uint8
{
	Single = 0,
	Quad = 1
};

enum class EViewMode : uint8
{
	Lit,
	Unlit,
	WireFrame
};

enum class EViewType : uint8
{
	Perspective,
	OrthoTop,
	OrthoBottom,
	OrthoLeft,
	OrthoRight,
	OrthoFront,
	OrthoBack
};
