#pragma once
#include "Widget.h"

class UClass;
class UCapsuleComponent;

UCLASS()
class UCapsuleComponentWidget : public UWidget
{
	GENERATED_BODY()
	DECLARE_CLASS(UCapsuleComponentWidget, UWidget)

public:
	UCapsuleComponentWidget() = default;
	virtual ~UCapsuleComponentWidget() = default;

	virtual void Initialize() override;
	virtual void Update() override;
	virtual void RenderWidget() override;

private:
	UCapsuleComponent* CapsuleComponent = nullptr;
};
