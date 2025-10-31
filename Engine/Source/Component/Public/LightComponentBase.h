#pragma once

#include "SceneComponent.h"

UCLASS()
class ULightComponentBase : public USceneComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(ULightComponentBase, USceneComponent)

public:
    ULightComponentBase() = default;

    virtual ~ULightComponentBase() = default;
    
    /*-----------------------------------------------------------------------------
        UObject Features
     -----------------------------------------------------------------------------*/
public:
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    virtual UObject* Duplicate() override;
        
    virtual void DuplicateSubObjects(UObject* DuplicatedObject) override;

    /*-----------------------------------------------------------------------------
        UActorComponent Features
     -----------------------------------------------------------------------------*/
public:
    virtual void BeginPlay() override { Super::BeginPlay(); }
    
    virtual void TickComponent(float DeltaTime) override { Super::TickComponent(DeltaTime); }

    virtual void EndPlay() override { Super::EndPlay(); }

    /*-----------------------------------------------------------------------------
        ULightComponent Features
     -----------------------------------------------------------------------------*/
public:
    // --- Getters & Setters ---

    float GetIntensity() const { return Intensity; }

    FVector GetLightColor() const { return LightColor;}

    //virtual ELightComponentType GetLightType() const { return ELightComponentType::LightType_Max; }
    
    
    bool GetVisible() const { return bVisible; }

    // --- [UE Style] ---

    // virtual FBox GetBoundingBox() const;

    // virtual FSphere GetBoundingSphere() const;
    
    /** @note Sets the light intensity and clamps it to the same range as Unreal Engine (0.0 - 20.0). */
    virtual void SetIntensity(float InIntensity) { Intensity = std::clamp(InIntensity, 0.0f, 20.0f); }

    virtual void SetLightColor(FVector InLightColor) { LightColor = InLightColor; }
    
    void SetVisible(bool InVisible) { bVisible = InVisible; }
    
    bool GetLightEnabled() const { return bLightEnabled; }
    void SetLightEnabled(bool InEnabled) { bLightEnabled = InEnabled; }

    /*-----------------------------------------------------------------------------
        Shadow Features
     -----------------------------------------------------------------------------*/
public:
    EShadowModeIndex GetShadowModeIndex() const { return ShadowModeIndex; }

    void SetShadowModeIndex(EShadowModeIndex InShadowModeIndex) { ShadowModeIndex = InShadowModeIndex; }
    
    /** @brief 이 라이트가 shadow를 cast할지 여부를 반환합니다. */
    bool GetCastShadows() const { return bCastShadows; }

    /** @brief 이 라이트의 shadow casting 여부를 설정합니다. */
    void SetCastShadows(bool InCastShadows) { bCastShadows = InCastShadows; }

protected:
    /** Total energy that the light emits. */
    float Intensity = 1.0f;

    /**
     * Filter color of the light.
     * @todo Change type of this variable into FLinearColor
     */
    FVector LightColor = { 1.0f, 1.0f, 1.0f };

    bool bVisible = true;
    
    bool bLightEnabled = true; // 조명 계산 포함 여부 (Outliner Visible과 독립)

    /** 현재 그림자의 종류 */
    EShadowModeIndex ShadowModeIndex = EShadowModeIndex::SMI_PCF;

    /** 이 라이트가 shadow를 cast할지 여부 */
    bool bCastShadows = true;
};
