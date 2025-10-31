#pragma once
#include "Widget.h"

class UClass;
class USphereComponent;

UCLASS()
class USphereComponentWidget : public UWidget
{
	GENERATED_BODY()
	DECLARE_CLASS(USphereComponentWidget, UWidget)

public:
	USphereComponentWidget() = default;
	virtual ~USphereComponentWidget() = default;

	virtual void Initialize() override;
	virtual void Update() override;
	virtual void RenderWidget() override;

private:
	USphereComponent* SphereComponent = nullptr;
};
