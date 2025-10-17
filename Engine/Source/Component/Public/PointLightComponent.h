﻿#pragma once

#include "LightComponent.h"

UCLASS()
class UPointLightComponent : public ULightComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(UPointLightComponent, ULightComponent)

public:
    UPointLightComponent() = default;

    virtual ~UPointLightComponent() = default;
    
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

    virtual UClass* GetSpecificWidgetClass() const override; 

    /*-----------------------------------------------------------------------------
        ULightComponent Features
     -----------------------------------------------------------------------------*/
public:
    virtual ELightComponentType GetLightType() const override { return ELightComponentType::LightType_Point; }

    /*-----------------------------------------------------------------------------
        UPointLightComponent Features
     -----------------------------------------------------------------------------*/
public:
    // --- Getters & Setters ---

    float GetLightFalloffExponent() const { return LightFalloffExponent; }

    float GetAttenuationRadius() const { return AttenuationRadius;}
    
    /** @note Sets the light falloff exponent and clamps it to the same range as Unreal Engine (2.0 - 16.0). */
    void SetLightFalloffExponent(float InLightFalloffExponent) { LightFalloffExponent = std::clamp(InLightFalloffExponent, 2.0f, 16.0f); }
    
    void SetAttenuationRadius(float InAttenuationRadius) { AttenuationRadius = InAttenuationRadius; }

private:
    /**
     * Controls the radial falloff of the light.
     * 2 is almost linear and very unrealistic and around 8 it looks reasonable.
     * With large exponents, the light has contribution to only a small area of its influence radius but still costs the same as low exponents.
     */
    float LightFalloffExponent = 2.0f;
    
    /** Radius of light source shape. */
    float AttenuationRadius = 10.0f;
};