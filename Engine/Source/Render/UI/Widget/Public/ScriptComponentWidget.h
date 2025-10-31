#pragma once
#include "Render/UI/Widget/Public/Widget.h"

class UScriptComponent;

UCLASS()
class UScriptComponentWidget : public UWidget
{
    GENERATED_BODY()
    DECLARE_CLASS(UScriptComponentWidget, UWidget)

public:
    void Initialize() override {}
    void Update() override;
    void RenderWidget() override;
};