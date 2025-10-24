#include "pch.h"

#include "Component/Public/LightComponentBase.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_ABSTRACT_CLASS(ULightComponentBase, USceneComponent)

void ULightComponentBase::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
    if (bInIsLoading)
    {
        float LoadedIntensity = Intensity;
        FVector LoadedColor = LightColor;
        FString VisibleString;
        FString LightEnabledString;
        FString CastShadowsString;
        FJsonSerializer::ReadFloat(InOutHandle, "Intensity", LoadedIntensity);
        FJsonSerializer::ReadVector(InOutHandle, "LightColor", LoadedColor);
        FJsonSerializer::ReadString(InOutHandle, "bVisible", VisibleString, "true");
        FJsonSerializer::ReadString(InOutHandle, "bLightEnabled", LightEnabledString, "true");
        FJsonSerializer::ReadString(InOutHandle, "bCastShadows", CastShadowsString, "true");
        SetIntensity(LoadedIntensity);
        SetLightColor(LoadedColor);
        SetVisible(VisibleString == "true");
        SetLightEnabled(LightEnabledString == "true");
        SetCastShadows(CastShadowsString == "true");
    }
    else
    {
        InOutHandle["Intensity"] = Intensity;
        InOutHandle["LightColor"] = FJsonSerializer::VectorToJson(LightColor);
        InOutHandle["bVisible"] = GetVisible() ? "true" : "false";
        InOutHandle["bLightEnabled"] = GetLightEnabled() ? "true" : "false";
        InOutHandle["bCastShadows"] = GetCastShadows() ? "true" : "false";
    }
}

UObject* ULightComponentBase::Duplicate()
{
    ULightComponentBase* LightComponentBase = Cast<ULightComponentBase>(Super::Duplicate());
    LightComponentBase->Intensity = Intensity;
    LightComponentBase->LightColor = LightColor;
    LightComponentBase->bVisible = bVisible;
    LightComponentBase->bLightEnabled = bLightEnabled;
    LightComponentBase->bCastShadows = bCastShadows;
    return LightComponentBase;
}

void ULightComponentBase::DuplicateSubObjects(UObject* DuplicatedObject)
{
    Super::DuplicateSubObjects(DuplicatedObject);
}
