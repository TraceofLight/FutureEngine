#pragma once
#include "Widget.h"

class UClass;
class UBoxComponent;

UCLASS()
class UBoxComponentWidget : public UWidget
{
	GENERATED_BODY()
	DECLARE_CLASS(UBoxComponentWidget, UWidget)

public:
	UBoxComponentWidget() = default;
	virtual ~UBoxComponentWidget() = default;

	virtual void Initialize() override;
	virtual void Update() override;
	virtual void RenderWidget() override;

private:
	UBoxComponent* BoxComponent = nullptr;
};
