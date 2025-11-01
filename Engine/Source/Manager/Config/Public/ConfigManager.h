#pragma once

/**
 * @brief 에디터 설정 파일(editor.ini) 관리
 * @details 상태를 저장 처리를 위한 순수 파일 I/O 유틸리티 클래스
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
	void SaveViewportLayoutSettings(const JSON& InViewportLayoutJson) const;
	JSON LoadViewportLayoutSettings() const;

	// 카메라 설정 저장/로드
	void SaveViewportCameraSettings(const JSON& InViewportCameraJson) const;
	JSON LoadViewportCameraSettings() const;

private:
	FName EditorConfigFileName;
};
