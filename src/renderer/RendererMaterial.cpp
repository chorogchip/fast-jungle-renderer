#include "FastJungle/renderer/RendererMaterial.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <cwctype>
#include <limits>
#include <stdexcept>
#include <utility>
#include <variant>
#include <objbase.h>
#include <d3d12.h>
#include <wrl.h>

#include "FastJungle/dx12/Buffer.hpp"
#include "FastJungle/dx12/Texture.hpp"
#include "FastJungle/cooker/JungleScene.hpp"

namespace fjr {

    namespace {

        using Scene = cooker::JungleScene;

        constexpr UINT MAX_TEXTURE_DIMENSION = 512;
        constexpr UINT TEXTURES_PER_MATERIAL = 4;

        float read_float(
            const Scene::ShaderInput* input,
            float fallback) {

            if (input == nullptr ||
                input->value.kind == Scene::ShaderValueKind::Empty) {
                return fallback;
            }
            if (input->value.kind != Scene::ShaderValueKind::Float) {
                throw std::runtime_error(
                    "Expected a float shader input named " + input->name);
            }
            return std::get<float>(input->value.data);
        }

        std::array<float, 3> read_float3(
            const Scene::ShaderInput* input,
            std::array<float, 3> fallback) {

            if (input == nullptr ||
                input->value.kind == Scene::ShaderValueKind::Empty) {
                return fallback;
            }
            if (input->value.kind != Scene::ShaderValueKind::Float3) {
                throw std::runtime_error(
                    "Expected a float3 shader input named " + input->name);
            }
            const auto& value = std::get<Scene::Float3>(input->value.data);
            return {value.x, value.y, value.z};
        }

        std::string read_string(const Scene::ShaderInput* input) {
            if (input == nullptr ||
                (input->value.kind != Scene::ShaderValueKind::String &&
                 input->value.kind != Scene::ShaderValueKind::Token)) {
                return {};
            }
            return std::get<std::string>(input->value.data);
        }

        std::uint32_t texture_channel(std::string_view output) {
            if (output == "g") {
                return 1;
            }
            if (output == "b") {
                return 2;
            }
            if (output == "a") {
                return 3;
            }
            return 0;
        }

        D3D12_TEXTURE_ADDRESS_MODE texture_address_mode(
            std::string_view mode) {

            if (mode.empty() || mode == "repeat" || mode == "useMetadata") {
                return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            }
            if (mode == "clamp") {
                return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            }
            if (mode == "mirror") {
                return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            }
            if (mode == "black") {
                return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            }
            throw std::runtime_error(
                "Unsupported Jungle texture wrap mode " +
                std::string{mode});
        }

        std::wstring normalized_texture_key(
            const std::filesystem::path& path) {

            auto result = path.lexically_normal().native();
            std::ranges::transform(result, result.begin(), [](wchar_t value) {
                return static_cast<wchar_t>(std::towlower(value));
            });
            return result;
        }

        void throw_if_failed(HRESULT result, const char* operation) {
            if (FAILED(result)) {
                throw std::runtime_error(operation);
            }
        }

    } // namespace

    MaterialResolver::MaterialResolver(const cooker::JungleScene& scene)
        : scene_(scene) {

        materials_.reserve(scene.materials.size());
        for (const auto& material : scene.materials) {
            materials_.emplace(material.prim_path, &material);
        }
        shaders_.reserve(scene.shader_nodes.size());
        for (const auto& shader : scene.shader_nodes) {
            shaders_.emplace(shader.prim_path, &shader);
        }
    }

    const Scene::ShaderNode& MaterialResolver::find_shader(
        const std::string& path) const {

        const auto shader = shaders_.find(path);
        if (shader == shaders_.end()) {
            throw std::runtime_error("Missing Jungle shader " + path);
        }
        return *shader->second;
    }

    const Scene::ShaderInput* MaterialResolver::find_input(
        const Scene::ShaderNode& shader,
        std::string_view name) const noexcept {

        const auto input = std::ranges::find(
            shader.inputs,
            name,
            &Scene::ShaderInput::name);
        return input == shader.inputs.end() ? nullptr : &*input;
    }

    MaterialDescription::TextureBinding MaterialResolver::resolve_texture(
        const Scene::ShaderInput& input,
        bool default_srgb,
        std::string& uv_primvar) const {

        if (input.connections.size() != 1) {
            throw std::runtime_error(
                "Jungle material input " + input.name +
                " does not have exactly one texture connection.");
        }
        const auto& connection = input.connections.front();
        const auto& texture = find_shader(connection.source_prim_path);
        if (texture.shader_id != "UsdUVTexture") {
            throw std::runtime_error(
                "Unsupported Jungle texture shader " + texture.shader_id);
        }

        MaterialDescription::TextureBinding result;
        result.source_output = connection.source_name;
        result.srgb = default_srgb;

        const auto* file = find_input(texture, "file");
        if (file == nullptr ||
            file->value.kind != Scene::ShaderValueKind::Asset) {
            throw std::runtime_error(
                "UsdUVTexture has no asset input: " + texture.prim_path);
        }
        const auto& asset = std::get<Scene::AssetReference>(file->value.data);
        result.path = asset.resolved_path.empty()
            ? std::filesystem::path{asset.authored_path}
            : std::filesystem::path{asset.resolved_path};
        if (!asset.resolved_file_exists || result.path.empty()) {
            throw std::runtime_error(
                "Jungle texture is unresolved: " + asset.authored_path);
        }

        const auto color_space = read_string(
            find_input(texture, "sourceColorSpace"));
        if (color_space == "sRGB") {
            result.srgb = true;
        }
        else if (color_space == "raw") {
            result.srgb = false;
        }

        result.address_u = texture_address_mode(
            read_string(find_input(texture, "wrapS")));
        result.address_v = texture_address_mode(
            read_string(find_input(texture, "wrapT")));

        const auto* st = find_input(texture, "st");
        if (st != nullptr && !st->connections.empty()) {
            if (st->connections.size() != 1) {
                throw std::runtime_error(
                    "UsdUVTexture has multiple st connections.");
            }
            const auto& reader = find_shader(
                st->connections.front().source_prim_path);
            if (reader.shader_id != "UsdPrimvarReader_float2") {
                throw std::runtime_error(
                    "Unsupported Jungle UV reader " + reader.shader_id);
            }
            const auto varname = read_string(find_input(reader, "varname"));
            if (varname.empty()) {
                throw std::runtime_error(
                    "Jungle UV reader has no varname: " + reader.prim_path);
            }
            if (!uv_primvar.empty() && uv_primvar != varname) {
                throw std::runtime_error(
                    "One Jungle material uses multiple UV primvars.");
            }
            uv_primvar = varname;
        }
        return result;
    }

    MaterialDescription MaterialResolver::resolve(
        std::string_view material_path) const {

        const auto material = materials_.find(std::string{material_path});
        if (material == materials_.end()) {
            throw std::runtime_error(
                "Missing Jungle material " + std::string{material_path});
        }

        const Scene::ShaderNode* surface = nullptr;
        const auto output = std::ranges::find(
            material->second->outputs,
            "surface",
            &Scene::ShaderOutput::name);
        if (output != material->second->outputs.end() &&
            output->connections.size() == 1) {
            surface = &find_shader(
                output->connections.front().source_prim_path);
        }
        if (surface == nullptr) {
            for (const auto shader_index : material->second->shader_nodes) {
                if (shader_index < scene_.shader_nodes.size() &&
                    scene_.shader_nodes[shader_index].shader_id ==
                        "UsdPreviewSurface") {
                    surface = &scene_.shader_nodes[shader_index];
                    break;
                }
            }
        }
        if (surface == nullptr || surface->shader_id != "UsdPreviewSurface") {
            throw std::runtime_error(
                "Jungle material has no UsdPreviewSurface: " +
                std::string{material_path});
        }

        MaterialDescription result;
        result.material_path = material_path;
        result.constants.base_color = {0.18f, 0.18f, 0.18f, 1.0f};

        const auto* diffuse = find_input(*surface, "diffuseColor");
        if (diffuse != nullptr && !diffuse->connections.empty()) {
            result.base_color = resolve_texture(
                *diffuse,
                true,
                result.uv_primvar);
            result.constants.base_color = {1.0f, 1.0f, 1.0f, 1.0f};
            result.constants.options[0] |=
                MaterialDescription::BaseColorTexture;
        }
        else {
            const auto color = read_float3(
                diffuse,
                {0.18f, 0.18f, 0.18f});
            result.constants.base_color = {
                color[0], color[1], color[2], 1.0f};
        }

        const auto* normal = find_input(*surface, "normal");
        if (normal != nullptr && !normal->connections.empty()) {
            result.normal = resolve_texture(
                *normal,
                false,
                result.uv_primvar);
            result.constants.options[0] |=
                MaterialDescription::NormalTexture;
        }

        const auto* roughness = find_input(*surface, "roughness");
        result.constants.emissive_roughness[3] = read_float(
            roughness,
            0.5f);
        if (roughness != nullptr && !roughness->connections.empty()) {
            result.roughness = resolve_texture(
                *roughness,
                false,
                result.uv_primvar);
            result.constants.options[0] |=
                MaterialDescription::RoughnessTexture;
            result.constants.options[1] = texture_channel(
                result.roughness.source_output);
        }

        result.constants.surface[0] = read_float(
            find_input(*surface, "metallic"),
            0.0f);

        const auto* opacity = find_input(*surface, "opacity");
        result.constants.surface[1] = read_float(opacity, 1.0f);
        if (opacity != nullptr && !opacity->connections.empty()) {
            result.opacity = resolve_texture(
                *opacity,
                false,
                result.uv_primvar);
            result.constants.options[0] |=
                MaterialDescription::OpacityTexture;
            result.constants.options[2] = texture_channel(
                result.opacity.source_output);
        }
        result.constants.surface[2] = read_float(
            find_input(*surface, "opacityThreshold"),
            0.0f);

        const auto* emissive = find_input(*surface, "emissiveColor");
        if (emissive != nullptr && !emissive->connections.empty()) {
            throw std::runtime_error(
                "Connected Jungle emissive textures are not supported.");
        }
        const auto emission = read_float3(
            emissive,
            {0.0f, 0.0f, 0.0f});
        result.constants.emissive_roughness[0] = emission[0];
        result.constants.emissive_roughness[1] = emission[1];
        result.constants.emissive_roughness[2] = emission[2];

        if (result.uv_primvar.empty()) {
            result.uv_primvar = "st";
        }
        return result;
    }

    struct TextureLoader::DecodedImage {
        UINT width = 0;
        UINT height = 0;
        std::vector<std::uint8_t> pixels;
    };

    struct TextureLoader::TextureRecord {
        std::filesystem::path path;
        dx::Texture texture;
    };

    TextureLoader::TextureLoader() = default;

    TextureLoader::~TextureLoader() {
        wic_factory_.Reset();
        if (uninitialize_com_) {
            CoUninitialize();
        }
    }

    void TextureLoader::init(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        std::size_t material_capacity) {

        if (device == nullptr || command_list == nullptr) {
            throw std::invalid_argument("Invalid texture loader device.");
        }
        if (material_capacity >
            std::numeric_limits<UINT>::max() / TEXTURES_PER_MATERIAL) {
            throw std::runtime_error("Too many Jungle materials.");
        }

        device_ = device;
        command_list_ = command_list;
        material_count_ = 0;
        loaded_texture_count_ = 0;
        fallback_binding_count_ = 0;
        textures_.clear();
        upload_buffers_.clear();
        texture_cache_.clear();

        const auto com_result = CoInitializeEx(
            nullptr,
            COINIT_MULTITHREADED);
        if (SUCCEEDED(com_result)) {
            uninitialize_com_ = true;
        }
        else if (com_result != RPC_E_CHANGED_MODE) {
            throw_if_failed(com_result, "CoInitializeEx failed.");
        }
        throw_if_failed(
            CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(wic_factory_.ReleaseAndGetAddressOf())),
            "Could not create the WIC imaging factory.");

        const auto descriptor_count = std::max<UINT>(
            TEXTURES_PER_MATERIAL,
            static_cast<UINT>(material_capacity) * TEXTURES_PER_MATERIAL);
        resource_heap_.init(
            device_,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            descriptor_count,
            true);
        sampler_heap_.init(
            device_,
            D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
            descriptor_count,
            true);

        DecodedImage white{1, 1, {255, 255, 255, 255}};
        DecodedImage normal{1, 1, {128, 128, 255, 255}};
        DecodedImage roughness{1, 1, {128, 128, 128, 255}};
        white_texture_ = create_texture(L"#white", {}, white);
        normal_texture_ = create_texture(L"#normal", {}, normal);
        roughness_texture_ = create_texture(L"#roughness", {}, roughness);
        loaded_texture_count_ = 0;
    }

    TextureLoader::DecodedImage TextureLoader::decode(
        const std::filesystem::path& path) const {

        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        throw_if_failed(
            wic_factory_->CreateDecoderFromFilename(
                path.c_str(),
                nullptr,
                GENERIC_READ,
                WICDecodeMetadataCacheOnDemand,
                decoder.ReleaseAndGetAddressOf()),
            "WIC could not open a Jungle texture.");

        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
        throw_if_failed(
            decoder->GetFrame(0, frame.ReleaseAndGetAddressOf()),
            "WIC could not read a Jungle texture frame.");

        UINT source_width = 0;
        UINT source_height = 0;
        throw_if_failed(
            frame->GetSize(&source_width, &source_height),
            "WIC could not read a Jungle texture size.");
        if (source_width == 0 || source_height == 0) {
            throw std::runtime_error("Jungle texture has an empty image.");
        }

        UINT width = source_width;
        UINT height = source_height;
        Microsoft::WRL::ComPtr<IWICBitmapSource> source;
        throw_if_failed(
            frame.As(&source),
            "WIC could not expose the Jungle texture frame.");
        if (width > MAX_TEXTURE_DIMENSION || height > MAX_TEXTURE_DIMENSION) {
            const auto scale = std::min(
                static_cast<double>(MAX_TEXTURE_DIMENSION) / width,
                static_cast<double>(MAX_TEXTURE_DIMENSION) / height);
            width = std::max<UINT>(1, static_cast<UINT>(width * scale));
            height = std::max<UINT>(1, static_cast<UINT>(height * scale));

            Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler;
            throw_if_failed(
                wic_factory_->CreateBitmapScaler(
                    scaler.ReleaseAndGetAddressOf()),
                "WIC could not create a texture scaler.");
            throw_if_failed(
                scaler->Initialize(
                    frame.Get(),
                    width,
                    height,
                    WICBitmapInterpolationModeFant),
                "WIC could not scale a Jungle texture.");
            throw_if_failed(
                scaler.As(&source),
                "WIC could not expose a scaled Jungle texture.");
        }

        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
        throw_if_failed(
            wic_factory_->CreateFormatConverter(
                converter.ReleaseAndGetAddressOf()),
            "WIC could not create a texture format converter.");
        throw_if_failed(
            converter->Initialize(
                source.Get(),
                GUID_WICPixelFormat32bppRGBA,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0,
                WICBitmapPaletteTypeCustom),
            "WIC could not convert a Jungle texture to RGBA8.");

        const auto stride64 = static_cast<std::uint64_t>(width) * 4u;
        const auto size64 = stride64 * height;
        if (stride64 > std::numeric_limits<UINT>::max() ||
            size64 > std::numeric_limits<UINT>::max()) {
            throw std::runtime_error("Decoded Jungle texture is too large.");
        }
        DecodedImage result;
        result.width = width;
        result.height = height;
        result.pixels.resize(static_cast<std::size_t>(size64));
        throw_if_failed(
            converter->CopyPixels(
                nullptr,
                static_cast<UINT>(stride64),
                static_cast<UINT>(size64),
                result.pixels.data()),
            "WIC could not copy Jungle texture pixels.");
        return result;
    }

    std::uint32_t TextureLoader::create_texture(
        std::wstring cache_key,
        const std::filesystem::path& path,
        const DecodedImage& image) {

        D3D12_RESOURCE_DESC description{};
        description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        description.Width = image.width;
        description.Height = image.height;
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        auto record = std::make_unique<TextureRecord>();
        record->path = path;
        record->texture.init(
            device_,
            description,
            dx::TextureType::texture2d,
            D3D12_RESOURCE_STATE_COPY_DEST);

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT row_count = 0;
        UINT64 row_size = 0;
        UINT64 upload_size = 0;
        device_->GetCopyableFootprints(
            &description,
            0,
            1,
            0,
            &footprint,
            &row_count,
            &row_size,
            &upload_size);

        dx::Buffer upload;
        upload.init(
            device_,
            upload_size,
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ);
        std::byte* mapped = nullptr;
        constexpr D3D12_RANGE no_read{0, 0};
        throw_if_failed(
            upload->Map(
                0,
                &no_read,
                reinterpret_cast<void**>(&mapped)),
            "Could not map a Jungle texture upload buffer.");
        const auto source_stride = static_cast<std::size_t>(image.width) * 4u;
        for (UINT row = 0; row < row_count; ++row) {
            std::memcpy(
                mapped + footprint.Offset +
                    static_cast<std::size_t>(row) * footprint.Footprint.RowPitch,
                image.pixels.data() +
                    static_cast<std::size_t>(row) * source_stride,
                source_stride);
        }
        const D3D12_RANGE written{0, static_cast<SIZE_T>(upload_size)};
        upload->Unmap(0, &written);

        D3D12_TEXTURE_COPY_LOCATION destination{};
        destination.pResource = record->texture.get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destination.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = upload.get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.PlacedFootprint = footprint;
        command_list_->CopyTextureRegion(
            &destination,
            0,
            0,
            0,
            &source,
            nullptr);
        record->texture.transition(
            command_list_,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        if (textures_.size() >= std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("Too many Jungle textures.");
        }
        const auto index = static_cast<std::uint32_t>(textures_.size());
        textures_.push_back(std::move(record));
        upload_buffers_.push_back(std::move(upload));
        texture_cache_.emplace(std::move(cache_key), index);
        ++loaded_texture_count_;
        return index;
    }

    std::uint32_t TextureLoader::find_or_load(
        const MaterialDescription::TextureBinding& binding,
        std::uint32_t fallback_texture,
        MaterialDescription& material,
        MaterialDescription::Flag flag) {

        if (binding.path.empty()) {
            material.constants.options[0] &= ~flag;
            return fallback_texture;
        }
        auto extension = binding.path.extension().wstring();
        std::ranges::transform(
            extension,
            extension.begin(),
            [](wchar_t value) {
                return static_cast<wchar_t>(std::towlower(value));
            });
        if (extension == L".exr") {
            const auto warning = std::wstring{
                L"Fast Jungle: EXR auxiliary map uses fallback: "} +
                binding.path.native() + L"\n";
            OutputDebugStringW(warning.c_str());
            material.constants.options[0] &= ~flag;
            ++fallback_binding_count_;
            return fallback_texture;
        }

        const auto key = normalized_texture_key(binding.path);
        const auto existing = texture_cache_.find(key);
        if (existing != texture_cache_.end()) {
            return existing->second;
        }
        return create_texture(key, binding.path, decode(binding.path));
    }

    void TextureLoader::create_srv(
        std::uint32_t texture_index,
        std::uint32_t descriptor_index,
        bool srgb) const {

        if (texture_index >= textures_.size()) {
            throw std::runtime_error("Invalid Jungle texture index.");
        }
        D3D12_SHADER_RESOURCE_VIEW_DESC description{};
        description.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        description.Format = srgb
            ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            : DXGI_FORMAT_R8G8B8A8_UNORM;
        description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        description.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(
            textures_[texture_index]->texture.get(),
            &description,
            resource_heap_.get_cpu_handle(descriptor_index));
    }

    void TextureLoader::create_sampler(
        const MaterialDescription::TextureBinding& binding,
        std::uint32_t descriptor_index) const {

        D3D12_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = binding.address_u;
        sampler.AddressV = binding.address_v;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        device_->CreateSampler(
            &sampler,
            sampler_heap_.get_cpu_handle(descriptor_index));
    }

    std::uint32_t TextureLoader::add_material(
        MaterialDescription& material) {

        if ((material_count_ + 1u) * TEXTURES_PER_MATERIAL >
            resource_heap_.get_capacity()) {
            throw std::runtime_error("Jungle material heap is full.");
        }
        const auto base = find_or_load(
            material.base_color,
            white_texture_,
            material,
            MaterialDescription::BaseColorTexture);
        const auto normal = find_or_load(
            material.normal,
            normal_texture_,
            material,
            MaterialDescription::NormalTexture);
        const auto roughness = find_or_load(
            material.roughness,
            roughness_texture_,
            material,
            MaterialDescription::RoughnessTexture);
        const auto opacity = find_or_load(
            material.opacity,
            white_texture_,
            material,
            MaterialDescription::OpacityTexture);

        const auto descriptor = material_count_ * TEXTURES_PER_MATERIAL;
        create_srv(
            base,
            descriptor,
            material.base_color.path.empty() || material.base_color.srgb);
        create_srv(normal, descriptor + 1u, false);
        create_srv(roughness, descriptor + 2u, false);
        create_srv(opacity, descriptor + 3u, false);
        create_sampler(material.base_color, descriptor);
        create_sampler(material.normal, descriptor + 1u);
        create_sampler(material.roughness, descriptor + 2u);
        create_sampler(material.opacity, descriptor + 3u);
        return material_count_++;
    }

    void TextureLoader::finish_uploads() {
        upload_buffers_.clear();
        command_list_ = nullptr;
    }

    ID3D12DescriptorHeap* TextureLoader::resource_heap()
        const noexcept {
        return resource_heap_.get_descriptor_heap();
    }

    ID3D12DescriptorHeap* TextureLoader::sampler_heap()
        const noexcept {
        return sampler_heap_.get_descriptor_heap();
    }

    D3D12_GPU_DESCRIPTOR_HANDLE TextureLoader::material_handle(
        std::uint32_t material_index) const noexcept {

        return resource_heap_.get_gpu_handle(
            material_index * TEXTURES_PER_MATERIAL);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE TextureLoader::sampler_handle(
        std::uint32_t material_index) const noexcept {

        return sampler_heap_.get_gpu_handle(
            material_index * TEXTURES_PER_MATERIAL);
    }

} // namespace fjr
