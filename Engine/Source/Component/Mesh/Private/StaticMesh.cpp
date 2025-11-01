#include "pch.h" // 프로젝트의 Precompiled Header
#include "Source/Component/Mesh/Public/StaticMesh.h" // UStaticMesh 클래스 자신의 헤더

// FStaticMesh 구조체에 대한 정의가 UStaticMesh.h에 이미 포함되어 있다고 가정합니다.

// 클래스 구현 매크로
IMPLEMENT_CLASS(UStaticMesh, UObject)

UStaticMesh::UStaticMesh()
	: StaticMeshAsset(nullptr) // 멤버 변수를 nullptr로 명시적 초기화
{
}

UStaticMesh::~UStaticMesh()
{
	// 이 클래스는 FStaticMesh 데이터의 소유자가 아닙니다.
	// 실제 데이터는 AssetManager가 관리하므로, 여기서는 아무것도 하지 않습니다.

	// 임시로 할당된 Material 해제 -> 이후 GUObject에서 관리 예정
	for (UMaterial* Material : Materials)
	{
		// Skip deletion of shared DefaultMaterial to prevent dangling pointer
		if (Material && Material->GetName().ToString() != "DefaultMaterial")
		{
			SafeDelete(Material);
		}
	}
	Materials.Empty();
}

void UStaticMesh::SetStaticMeshAsset(FStaticMesh* InStaticMeshAsset)
{
	this->StaticMeshAsset = InStaticMeshAsset;
}

const FName& UStaticMesh::GetAssetPathFileName() const
{
	// 항상 포인터가 유효한지 확인 후 접근해야 합니다.
	if (StaticMeshAsset)
	{
		return StaticMeshAsset->PathFileName;
	}

	// 포인터가 null일 경우, 안전하게 비어있는 static 객체를 반환합니다.
	static const FName EmptyString = "";
	return EmptyString;
}

const TArray<FNormalVertex>& UStaticMesh::GetVertices() const
{
	if (StaticMeshAsset)
	{
		return StaticMeshAsset->Vertices;
	}
	static const TArray<FNormalVertex> EmptyVertices;
	return EmptyVertices;
}

TArray<FNormalVertex>& UStaticMesh::GetVertices()
{
	if (StaticMeshAsset)
	{
		return StaticMeshAsset->Vertices;
	}
	static TArray<FNormalVertex> EmptyVertices;
	return EmptyVertices;
}

const TArray<uint32>& UStaticMesh::GetIndices() const
{
	if (StaticMeshAsset)
	{
		return StaticMeshAsset->Indices;
	}
	static const TArray<uint32> EmptyIndices;
	return EmptyIndices;
}

UMaterial* UStaticMesh::GetMaterial(int32 MaterialIndex) const
{
	return (MaterialIndex >= 0 && MaterialIndex < Materials.Num()) ? Materials[MaterialIndex] : nullptr;
}

void UStaticMesh::SetMaterial(int32 MaterialIndex, UMaterial* Material)
{
	if (MaterialIndex >= 0)
	{
		// 배열 크기가 부족하면 확장
		if (MaterialIndex >= Materials.Num())
		{
			Materials.SetNum(MaterialIndex + 1, nullptr);
		}
		Materials[MaterialIndex] = Material;
	}
}

int32 UStaticMesh::GetNumMaterials() const
{
	return Materials.Num();
}

const TArray<FMeshSection>& UStaticMesh::GetSections() const
{
	if (StaticMeshAsset)
	{
		return StaticMeshAsset->Sections;
	}
	static const TArray<FMeshSection> EmptySections;
	return EmptySections;
}
