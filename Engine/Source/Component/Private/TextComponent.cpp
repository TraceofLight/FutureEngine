#include "pch.h"
#include "Component/Public/TextComponent.h"
#include "Editor/Public/Editor.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/UI/Widget/Public/SetTextComponentWidget.h"
#include "Level/Public/Level.h"

IMPLEMENT_CLASS(UTextComponent, UPrimitiveComponent)

UTextComponent::UTextComponent()
{
	Vertices = &PickingAreaVertex;
	NumVertices = static_cast<uint32>(PickingAreaVertex.Num());

	Indices = &PickingAreaIndex;
	NumIndices = static_cast<uint32>(PickingAreaIndex.Num());

	RegulatePickingAreaByTextLength();
}

UTextComponent::~UTextComponent()
{
}

void UTextComponent::UpdateRotationMatrix(const FVector& InCameraLocation) {}
FMatrix UTextComponent::GetRTMatrix() const { return FMatrix(); }

const FString& UTextComponent::GetText() { return Text; }
void UTextComponent::SetText(const FString& InText)
{
	if (Text == InText) return;           // 불필요한 갱신 방지

	Text = InText;
	RegulatePickingAreaByTextLength(); // 길이에 맞춘 로컬 박스 재계산

	// 1) AABB 캐시 무효화 + 자식 변환 더티 전파
	MarkAsDirty();

	// 2) 옥트리/가시성 시스템에 바운딩 업데이트 알림
	GWorld->GetLevel()->UpdatePrimitiveInOctree(this);
}

UClass* UTextComponent::GetSpecificWidgetClass() const
{
	return USetTextComponentWidget::StaticClass();
}

UObject* UTextComponent::Duplicate()
{
	UTextComponent* TextComponent = Cast<UTextComponent>(Super::Duplicate());
	TextComponent->Text = Text;
	return TextComponent;
}

void UTextComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}

void UTextComponent::RegulatePickingAreaByTextLength()
{
	PickingAreaVertex.Empty();
	int32 NewStrLen = static_cast<int32>(Text.size());

	const static TPair<int32, int32> Offset[] =
	{
		{-1, 1},
		{1, 1},
		{-1, -1},
		{1, -1}
	};

	for (int i = 0; i < 4; i++)
	{
		FNormalVertex NewVertex = {
			{0.0f, 0.5f * NewStrLen * Offset[i].first, 0.5f * Offset[i].second},
			{}, {}, {}
		};
		PickingAreaVertex.Add(NewVertex);
	}

	PickingAreaBoundingBox =
		FAABB(
			FVector(0.0f, -0.5f * NewStrLen, -0.5f),
			FVector(0.0f, 0.5f * NewStrLen, 0.5f)
		);
	BoundingBox = &PickingAreaBoundingBox;
}
