#include "pch.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Component/Mesh/Public/VertexDatas.h"
#include "Physics/Public/AABB.h"
#include "Texture/Public/Texture.h"
#include "Manager/Asset/Public/ObjManager.h"
#include "Manager/Path/Public/PathManager.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"

IMPLEMENT_SINGLETON_CLASS(UAssetManager, UObject)
UAssetManager::UAssetManager()
{
	TextureManager = new FTextureManager();
}

UAssetManager::~UAssetManager() = default;

void UAssetManager::Initialize()
{
	TextureManager->LoadAllTexturesFromDirectory(UPathManager::GetInstance().GetDataPath());
	// Data 폴더 속 모든 .obj 파일 로드 및 캐싱
	LoadAllObjStaticMesh();

	VertexDatas.Emplace(EPrimitiveType::Torus, &VerticesTorus);
	VertexDatas.Emplace(EPrimitiveType::Arrow, &VerticesArrow);
	VertexDatas.Emplace(EPrimitiveType::CubeArrow, &VerticesCubeArrow);
	VertexDatas.Emplace(EPrimitiveType::Ring, &VerticesRing);
	VertexDatas.Emplace(EPrimitiveType::Line, &VerticesLine);
	VertexDatas.Emplace(EPrimitiveType::Sprite, &VerticesVerticalSquare);

	IndexDatas.Emplace(EPrimitiveType::Sprite, &IndicesVerticalSquare);
	IndexBuffers.Emplace(EPrimitiveType::Sprite,
		FRenderResourceFactory::CreateIndexBuffer(IndicesVerticalSquare.GetData(), static_cast<int>(IndicesVerticalSquare.Num()) * sizeof(uint32)));

	NumIndices.Emplace(EPrimitiveType::Sprite, static_cast<uint32>(IndicesVerticalSquare.Num()));
	
	VertexBuffers.Emplace(EPrimitiveType::Torus, FRenderResourceFactory::CreateVertexBuffer(
		VerticesTorus.GetData(), static_cast<int>(VerticesTorus.Num() * sizeof(FNormalVertex))));
	VertexBuffers.Emplace(EPrimitiveType::Arrow, FRenderResourceFactory::CreateVertexBuffer(
		VerticesArrow.GetData(), static_cast<int>(VerticesArrow.Num() * sizeof(FNormalVertex))));
	VertexBuffers.Emplace(EPrimitiveType::CubeArrow, FRenderResourceFactory::CreateVertexBuffer(
		VerticesCubeArrow.GetData(), static_cast<int>(VerticesCubeArrow.Num() * sizeof(FNormalVertex))));
	VertexBuffers.Emplace(EPrimitiveType::Ring, FRenderResourceFactory::CreateVertexBuffer(
		VerticesRing.GetData(), static_cast<int>(VerticesRing.Num() * sizeof(FNormalVertex))));
	VertexBuffers.Emplace(EPrimitiveType::Line, FRenderResourceFactory::CreateVertexBuffer(
		VerticesLine.GetData(), static_cast<int>(VerticesLine.Num() * sizeof(FNormalVertex))));
	VertexBuffers.Emplace(EPrimitiveType::Sprite, FRenderResourceFactory::CreateVertexBuffer(
		VerticesVerticalSquare.GetData(), static_cast<int>(VerticesVerticalSquare.Num() * sizeof(FNormalVertex))));

	NumVertices.Emplace(EPrimitiveType::Torus, static_cast<uint32>(VerticesTorus.Num()));
	NumVertices.Emplace(EPrimitiveType::Arrow, static_cast<uint32>(VerticesArrow.Num()));
	NumVertices.Emplace(EPrimitiveType::CubeArrow, static_cast<uint32>(VerticesCubeArrow.Num()));
	NumVertices.Emplace(EPrimitiveType::Ring, static_cast<uint32>(VerticesRing.Num()));
	NumVertices.Emplace(EPrimitiveType::Line, static_cast<uint32>(VerticesLine.Num()));
	NumVertices.Emplace(EPrimitiveType::Sprite, static_cast<uint32>(VerticesVerticalSquare.Num()));
	
	// Calculate AABB for all primitive types (excluding StaticMesh)
	for (const auto& Pair : VertexDatas)
	{
		EPrimitiveType Type = Pair.first;
		const auto* Vertices = Pair.second;
		if (!Vertices || Vertices->IsEmpty())
		{
			continue;
		}

		AABBs[Type] = CalculateAABB(*Vertices);
	}

	// Calculate AABB for each StaticMesh
	for (const auto& MeshPair : StaticMeshCache)
	{
		const FName& ObjPath = MeshPair.first;
		const auto& Mesh = MeshPair.second;
		if (!Mesh || !Mesh->IsValid())
		{
			continue;
		}

		const auto& Vertices = Mesh->GetVertices();
		if (Vertices.IsEmpty())
		{
			continue;
		}

		StaticMeshAABBs[ObjPath] = CalculateAABB(Vertices);
	}
}

void UAssetManager::Release()
{
	// TMap.Value()
	for (auto& Pair : VertexBuffers)
	{
		SafeRelease(Pair.second);
	}
	for (auto& Pair : IndexBuffers)
	{
		SafeRelease(Pair.second);
	}

	for (auto& Pair : StaticMeshVertexBuffers)
	{
		SafeRelease(Pair.second);
	}
	for (auto& Pair : StaticMeshIndexBuffers)
	{
		SafeRelease(Pair.second);
	}

	StaticMeshCache.Empty();	// unique ptr 이라서 자동으로 해제됨
	StaticMeshVertexBuffers.Empty();
	StaticMeshIndexBuffers.Empty();

	// TMap.Empty()
	VertexBuffers.Empty();
	IndexBuffers.Empty();
	
	SafeDelete(TextureManager);
}

/**
 * @brief Data/ 경로 하위에 모든 .obj 파일을 로드 후 캐싱한다
 */
void UAssetManager::LoadAllObjStaticMesh()
{
	TArray<FName> ObjList;
	const FString DataDirectory = "Data/"; // 검색할 기본 디렉토리
	// 디렉토리가 실제로 존재하는지 먼저 확인합니다.
	if (std::filesystem::exists(DataDirectory) && std::filesystem::is_directory(DataDirectory))
	{
		// recursive_directory_iterator를 사용하여 디렉토리와 모든 하위 디렉토리를 순회합니다.
		for (const auto& Entry : std::filesystem::recursive_directory_iterator(DataDirectory))
		{
			// 현재 항목이 일반 파일이고, 확장자가 ".obj"인지 확인합니다.
			if (Entry.is_regular_file() && Entry.path().extension() == ".obj")
			{
				// .generic_string()을 사용하여 OS에 상관없이 '/' 구분자를 사용하는 경로를 바로 얻습니다.
				FString PathString = Entry.path().generic_string();

				// 찾은 파일 경로를 FName으로 변환하여 ObjList에 추가합니다.
				ObjList.Emplace(FName(PathString));
			}
		}
	}

	// Enable winding order flip for this OBJ file
	FObjImporter::Configuration Config;
	Config.bFlipWindingOrder = false;
	Config.bIsBinaryEnabled = true;
	Config.bPositionToUEBasis = true;
	Config.bNormalToUEBasis = true;
	Config.bUVToUEBasis = true;

	// 범위 기반 for문을 사용하여 배열의 모든 요소를 순회합니다.
	for (const FName& ObjPath : ObjList)
	{
		// FObjManager가 UStaticMesh 포인터를 반환한다고 가정합니다.
		UStaticMesh* LoadedMesh = FObjManager::LoadObjStaticMesh(ObjPath, Config);

		// 로드에 성공했는지 확인합니다.
		if (LoadedMesh)
		{
			StaticMeshCache.Emplace(ObjPath, LoadedMesh);

			StaticMeshVertexBuffers.Emplace(ObjPath, this->CreateVertexBuffer(LoadedMesh->GetVertices()));
			StaticMeshIndexBuffers.Emplace(ObjPath, this->CreateIndexBuffer(LoadedMesh->GetIndices()));
		}
	}
}

ID3D11Buffer* UAssetManager::GetVertexBuffer(FName InObjPath)
{
	return StaticMeshVertexBuffers.FindRef(InObjPath);
}

ID3D11Buffer* UAssetManager::GetIndexBuffer(FName InObjPath)
{
	return StaticMeshIndexBuffers.FindRef(InObjPath);
}

ID3D11Buffer* UAssetManager::CreateVertexBuffer(TArray<FNormalVertex> InVertices)
{
	return FRenderResourceFactory::CreateVertexBuffer(InVertices.GetData(), static_cast<int>(InVertices.Num()) * sizeof(FNormalVertex));
}

ID3D11Buffer* UAssetManager::CreateIndexBuffer(TArray<uint32> InIndices)
{
	return FRenderResourceFactory::CreateIndexBuffer(InIndices.GetData(), static_cast<int>(InIndices.Num()) * sizeof(uint32));
}

TArray<FNormalVertex>* UAssetManager::GetVertexData(EPrimitiveType InType)
{
	return VertexDatas[InType];
}

ID3D11Buffer* UAssetManager::GetVertexbuffer(EPrimitiveType InType)
{
	return VertexBuffers[InType];
}

uint32 UAssetManager::GetNumVertices(EPrimitiveType InType)
{
	return NumVertices[InType];
}

TArray<uint32>* UAssetManager::GetIndexData(EPrimitiveType InType)
{
	return IndexDatas[InType];
}

ID3D11Buffer* UAssetManager::GetIndexBuffer(EPrimitiveType InType)
{
	return IndexBuffers[InType];
}

uint32 UAssetManager::GetNumIndices(EPrimitiveType InType)
{
	return NumIndices[InType];
}

FAABB& UAssetManager::GetAABB(EPrimitiveType InType)
{
	return AABBs[InType];
}

FAABB& UAssetManager::GetStaticMeshAABB(FName InName)
{
	return StaticMeshAABBs[InName];
}

// StaticMesh Cache Accessors
UStaticMesh* UAssetManager::GetStaticMeshFromCache(const FName& InObjPath)
{
	if (auto* FoundPtr = StaticMeshCache.Find(InObjPath))
	{
		return *FoundPtr;
	}
	return nullptr;
}

void UAssetManager::AddStaticMeshToCache(const FName& InObjPath, UStaticMesh* InStaticMesh)
{
	if (!InStaticMesh)
	{
		return;
	}

	if (!StaticMeshCache.Contains(InObjPath))
	{
		StaticMeshCache.Add(InObjPath, InStaticMesh);
	}
}

/**
 * @brief Vertex 배열로부터 AABB(Axis-Aligned Bounding Box)를 계산하는 헬퍼 함수
 * @param Vertices 정점 데이터 배열
 * @return 계산된 FAABB 객체
 */
FAABB UAssetManager::CalculateAABB(const TArray<FNormalVertex>& Vertices)
{
	FVector MinPoint(+FLT_MAX, +FLT_MAX, +FLT_MAX);
	FVector MaxPoint(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (const auto& Vertex : Vertices)
	{
		MinPoint.X = std::min(MinPoint.X, Vertex.Position.X);
		MinPoint.Y = std::min(MinPoint.Y, Vertex.Position.Y);
		MinPoint.Z = std::min(MinPoint.Z, Vertex.Position.Z);

		MaxPoint.X = std::max(MaxPoint.X, Vertex.Position.X);
		MaxPoint.Y = std::max(MaxPoint.Y, Vertex.Position.Y);
		MaxPoint.Z = std::max(MaxPoint.Z, Vertex.Position.Z);
	}

	return FAABB(MinPoint, MaxPoint);
}

/**
 * @brief 넘겨준 경로로 캐싱된 UTexture 포인터를 반환해주는 함수
 * @param 로드할 텍스처 경로
 * @return 캐싱된 UTexture 포인터
 */
UTexture* UAssetManager::LoadTexture(const FName& InFilePath)
{
	return TextureManager->LoadTexture(InFilePath);
}

/**
 * @brief 지금까지 캐싱된 UTexture 포인터 목록 반환해주는 함수
 * @return {경로, 캐싱된 UTexture 포인터}
 */
const TMap<FName, UTexture*>& UAssetManager::GetTextureCache() const
{
	return TextureManager->GetTextureCache();
}
