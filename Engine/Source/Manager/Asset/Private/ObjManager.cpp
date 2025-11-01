#include "pch.h"

#include "Core/Public/ObjectIterator.h"
#include "Manager/Asset/Public/ObjManager.h"
#include "Manager/Asset/Public/ObjImporter.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Texture/Public/Material.h"
#include "Texture/Public/Texture.h"
#include <filesystem>

// N과 직교하는 안전한 탄젠트 생성 (폴백용)
static FORCEINLINE FVector MakeFallbackTangent(const FVector& N)
{
	// N과 덜 평행한 기준축 선택
	const FVector Pick = (abs(N.Z) < 0.999f) ? FVector(0.f, 0.f, 1.f) : FVector(0.f, 1.f, 0.f);
	FVector T = Cross(Pick, N);
	T.Normalize();
	return T;
}
static void ComputeTangents(TArray<FNormalVertex>& Vertices, const TArray<uint32>& Indices)
{
	TArray<FVector> AccumulatedBitangent;
	AccumulatedBitangent.SetNum(Vertices.Num());
	for (size_t I = 0; I < AccumulatedBitangent.Num(); ++I)
	{
		AccumulatedBitangent[I] = FVector(0.0f, 0.0f, 0.0f);
		Vertices[I].Tangent = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	size_t IndexCount = Indices.Num();
	for (size_t I = 0; I + 2 < IndexCount; I += 3)
	{
		uint32 I0 = Indices[I + 0];
		uint32 I1 = Indices[I + 1];
		uint32 I2 = Indices[I + 2];

		const FVector& P0 = Vertices[I0].Position;
		const FVector& P1 = Vertices[I1].Position;
		const FVector& P2 = Vertices[I2].Position;

		const FVector2& Uv0 = Vertices[I0].TexCoord;
		const FVector2& Uv1 = Vertices[I1].TexCoord;
		const FVector2& Uv2 = Vertices[I2].TexCoord;

		FVector Edge1 = P1 - P0;
		FVector Edge2 = P2 - P0;

		float DeltaU1 = Uv1.X - Uv0.X;
		float DeltaV1 = Uv1.Y - Uv0.Y;
		float DeltaU2 = Uv2.X - Uv0.X;
		float DeltaV2 = Uv2.Y - Uv0.Y;

		float Denominator = (DeltaU1 * DeltaV2 - DeltaU2 * DeltaV1);
		if (fabsf(Denominator) < 1e-8f)
		{
			continue;
		}
		float Reciprocal = 1.0f / Denominator;

		FVector FaceTangent = (Edge1 * DeltaV2 - Edge2 * DeltaV1) * Reciprocal;
		FVector FaceBitangent = (Edge2 * DeltaU1 - Edge1 * DeltaU2) * Reciprocal;

		Vertices[I0].Tangent.X += FaceTangent.X;
		Vertices[I0].Tangent.Y += FaceTangent.Y;
		Vertices[I0].Tangent.Z += FaceTangent.Z;
		AccumulatedBitangent[I0] = AccumulatedBitangent[I0] + FaceBitangent;

		Vertices[I1].Tangent.X += FaceTangent.X;
		Vertices[I1].Tangent.Y += FaceTangent.Y;
		Vertices[I1].Tangent.Z += FaceTangent.Z;
		AccumulatedBitangent[I1] = AccumulatedBitangent[I1] + FaceBitangent;

		Vertices[I2].Tangent.X += FaceTangent.X;
		Vertices[I2].Tangent.Y += FaceTangent.Y;
		Vertices[I2].Tangent.Z += FaceTangent.Z;
		AccumulatedBitangent[I2] = AccumulatedBitangent[I2] + FaceBitangent;
	}

	for (size_t V = 0; V < Vertices.Num(); ++V)
	{
		FVector Normal = Vertices[V].Normal;
		FVector Tangent = FVector(Vertices[V].Tangent.X, Vertices[V].Tangent.Y, Vertices[V].Tangent.Z);
		// Gram-Schmidt 직교화
		Tangent = Tangent - Normal * Dot(Normal, Tangent);
		float TangentLength = sqrtf(Tangent.X * Tangent.X + Tangent.Y * Tangent.Y + Tangent.Z * Tangent.Z);
		if (TangentLength > 1e-8f)
		{
			Tangent = Tangent * (1.0f / TangentLength);
		}
		else
		{
			Tangent = MakeFallbackTangent(Normal);
		}

		FVector Bitangent = AccumulatedBitangent[V];

		float Handedness = (Dot(Cross(Normal, Tangent), Bitangent) < 0.0f) ? -1.0f : 1.0f;

		Vertices[V].Tangent = FVector4(Tangent.X, Tangent.Y, Tangent.Z, Handedness);
	}

}

// static 멤버 변수의 실체를 정의(메모리 할당)합니다.
TMap<FName, std::unique_ptr<FStaticMesh>> FObjManager::ObjFStaticMeshMap;
UMaterial* FObjManager::CachedDefaultMaterial = nullptr;

/** @brief: Vertex Key for creating index buffer */
using VertexKey = std::tuple<size_t, size_t, size_t>;

struct VertexKeyHash
{
	std::size_t operator() (VertexKey Key) const
	{
		auto Hash1 = std::hash<size_t>{}(std::get<0>(Key));
		auto Hash2 = std::hash<size_t>{}(std::get<1>(Key));
		auto Hash3 = std::hash<size_t>{}(std::get<2>(Key));

		std::size_t Seed = Hash1;
		Seed ^= Hash2 + 0x9e3779b97f4a7c15ULL + (Seed << 6) + (Seed >> 2);
		Seed ^= Hash3 + 0x9e3779b97f4a7c15ULL + (Seed << 6) + (Seed >> 2);

		return Seed;
	}
};

/** @todo: std::filesystem으로 변경 */
FStaticMesh* FObjManager::LoadObjStaticMeshAsset(const FName& PathFileName, const FObjImporter::Configuration& Config)
{
	auto Iter = ObjFStaticMeshMap.find(PathFileName);
	if (Iter != ObjFStaticMeshMap.end())
	{
		return Iter->second.get();
	}

	/** #1. '.obj' 파일로부터 오브젝트 정보를 로드 */
	FObjInfo ObjInfo;
	if (!FObjImporter::LoadObj(PathFileName.ToString(), &ObjInfo, Config))
	{
		UE_LOG_ERROR("파일 정보를 읽어오는데 실패했습니다: %s", PathFileName.ToString().c_str());
		return nullptr;
	}

	auto StaticMesh = std::make_unique<FStaticMesh>();
	StaticMesh->PathFileName = PathFileName;

	if (ObjInfo.ObjectInfoList.Num() == 0)
	{
		UE_LOG_ERROR("오브젝트 정보를 찾을 수 없습니다");
		return nullptr;
	}

	/** #2. 오브젝트 정보로부터 버텍스 배열과 인덱스 배열을 구성 */
	/** @note: Use only first object in '.obj' file to create FStaticMesh. */
	FObjectInfo& ObjectInfo = ObjInfo.ObjectInfoList[0];

	TMap<VertexKey, size_t, VertexKeyHash> VertexMap;
	for (size_t i = 0; i < ObjectInfo.VertexIndexList.Num(); ++i)
	{
		size_t VertexIndex = ObjectInfo.VertexIndexList[i];

		size_t NormalIndex = INVALID_INDEX;
		if (!ObjectInfo.NormalIndexList.IsEmpty())
		{
			NormalIndex = ObjectInfo.NormalIndexList[i];
		}

		size_t TexCoordIndex = INVALID_INDEX;
		if (!ObjectInfo.TexCoordIndexList.IsEmpty())
		{
			TexCoordIndex = ObjectInfo.TexCoordIndexList[i];
		}

		VertexKey Key{ VertexIndex, NormalIndex, TexCoordIndex };
		auto It = VertexMap.find(Key);
		if (It == VertexMap.end())
		{
			FNormalVertex Vertex = {};
			Vertex.Position = ObjInfo.VertexList[VertexIndex];

			if (NormalIndex != INVALID_INDEX)
			{
				assert("Vertex normal index out of range" && NormalIndex < ObjInfo.NormalList.Num());
				Vertex.Normal = ObjInfo.NormalList[NormalIndex];
			}

			if (TexCoordIndex != INVALID_INDEX)
			{
				assert("Texture coordinate index out of range" && TexCoordIndex < ObjInfo.TexCoordList.Num());
				Vertex.TexCoord = ObjInfo.TexCoordList[TexCoordIndex];
			}

			uint32 Index = static_cast<uint32>(StaticMesh->Vertices.Num());
			StaticMesh->Vertices.Add(Vertex);
			StaticMesh->Indices.Add(Index);
			VertexMap[Key] = Index;
		}
		else
		{
			StaticMesh->Indices.Add(static_cast<uint32>(It->second));
		}
	}
	ComputeTangents(StaticMesh->Vertices, StaticMesh->Indices);
	/** #3. 오브젝트가 사용하는 머티리얼의 목록을 저장 */
	TSet<FName> UniqueMaterialNames;
	for (const auto& MaterialName : ObjectInfo.MaterialNameList)
	{
		UniqueMaterialNames.insert(MaterialName);
	}

	StaticMesh->MaterialInfo.SetNum(UniqueMaterialNames.size());
	TMap<FName, int32> MaterialNameToSlot;
	int32 CurrentMaterialSlot = 0;

	for (const auto& MaterialName : UniqueMaterialNames)
	{
		for (size_t j = 0; j < ObjInfo.ObjectMaterialInfoList.Num(); ++j)
		{
			if (MaterialName == ObjInfo.ObjectMaterialInfoList[j].Name)
			{
				StaticMesh->MaterialInfo[CurrentMaterialSlot].Name = std::move(ObjInfo.ObjectMaterialInfoList[j].Name);
				StaticMesh->MaterialInfo[CurrentMaterialSlot].Ka = ObjInfo.ObjectMaterialInfoList[j].Ka;
				StaticMesh->MaterialInfo[CurrentMaterialSlot].Kd = ObjInfo.ObjectMaterialInfoList[j].Kd;
				StaticMesh->MaterialInfo[CurrentMaterialSlot].Ks = ObjInfo.ObjectMaterialInfoList[j].Ks;
				StaticMesh->MaterialInfo[CurrentMaterialSlot].Ke = ObjInfo.ObjectMaterialInfoList[j].Ke;
				StaticMesh->MaterialInfo[CurrentMaterialSlot].Ns = ObjInfo.ObjectMaterialInfoList[j].Ns;
				StaticMesh->MaterialInfo[CurrentMaterialSlot].Ni = ObjInfo.ObjectMaterialInfoList[j].Ni;
				StaticMesh->MaterialInfo[CurrentMaterialSlot].D = ObjInfo.ObjectMaterialInfoList[j].D;
				StaticMesh->MaterialInfo[CurrentMaterialSlot].Illumination = ObjInfo.ObjectMaterialInfoList[j].Illumination;
				StaticMesh->MaterialInfo[CurrentMaterialSlot].KaMap = std::move(ObjInfo.ObjectMaterialInfoList[j].KaMap);
				StaticMesh->MaterialInfo[CurrentMaterialSlot].KdMap = std::move(ObjInfo.ObjectMaterialInfoList[j].KdMap);
				StaticMesh->MaterialInfo[CurrentMaterialSlot].KsMap = std::move(ObjInfo.ObjectMaterialInfoList[j].KsMap);
				StaticMesh->MaterialInfo[CurrentMaterialSlot].NsMap = std::move(ObjInfo.ObjectMaterialInfoList[j].NsMap);
				StaticMesh->MaterialInfo[CurrentMaterialSlot].DMap = std::move(ObjInfo.ObjectMaterialInfoList[j].DMap);
				StaticMesh->MaterialInfo[CurrentMaterialSlot].BumpMap = std::move(ObjInfo.ObjectMaterialInfoList[j].BumpMap);

				MaterialNameToSlot.emplace(MaterialName, CurrentMaterialSlot);
				CurrentMaterialSlot++;
				break;
			}
		}
	}
	if (StaticMesh->MaterialInfo.IsEmpty())
	{
		// Use a shared default material name to prevent duplicates
		StaticMesh->MaterialInfo.SetNum(1);
		StaticMesh->MaterialInfo[0].Name = "DefaultMaterial";
		StaticMesh->MaterialInfo[0].Kd = FVector(0.9f, 0.9f, 0.9f);
		StaticMesh->MaterialInfo[0].Ka = FVector(0.2f, 0.2f, 0.2f);
		StaticMesh->MaterialInfo[0].Ks = FVector(0.5f, 0.5f, 0.5f);
		StaticMesh->MaterialInfo[0].Ns = 32.0f;
		StaticMesh->MaterialInfo[0].D = 1.0f;
	}
	
	/** #4. 오브젝트의 서브메쉬 정보를 저장 */
	if (ObjectInfo.MaterialNameList.IsEmpty())
	{
		StaticMesh->Sections.SetNum(1);
		StaticMesh->Sections[0].StartIndex = 0;
		StaticMesh->Sections[0].IndexCount = static_cast<uint32>(StaticMesh->Indices.Num());
		StaticMesh->Sections[0].MaterialSlot = 0;
	}
	else
	{
		StaticMesh->Sections.SetNum(ObjectInfo.MaterialIndexList.Num());
		for (size_t i = 0; i < ObjectInfo.MaterialIndexList.Num(); ++i)
		{
			StaticMesh->Sections[i].StartIndex = static_cast<uint32>(ObjectInfo.MaterialIndexList[i]) * 3;
			if (i < ObjectInfo.MaterialIndexList.Num() - 1)
			{
				StaticMesh->Sections[i].IndexCount = static_cast<uint32>(ObjectInfo.MaterialIndexList[i + 1] - ObjectInfo.MaterialIndexList[i]) * 3;
			}
			else
			{
				StaticMesh->Sections[i].IndexCount = static_cast<uint32>(StaticMesh->Indices.Num() / 3 - ObjectInfo.MaterialIndexList[i]) * 3;
			}

			const FName& MaterialName = ObjectInfo.MaterialNameList[i];
			auto It = MaterialNameToSlot.find(MaterialName);
			if (It != MaterialNameToSlot.end())
			{
				StaticMesh->Sections[i].MaterialSlot = It->second;
			}
			else
			{
				StaticMesh->Sections[i].MaterialSlot = static_cast<uint32>(INVALID_INDEX);
			}
		}
	}

	//StaticMesh->BVH.Build(StaticMesh.get()); // 빠른 피킹용 BVH 구축
	ObjFStaticMeshMap.emplace(PathFileName, std::move(StaticMesh));

	return ObjFStaticMeshMap[PathFileName].get();
}

/**
 * @brief MTL 정보를 바탕으로 UStaticMesh에 재질을 설정하는 함수
 */
void FObjManager::CreateMaterialsFromMTL(UStaticMesh* StaticMesh, FStaticMesh* StaticMeshAsset, const FName& ObjFilePath)
{
	if (!StaticMesh || !StaticMeshAsset || StaticMeshAsset->MaterialInfo.IsEmpty())
	{
		return;
	}

	// OBJ 파일이 있는 디렉토리 경로 추출
	std::filesystem::path ObjDirectory = std::filesystem::path(ObjFilePath.ToString()).parent_path();

	UAssetManager& AssetManager = UAssetManager::GetInstance();
	
	size_t MaterialCount = StaticMeshAsset->MaterialInfo.Num();
	for (size_t i = 0; i < MaterialCount; ++i)
	{
		const FMaterial& MaterialInfo = StaticMeshAsset->MaterialInfo[i];
		
		// Reuse cached DefaultMaterial to prevent duplicates
		UMaterial* Material = nullptr;
		if (MaterialInfo.Name == "DefaultMaterial")
		{
			if (!CachedDefaultMaterial)
			{
				CachedDefaultMaterial = NewObject<UMaterial>();
				CachedDefaultMaterial->SetName(MaterialInfo.Name);
				CachedDefaultMaterial->SetMaterialData(MaterialInfo);
			}
			Material = CachedDefaultMaterial;
		}
		else
		{
			Material = NewObject<UMaterial>();
			Material->SetName(MaterialInfo.Name);
			Material->SetMaterialData(MaterialInfo);
		}

		// Diffuse 텍스처 로드 (map_Kd)
		if (!MaterialInfo.KdMap.empty())
		{
			// .generic_string()을 사용하여 경로를 '/'로 통일하고 std::replace 제거
			FString TexturePathStr = (ObjDirectory / MaterialInfo.KdMap).generic_string();

			if (std::filesystem::exists(TexturePathStr))
			{
				UTexture* DiffuseTexture = AssetManager.LoadTexture(TexturePathStr);
				if (DiffuseTexture)
				{
					Material->SetDiffuseTexture(DiffuseTexture);
				}
			}
		}

		// Ambient 텍스처 로드 (map_Ka)
		if (!MaterialInfo.KaMap.empty())
		{
			FString TexturePathStr = (ObjDirectory / MaterialInfo.KaMap).generic_string();

			if (std::filesystem::exists(TexturePathStr))
			{
				UTexture* AmbientTexture = AssetManager.LoadTexture(TexturePathStr);
				if (AmbientTexture)
				{
					Material->SetAmbientTexture(AmbientTexture);
				}
			}
		}

		// Specular 텍스처 로드 (map_Ks)
		if (!MaterialInfo.KsMap.empty())
		{
			FString TexturePathStr = (ObjDirectory / MaterialInfo.KsMap).generic_string();

			if (std::filesystem::exists(TexturePathStr))
			{
				UTexture* SpecularTexture = AssetManager.LoadTexture(TexturePathStr);
				if (SpecularTexture)
				{
					Material->SetSpecularTexture(SpecularTexture);
				}
			}
		}

		// Alpha 텍스처 로드 (map_d)
		if (!MaterialInfo.DMap.empty())
		{
			FString TexturePathStr = (ObjDirectory / MaterialInfo.DMap).generic_string();

			if (std::filesystem::exists(TexturePathStr))
			{
				UTexture* AlphaTexture = AssetManager.LoadTexture(TexturePathStr);
				if (AlphaTexture)
				{
					Material->SetAlphaTexture(AlphaTexture);
				}
			}
		}
		// Normal(=map_Bump) 텍스처 로드
		if (!MaterialInfo.BumpMap.empty())
		{
			FString TexturePathStr = (ObjDirectory / MaterialInfo.BumpMap).generic_string();
			if (std::filesystem::exists(TexturePathStr))
			{
				UTexture* NormalMapTexture = AssetManager.LoadTexture(TexturePathStr);
				if (NormalMapTexture)
				{
					// 프로젝트 정책에 따라 Bump를 Normal로 사용
					Material->SetNormalTexture(NormalMapTexture);

					// 필요 시 bump 슬롯도 함께 사용하려면 아래 활성화
					// 하지만 지금은 필요없음
					// Material->SetBumpTexture(NormalMapTexture);
				}
			}

		}
		StaticMesh->SetMaterial(static_cast<int32>(i), Material);
	}
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const FName& PathFileName, const FObjImporter::Configuration& Config)
{
	// 1) Try AssetManager cache first (non-owning lookup)
	UAssetManager& AssetManager = UAssetManager::GetInstance();
	if (UStaticMesh* Cached = AssetManager.GetStaticMeshFromCache(PathFileName))
	{
		return Cached;
	}

	// 2) Load asset-level data (FStaticMesh) if not cached
	FStaticMesh* StaticMeshAsset = FObjManager::LoadObjStaticMeshAsset(PathFileName, Config);
	if (StaticMeshAsset)
	{
		// Create runtime UStaticMesh and register to AssetManager cache (ownership there)
		UStaticMesh* StaticMesh = new UStaticMesh();
		StaticMesh->SetStaticMeshAsset(StaticMeshAsset);

		// Create materials based on MTL information
		CreateMaterialsFromMTL(StaticMesh, StaticMeshAsset, PathFileName);

		// Register into AssetManager's cache (takes ownership)
		AssetManager.AddStaticMeshToCache(PathFileName, StaticMesh);

		return StaticMesh;
	}

	return nullptr;
}

void FObjManager::Release()
{
	// Clean up the cached default material to prevent memory leak
	if (CachedDefaultMaterial)
	{
		delete CachedDefaultMaterial;
		CachedDefaultMaterial = nullptr;
	}
}
