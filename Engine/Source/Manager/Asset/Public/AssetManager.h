#pragma once

#include "ObjImporter.h"
#include "TextureManager.h"
#include "Component/Mesh/Public/StaticMesh.h"

struct FAABB;

/**
 * @brief 전역의 On-Memory Asset을 관리하는 매니저 클래스
 */
UCLASS()
class UAssetManager	: public UObject
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(UAssetManager, UObject)

public:
	void Initialize();
	void Release();

	// Vertex 관련 함수들
	TArray<FNormalVertex>* GetVertexData(EPrimitiveType InType);
	ID3D11Buffer* GetVertexbuffer(EPrimitiveType InType);
	uint32 GetNumVertices(EPrimitiveType InType);

	// Index 관련 함수들
	TArray<uint32>* GetIndexData(EPrimitiveType InType);
	ID3D11Buffer* GetIndexBuffer(EPrimitiveType InType);
	uint32 GetNumIndices(EPrimitiveType InType);

	// StaticMesh 관련 함수
	void LoadAllObjStaticMesh();
	ID3D11Buffer* GetVertexBuffer(FName InObjPath);
	ID3D11Buffer* GetIndexBuffer(FName InObjPath);

	// StaticMesh Cache Accessors
	UStaticMesh* GetStaticMeshFromCache(const FName& InObjPath);
	void AddStaticMeshToCache(const FName& InObjPath, UStaticMesh* InStaticMesh);

	// Bounding Box
	FAABB& GetAABB(EPrimitiveType InType);
	FAABB& GetStaticMeshAABB(FName InName);

private:
	// Vertex Resource
	TMap<EPrimitiveType, ID3D11Buffer*> VertexBuffers;
	TMap<EPrimitiveType, uint32> NumVertices;
	TMap<EPrimitiveType, TArray<FNormalVertex>*> VertexDatas;

	// 인덱스 리소스
	TMap<EPrimitiveType, ID3D11Buffer*> IndexBuffers;
	TMap<EPrimitiveType, uint32> NumIndices;
	TMap<EPrimitiveType, TArray<uint32>*> IndexDatas;

	// Texture Resource

	// StaticMesh Resource
	TMap<FName, UStaticMesh*> StaticMeshCache;
	TMap<FName, ID3D11Buffer*> StaticMeshVertexBuffers;
	TMap<FName, ID3D11Buffer*> StaticMeshIndexBuffers;

	// Helper Functions
	ID3D11Buffer* CreateVertexBuffer(TArray<FNormalVertex> InVertices);
	ID3D11Buffer* CreateIndexBuffer(TArray<uint32> InIndices);
	FAABB CalculateAABB(const TArray<FNormalVertex>& Vertices);

	// AABB Resource
	TMap<EPrimitiveType, FAABB> AABBs;		// 각 타입별 AABB 저장
	TMap<FName, FAABB> StaticMeshAABBs;	// 스태틱 메시용 AABB 저장

// Texture Section
public:
	UTexture* LoadTexture(const FName& InFilePath);
	const TMap<FName, UTexture*>& GetTextureCache() const;

private:
	FTextureManager* TextureManager;
};
