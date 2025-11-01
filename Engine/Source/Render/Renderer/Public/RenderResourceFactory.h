#pragma once
#include "Render/Renderer/Public/Renderer.h"

class FRenderResourceFactory
{
public:

	static void CreateStructuredShaderResourceView(ID3D11Buffer* Buffer, ID3D11ShaderResourceView** OutSRV);
	static void CreateUnorderedAccessView(ID3D11Buffer* Buffer, ID3D11UnorderedAccessView** OutUAV);

	static void CreateVertexShaderAndInputLayout(const wstring& InFilePath, const TArray<D3D11_INPUT_ELEMENT_DESC>& InInputLayoutDescriptions,
												 ID3D11VertexShader** OutVertexShader, ID3D11InputLayout** OutInputLayout);
	static void CreateVertexShaderAndInputLayout(const wstring& InFilePath, const TArray<D3D11_INPUT_ELEMENT_DESC>& InInputLayoutDescriptions,
												 ID3D11VertexShader** OutVertexShader, ID3D11InputLayout** OutInputLayout,
												 const char* InEntryPoint, const D3D_SHADER_MACRO* InMacros = nullptr);
	static ID3D11Buffer* CreateVertexBuffer(FNormalVertex* InVertices, uint32 InByteWidth);
	static ID3D11Buffer* CreateVertexBuffer(FVector* InVertices, uint32 InByteWidth, bool bCpuAccess);
	static ID3D11Buffer* CreateIndexBuffer(const void* InIndices, uint32 InByteWidth);
	static void CreatePixelShader(const wstring& InFilePath, ID3D11PixelShader** InPixelShader);
	static void CreatePixelShader(const wstring& InFilePath, ID3D11PixelShader** InPixelShader,
		const char* InEntryPoint, const D3D_SHADER_MACRO* InMacros = nullptr);
	static void CreateComputeShader(const wstring& InFilePath, ID3D11ComputeShader** OutComputeShader,
		const char* InEntryPoint = "main", const D3D_SHADER_MACRO* InMacros = nullptr);
	static ID3D11SamplerState* CreateSamplerState(D3D11_FILTER InFilter, D3D11_TEXTURE_ADDRESS_MODE InAddressMode);
	static ID3D11SamplerState* CreateFXAASamplerState();
	static ID3D11RasterizerState* GetRasterizerState(const FRenderState& InRenderState);
	static void ReleaseRasterizerState();


	// Helper function
	static D3D11_CULL_MODE ToD3D11(ECullMode InCull);
	static D3D11_FILL_MODE ToD3D11(EFillMode InFill);

	template<typename T>
	static ID3D11Buffer* CreateConstantBuffer()
	{
		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = (sizeof(T) + 0xf) & 0xfffffff0;
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

		ID3D11Buffer* Buffer = nullptr;
		URenderer::GetInstance().GetDevice()->CreateBuffer(&Desc, nullptr, &Buffer);
		return Buffer;
	}

	template<typename T>
	static void UpdateConstantBufferData(ID3D11Buffer* Buffer, const T& Data)
	{
		D3D11_MAPPED_SUBRESOURCE MappedResource = {};
		URenderer::GetInstance().GetDeviceContext()->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
		memcpy(MappedResource.pData, &Data, sizeof(T));
		URenderer::GetInstance().GetDeviceContext()->Unmap(Buffer, 0);
	}
	template<typename T>
	static void UpdateStructuredBuffer(ID3D11Buffer* Buffer, const TArray<T>& Datas)
	{
		D3D11_MAPPED_SUBRESOURCE MappedResource = {};
		URenderer::GetInstance().GetDeviceContext()->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
		memcpy(MappedResource.pData, Datas.GetData(), sizeof(T) * Datas.Num());
		URenderer::GetInstance().GetDeviceContext()->Unmap(Buffer, 0);
	}

	template<typename T>
	static void UpdateVertexBufferData(ID3D11Buffer* InVertexBuffer, const TArray<T>& InVertices)
	{
		if (!URenderer::GetInstance().GetDeviceContext() || !InVertexBuffer || InVertices.empty()) return;

		D3D11_MAPPED_SUBRESOURCE MappedResource = {};
		if (FAILED(URenderer::GetInstance().GetDeviceContext()->Map(InVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource))) return;

		memcpy(MappedResource.pData, InVertices.data(), sizeof(T) * InVertices.size());
		URenderer::GetInstance().GetDeviceContext()->Unmap(InVertexBuffer, 0);
	}

	//GPU 읽기전용 Structured Buffer
	//16byte align 필수x
	template<typename T>
	static ID3D11Buffer* CreateStructuredBuffer(int Count)
	{
		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = sizeof(T) * Count;
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		Desc.StructureByteStride = sizeof(T);

		ID3D11Buffer* Buffer = nullptr;
		URenderer::GetInstance().GetDevice()->CreateBuffer(&Desc, nullptr, &Buffer);
		return Buffer;
	}

	//ComputeShader 읽기 쓰기 다 가능한 RWStructuredBuffer
	template<typename T>
	static ID3D11Buffer* CreateRWStructuredBuffer(int Count)
	{

		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = sizeof(T) * Count;
		int size = sizeof(T);

		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.CPUAccessFlags = 0;
		Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		Desc.StructureByteStride = sizeof(T);

		ID3D11Buffer* Buffer = nullptr;
		URenderer::GetInstance().GetDevice()->CreateBuffer(&Desc, nullptr, &Buffer);
		return Buffer;
	}
	static ID3D11Buffer* CreateRWStructuredBuffer(uint32 Size, uint32 Count)
	{

		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = Size * Count;

		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.CPUAccessFlags = 0;
		Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		Desc.StructureByteStride = Size;

		ID3D11Buffer* Buffer = nullptr;
		URenderer::GetInstance().GetDevice()->CreateBuffer(&Desc, nullptr, &Buffer);
		return Buffer;
	}

private:
	struct FRasterKey
	{
		D3D11_FILL_MODE FillMode = {};
		D3D11_CULL_MODE CullMode = {};

		bool operator==(const FRasterKey& InKey) const
		{
			return FillMode == InKey.FillMode && CullMode == InKey.CullMode;
		}
	};

	struct FRasterKeyHasher
	{
		size_t operator()(const FRasterKey& InKey) const noexcept
		{
			auto Mix = [](size_t& H, size_t V)
			{
				H ^= V + 0x9e3779b97f4a7c15ULL + (H << 6) + (H << 2);
			};

			size_t H = 0;
			Mix(H, (size_t)InKey.FillMode);
			Mix(H, (size_t)InKey.CullMode);

			return H;
		}
	};

	static TMap<FRasterKey, ID3D11RasterizerState*, FRasterKeyHasher> RasterCache;
};
