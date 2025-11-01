#include "pch.h"
#include "Render/UI/Widget/Public/SphereComponentWidget.h"
#include "Component/Public/SphereComponent.h"
#include "Editor/Public/Editor.h"
#include "Level/Public/Level.h"
#include "Component/Public/ActorComponent.h"
#include "ImGui/imgui.h"

IMPLEMENT_CLASS(USphereComponentWidget, UWidget)

void USphereComponentWidget::Initialize()
{
}

void USphereComponentWidget::Update()
{
	ULevel* CurrentLevel = GWorld->GetLevel();
	if (CurrentLevel)
	{
		UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
		if (SphereComponent != NewSelectedComponent)
		{
			SphereComponent = Cast<USphereComponent>(NewSelectedComponent);
		}
	}
}

void USphereComponentWidget::RenderWidget()
{
	if (!SphereComponent)
	{
		return;
	}

	ImGui::Separator();
	ImGui::Text("Sphere Component");

	// Push black background style for input fields
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

	// Sphere Radius
	float Radius = SphereComponent->GetSphereRadius();
	if (ImGui::DragFloat("Sphere Radius", &Radius, 0.01f, 0.0f, 1000.0f, "%.2f"))
	{
		SphereComponent->SetSphereRadius(Radius);
	}

	ImGui::PopStyleColor(3);

	// TODO: Add bGenerateHitEvents checkbox when SphereToSphere collision calculates HitResult
	// ImGui::Separator();
	// ImGui::Text("Collision");
	// bool bGenerateHit = SphereComponent->bGenerateHitEvents;
	// if (ImGui::Checkbox("Generate Hit Events", &bGenerateHit)) { ... }
}
