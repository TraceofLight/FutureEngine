#pragma once
#include "Editor/Public/GizmoTypes.h"

/**
 * @brief 에디터 설정 파일(editor.ini) 관리
 * @details 상태 저장 처리를 위한 I/O 유틸리티 클래스
 */
UCLASS()
class UConfigManager : public UObject
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(UConfigManager, UObject)

public:
	// Cell Size 저장/로드
	void SaveCellSize(float InCellSize) const;
	float LoadCellSize() const;

	// 뷰포트 레이아웃 저장/로드
	void SaveViewportLayoutSettings(const JSON& InViewportLayoutJson);
	JSON LoadViewportLayoutSettings() const;

	// 카메라 설정 저장/로드
	void SaveViewportCameraSettings(const JSON& InViewportCameraJson);
	JSON LoadViewportCameraSettings() const;

	// ========================================
	// Cached Values (저장용 복사본)
	// ========================================

	// Camera Settings
	float GetCachedEditorCameraSpeed() const { return CachedEditorCameraSpeed; }
	void SetCachedEditorCameraSpeed(float InSpeed) { CachedEditorCameraSpeed = InSpeed; }

	float GetCachedSharedOrthoZoom() const { return CachedSharedOrthoZoom; }
	void SetCachedSharedOrthoZoom(float InZoom) { CachedSharedOrthoZoom = InZoom; }

	// Rotation Snap Settings
	bool GetCachedRotationSnapEnabled() const { return bCachedRotationSnapEnabled; }
	void SetCachedRotationSnapEnabled(bool bEnabled) { bCachedRotationSnapEnabled = bEnabled; }

	float GetCachedRotationSnapAngle() const { return CachedRotationSnapAngle; }
	void SetCachedRotationSnapAngle(float InAngle) { CachedRotationSnapAngle = InAngle; }

	// Gizmo Settings
	EGizmoMode GetCachedGizmoMode() const { return CachedGizmoMode; }
	void SetCachedGizmoMode(EGizmoMode InMode) { CachedGizmoMode = InMode; }

private:
	FName EditorConfigFileName;

	// ========================================
	// Cached Config Values (저장용 복사본)
	// ========================================
	float CachedEditorCameraSpeed = 50.0f;
	float CachedSharedOrthoZoom = 500.0f;
	bool bCachedRotationSnapEnabled = true;
	float CachedRotationSnapAngle = 10.0f;
	EGizmoMode CachedGizmoMode;
};
