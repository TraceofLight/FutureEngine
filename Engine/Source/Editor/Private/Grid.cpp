#include "pch.h"
#include "Editor/Public/Grid.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Editor/Public/EditorPrimitive.h"
#include "Manager/Config/Public/ConfigManager.h"

UGrid::UGrid()
	: Vertices(TArray<FVector>()) // 아래 UpdateVerticesBy에 넣어주는 값과 달라야 함
{
	NumVertices = NumLines * 4;
	Vertices.Reserve(NumVertices);
	UpdateVerticesBy(UConfigManager::GetInstance().LoadCellSize());
}

UGrid::~UGrid()
{
	UConfigManager::GetInstance().SaveCellSize(CellSize);
}

void UGrid::UpdateVerticesBy(float NewCellSize)
{
	// 중복 삽입 방지
	if (CellSize == NewCellSize)
	{
		return;
	}

	CellSize = NewCellSize; // 필요하다면 멤버 변수도 갱신

	float LineLength = NewCellSize * static_cast<float>(NumLines) / 2.f;

	if (Vertices.Num() < NumVertices)
	{
		Vertices.SetNum(NumVertices);
	}

	uint32 vertexIndex = 0;
	// Y축 라인 업데이트 (X 방향으로 그리는 선들)
	for (int32 LineCount = -NumLines / 2; LineCount < NumLines / 2; ++LineCount)
	{
		Vertices[vertexIndex++] = { static_cast<float>(LineCount) * NewCellSize, -LineLength, 0.0f };
		Vertices[vertexIndex++] = { static_cast<float>(LineCount) * NewCellSize, LineLength, 0.0f };
	}

	// X축 라인 업데이트 (Y 방향으로 그리는 선들)
	for (int32 LineCount = -NumLines / 2; LineCount < NumLines / 2; ++LineCount)
	{
		Vertices[vertexIndex++] = { -LineLength, static_cast<float>(LineCount) * NewCellSize, 0.0f };
		Vertices[vertexIndex++] = { LineLength, static_cast<float>(LineCount) * NewCellSize, 0.0f };
	}
}

void UGrid::MergeVerticesAt(TArray<FVector>& DestVertices, size_t InsertStartIndex)
{
	// 인덱스 범위 보정
	InsertStartIndex = std::min(DestVertices.Num(), static_cast<int32>(InsertStartIndex));
	
	// 미리 메모리 확보
	DestVertices.Reserve(DestVertices.Num() + std::distance(Vertices.begin(), Vertices.end()));

	// 덮어쓸 수 있는 개수 계산
	size_t overwriteCount = std::min(
		Vertices.Num(),
		DestVertices.Num() - static_cast<int32>(InsertStartIndex)
	);

	// 기존 요소 덮어쓰기
	std::copy(
		Vertices.begin(),
		Vertices.begin() + overwriteCount,
		DestVertices.begin() + InsertStartIndex
	);
}
