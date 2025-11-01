#include "pch.h"
#include "Manager/Config/Public/ConfigManager.h"

#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_SINGLETON_CLASS(UConfigManager, UObject)

UConfigManager::UConfigManager()
    : EditorConfigFileName("editor.ini")
{
}

UConfigManager::~UConfigManager() = default;

void UConfigManager::SaveCellSize(float InCellSize) const
{
    // 기존 설정 파일 로드
    JSON ConfigJson;
    FJsonSerializer::LoadJsonFromFile(ConfigJson, EditorConfigFileName.ToString());

    // CellSize 업데이트
    ConfigJson["CellSize"] = InCellSize;

    // 파일에 저장
    FJsonSerializer::SaveJsonToFile(ConfigJson, EditorConfigFileName.ToString());
}

float UConfigManager::LoadCellSize() const
{
    JSON ConfigJson;
    if (!FJsonSerializer::LoadJsonFromFile(ConfigJson, EditorConfigFileName.ToString()))
    {
        return 5.0f; // 기본값
    }

    float CellSize = 5.0f;
    FJsonSerializer::ReadFloat(ConfigJson, "CellSize", CellSize, 5.0f);
    return CellSize;
}

void UConfigManager::SaveViewportCameraSettings(const JSON& InViewportCameraJson) const
{
    // 현재 설정 파일 로드
    JSON ConfigJson;
    FJsonSerializer::LoadJsonFromFile(ConfigJson, EditorConfigFileName.ToString());

    // ViewportCameraSettings 추가
    ConfigJson["ViewportCameraSettings"] = InViewportCameraJson;

    // 파일에 저장
    FJsonSerializer::SaveJsonToFile(ConfigJson, EditorConfigFileName.ToString());
}

JSON UConfigManager::LoadViewportCameraSettings() const
{
    JSON ConfigJson;
    if (!FJsonSerializer::LoadJsonFromFile(ConfigJson, EditorConfigFileName.ToString()))
    {
        return json::Object();
    }

    JSON CameraSettings;
    if (FJsonSerializer::ReadObject(ConfigJson, "ViewportCameraSettings", CameraSettings))
    {
        return CameraSettings;
    }

    return json::Object();
}

void UConfigManager::SaveViewportLayoutSettings(const JSON& InViewportLayoutJson) const
{
    // 현재 설정 파일 로드
    JSON ConfigJson;
    FJsonSerializer::LoadJsonFromFile(ConfigJson, EditorConfigFileName.ToString());

    // ViewportLayoutSettings 추가
    ConfigJson["ViewportLayoutSettings"] = InViewportLayoutJson;

    // 파일에 저장
    FJsonSerializer::SaveJsonToFile(ConfigJson, EditorConfigFileName.ToString());
}

JSON UConfigManager::LoadViewportLayoutSettings() const
{
    JSON ConfigJson;
    if (!FJsonSerializer::LoadJsonFromFile(ConfigJson, EditorConfigFileName.ToString()))
    {
        return json::Object();
    }

    JSON LayoutSettings;
    if (FJsonSerializer::ReadObject(ConfigJson, "ViewportLayoutSettings", LayoutSettings))
    {
        return LayoutSettings;
    }

    return json::Object();
}
