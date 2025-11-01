#include "pch.h"
#include "Manager/Asset/Public/TextureManager.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Texture/Public/Texture.h"
#include <DirectXTK/DDSTextureLoader.h>
#include <DirectXTK/WICTextureLoader.h>

#include "Manager/Path/Public/PathManager.h"

FTextureManager::FTextureManager() = default;

FTextureManager::~FTextureManager()
{
    for (auto& TextureCache: TextureCaches)
    {
        SafeDelete(TextureCache.second);
    }
    if (DefaultSampler)
    {
        SafeRelease(DefaultSampler);
    }
}

UTexture* FTextureManager::LoadTexture(const FName& InFilePath)
{
    // Path 정규화
    path InputPath(InFilePath.ToString());  // 사용자의 원본 입력
    path AbsolutePath;                            // 실제 파일을 찾을 때 사용할 절대 경로
    path RelativeKeyPath;                         // 캐시맵의 키로 사용할 상대 경로

    // 절대 경로 생성
    path RootPath = UPathManager::GetInstance().GetRootPath();
    InputPath.is_relative() ? AbsolutePath = RootPath / InputPath : AbsolutePath = InputPath;

    try
    {
        path CanonicalPath = canonical(AbsolutePath);
        RelativeKeyPath = relative(CanonicalPath, RootPath);
    }
    catch (const filesystem::filesystem_error&)
    {
        RelativeKeyPath = InputPath;
    }
    FName CacheKey(RelativeKeyPath.string());

    // Check Cached
    const auto* FoundValuePtr = TextureCaches.Find(CacheKey);
    if (FoundValuePtr)
    {
        return *FoundValuePtr;
    }

    // Not Cached
    ComPtr<ID3D11ShaderResourceView> SRV = CreateTextureFromFile(AbsolutePath.string());

    if (!DefaultSampler)
    {
        DefaultSampler = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
        UE_LOG("[TextureManager] Default Sampler Create");
    }
    
    UTexture* Texture = NewObject<UTexture>();
    Texture->SetFilePath(CacheKey);
    Texture->CreateRenderProxy(SRV, DefaultSampler);

    UTexture** ExistingTexturePtr = TextureCaches.Find(CacheKey);
    if (ExistingTexturePtr)
    {
        SafeDelete(*ExistingTexturePtr);
    }

    TextureCaches[CacheKey] = Texture;
    return Texture;
}

void FTextureManager::LoadAllTexturesFromDirectory(const path& InDirectoryPath)
{
    if (!std::filesystem::exists(InDirectoryPath) || !std::filesystem::is_directory(InDirectoryPath))
    {
        UE_LOG_ERROR("[TextureManager] 디렉토리를 찾을 수 없습니다: %ls", InDirectoryPath.c_str());
        return;
    }

    UE_LOG("[TextureManager] %ls 디렉토리에서 텍스처 로드를 시작합니다...", InDirectoryPath.c_str());

    // 가져올 확장자 목록
    const TSet<FString> SupportedExtensions = { ".png", ".dds", ".jpg", ".jpeg", ".bmp", ".tiff" };

    // 디렉토리의 모든 파일 순회
    for (const auto& Entry : std::filesystem::recursive_directory_iterator(InDirectoryPath))
    {
        // 파일이 아니면 건너뜀
        if (!Entry.is_regular_file()) { continue; }

        const path& FilePath = Entry.path();
        FString Extension = FilePath.extension().string();
        std::ranges::transform(Extension, Extension.begin(), ::tolower);

        if (SupportedExtensions.Contains(Extension))
        {
            FName TextureName(FilePath.string());
            UTexture* Texture = LoadTexture(TextureName);
            Texture->GetTextureSRV()->AddRef();
            int temp = Texture->GetTextureSRV()->Release();
            int y =1;
        }
    }

    UE_LOG("[TextureManager] 텍스처 로드를 완료했습니다.");
}

const TMap<FName, UTexture*>& FTextureManager::GetTextureCache() const
{
    return TextureCaches;
}

ComPtr<ID3D11ShaderResourceView> FTextureManager::CreateTextureFromFile(const path& InFilePath)
{
    URenderer& Renderer = URenderer::GetInstance();
    ID3D11Device* Device = Renderer.GetDevice();
    ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();

    if (!Device || !DeviceContext)
    {
        UE_LOG_ERROR("TextureManager: Texture 생성 실패 - Device 또는 DeviceContext가 null입니다");
        return nullptr;
    }

    FString FileExtension = InFilePath.extension().string();
    transform(FileExtension.begin(), FileExtension.end(), FileExtension.begin(), ::tolower);

    ComPtr<ID3D11ShaderResourceView> TextureSRV = nullptr;
    HRESULT ResultHandle;

    try
    {
        // DDS
        if (FileExtension == ".dds")
        {
            ResultHandle = DirectX::CreateDDSTextureFromFile(Device, DeviceContext,
                InFilePath.c_str(), nullptr, TextureSRV.GetAddressOf());

            if (SUCCEEDED(ResultHandle))
            {
                UE_LOG_SUCCESS("TextureManager: DDS 텍스처 로드 성공 - %ls", InFilePath.c_str());
            }
            else
            {
                UE_LOG_ERROR("TextureManager: DDS 텍스처 로드 실패 - %ls (HRESULT: 0x%08lX)", InFilePath.c_str(), ResultHandle);
            }
        }
        else
        {
            // PNG, JPG, BMP, TIFF 등
            ResultHandle = DirectX::CreateWICTextureFromFile(Device, DeviceContext,
                InFilePath.c_str(), nullptr, &TextureSRV);

            if (SUCCEEDED(ResultHandle))
            {
                UE_LOG_SUCCESS("TextureManager: WIC 텍스처 로드 성공 - %ls", InFilePath.c_str());
            }
            else
            {
                UE_LOG_ERROR("TextureManager: WIC 텍스처 로드 실패 - %ls (HRESULT: 0x%08lX)" , InFilePath.c_str(), ResultHandle);
            }
        }
    }
    catch (const exception& Exception)
    {
        UE_LOG_ERROR("TextureManager: 텍스처 로드 중 예외 발생 - %ls: %s", InFilePath.c_str(), Exception.what());
        return nullptr;
    }
    return SUCCEEDED(ResultHandle) ? TextureSRV : nullptr;
}
