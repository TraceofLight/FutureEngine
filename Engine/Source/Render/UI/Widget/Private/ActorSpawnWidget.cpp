#include "pch.h"
#include "Render/UI/Widget/Public/ActorSpawnWidget.h"
#include "Level/Public/Level.h"

IMPLEMENT_CLASS(UActorSpawnWidget, UWidget)

UActorSpawnWidget::UActorSpawnWidget()
{
	LoadActorClasses();
}

UActorSpawnWidget::~UActorSpawnWidget() = default;

void UActorSpawnWidget::Initialize()
{
	// Do Nothing Here
}

void UActorSpawnWidget::Update()
{
	// Do Nothing Here
}

void UActorSpawnWidget::RenderWidget()
{
	ImGui::Text("Actor 생성");
	ImGui::Spacing();

	ImGui::Text("Actor Type:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120);

	TArray<const char*> Names;
	for (const FString& ClassName : ActorClassNames)
	{
		Names.Add(ClassName.c_str());
	}
	
	ImGui::Combo(
		"##ActorType",
		&SelectedActorClassIndex,
		Names.GetData(),
		Names.Num()
	);
	
	// Spawn 버튼과 개수 입력
	ImGui::Text("Number of Spawn:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	ImGui::InputInt("##NumberOfSpawn", &NumberOfSpawn);
	NumberOfSpawn = max(1, NumberOfSpawn);
	NumberOfSpawn = min(100, NumberOfSpawn);

	ImGui::SameLine();
	if (ImGui::Button("Spawn Actors"))
	{
		SpawnActors();
	}

	// 스폰 범위 설정
	ImGui::Text("Spawn Range:");
	ImGui::SetNextItemWidth(80);
	ImGui::DragFloat("Min##SpawnRange", &SpawnRangeMin, 0.1f, -50.0f, SpawnRangeMax - 0.1f);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	ImGui::DragFloat("Max##SpawnRange", &SpawnRangeMax, 0.1f, SpawnRangeMin + 0.1f, 50.0f);

	ImGui::Separator();
}

/**
 * @brief Actor 생성 함수
 * 난수를 활용한 Range, Size, Rotion 값 생성으로 Actor Spawn
 */
void UActorSpawnWidget::SpawnActors()
{
	UE_LOG("ControlPanel: %s 타입의 Actor를 %d개 생성 시도합니다",
		SelectedActorClassName.ToString().data(), NumberOfSpawn);

	// 지정된 개수만큼 액터 생성
	AActor* LastSpawnedActor = nullptr;
	for (int32 i = 0; i < NumberOfSpawn; i++)
	{
		AActor* NewActor = GWorld->SpawnActor(ActorClasses[SelectedActorClassIndex]);

		if (NewActor)
		{
			// 범위 내 랜덤 위치
			float RandomX = SpawnRangeMin + (static_cast<float>(rand()) / RAND_MAX) * (SpawnRangeMax - SpawnRangeMin);
			float RandomY = SpawnRangeMin + (static_cast<float>(rand()) / RAND_MAX) * (SpawnRangeMax - SpawnRangeMin);
			float RandomZ = SpawnRangeMin + (static_cast<float>(rand()) / RAND_MAX) * (SpawnRangeMax - SpawnRangeMin);

			NewActor->SetActorLocation(FVector(RandomX, RandomY, RandomZ));

			// 임의의 스케일 (0.5 ~ 2.0 범위)
			float RandomScale = 0.5f + (static_cast<float>(rand()) / RAND_MAX) * 1.5f;
			NewActor->SetActorScale3D(FVector(RandomScale, RandomScale, RandomScale));

			UE_LOG("ControlPanel: (%.2f, %.2f, %.2f) 지점에 Actor를 배치했습니다", RandomX, RandomY, RandomZ);

			LastSpawnedActor = NewActor;
		}
		else
		{
			UE_LOG("ControlPanel: Actor 배치에 실패했습니다 %d", i);
		}
	}

	// 마지막으로 생성된 Actor를 선택
	if (LastSpawnedActor)
	{
		GEditor->GetEditorModule()->SelectActor(LastSpawnedActor);
	}
}

void UActorSpawnWidget::LoadActorClasses()
{
	for (UClass* Class : UClass::FindClasses(AActor::StaticClass()))
	{ 
		ActorClasses.Add(Class);
		ActorClassNames.Add(Class->GetName().ToString().substr(1));
	}
}
