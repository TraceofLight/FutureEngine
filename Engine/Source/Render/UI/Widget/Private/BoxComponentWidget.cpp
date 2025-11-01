#include "pch.h"
#include "Render/UI/Widget/Public/BoxComponentWidget.h"
#include "Component/Public/BoxComponent.h"
#include "Editor/Public/Editor.h"
#include "Level/Public/Level.h"
#include "Component/Public/ActorComponent.h"
#include "ImGui/imgui.h"

IMPLEMENT_CLASS(UBoxComponentWidget, UWidget)

void UBoxComponentWidget::Initialize()
{
}

void UBoxComponentWidget::Update()
{
	ULevel* CurrentLevel = GWorld->GetLevel();
	if (CurrentLevel)
	{
		UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
		if (BoxComponent != NewSelectedComponent)
		{
			BoxComponent = Cast<UBoxComponent>(NewSelectedComponent);
		}
	}
}

void UBoxComponentWidget::RenderWidget()
{
	if (!BoxComponent)
	{
		return;
	}

	ImGui::Separator();
	ImGui::Text("Box Component");

	// Push black background style for input fields
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

	// Box Extent (Half-extents)
	FVector Extent = BoxComponent->GetBoxExtent();
	float ExtentArray[3] = { Extent.X, Extent.Y, Extent.Z };
	if (ImGui::DragFloat3("Box Extent", ExtentArray, 0.01f, 0.0f, 1000.0f, "%.2f"))
	{
		BoxComponent->SetBoxExtent(FVector(ExtentArray[0], ExtentArray[1], ExtentArray[2]));
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Half-extents of the box in each direction");
	}

	ImGui::PopStyleColor(3);

	// Collision Settings
	ImGui::Separator();
	ImGui::Text("Collision");

	bool bGenerateHit = BoxComponent->bGenerateHitEvents;
	if (ImGui::Checkbox("Generate Hit Events", &bGenerateHit))
	{
		BoxComponent->bGenerateHitEvents = bGenerateHit;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("If true, fires OnActorHit (blocking collision).\nIf false, fires OnActorBeginOverlap (trigger).");
	}
}
