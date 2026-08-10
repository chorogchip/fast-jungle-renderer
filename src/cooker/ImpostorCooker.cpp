#include "FastJungle/cooker/ImpostorCooker.hpp"

#include <DirectXMath.h>
#include <DirectXTex.h>
#if defined(FASTJUNGLE_HAS_OPENEXR)
#include <DirectXTexEXR.h>
#endif

#include <Windows.h>
#include <objbase.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "FastJungle/core/util/Assume.h"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/Buffer.hpp"
#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/CommandQueue.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/dx12/DeviceUtils.hpp"
#include "FastJungle/dx12/FormatUtils.hpp"
#include "FastJungle/dx12/PSOUtils.hpp"
#include "FastJungle/dx12/ResourceUploader.hpp"
#include "FastJungle/dx12/RootSignatureBuilder.hpp"
#include "FastJungle/dx12/Shader.hpp"
#include "FastJungle/dx12/Texture.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"

namespace fjr::cooker {
    namespace {
        using StaticScene = scene::StaticScene;

        constexpr std::uint32_t DIRECTION_COUNT = 8;
        constexpr float LOD_ENTRY_PIXELS = 4.0f;
        constexpr float TEXELS_PER_ENTRY_PIXEL = 1.25f;
        constexpr std::uint32_t BORDER_TEXELS = 4;
        constexpr std::uint32_t DIMENSION_ALIGNMENT = 16;
        constexpr float TWO_PI = 6.28318530717958647692f;

        constexpr std::array TARGET_MESH_NAMES{
            std::string_view{"RiverForest_03_Animated_Translucent"},
            std::string_view{"RiverForest_01_Animated_Translucent"},
            std::string_view{"RiverForest_05_Animated_Translucent"},
            std::string_view{"RiverSapling_01_Translucent"},
            std::string_view{"RiverSapling_02_Translucent"},
            std::string_view{"RiverSapling_03_Translucent"},
            std::string_view{"RiverSapling_04_Translucent"},
            std::string_view{"RiverSapling_05_Translucent"},
            std::string_view{"QueenForest_02_Animated_Translucent"},
            std::string_view{"QueenForest_05_Animated_Translucent"},
            std::string_view{"QueenForest_06_Animated_Translucent"},
            std::string_view{"RiverSeedling_01_Translucent"},
            std::string_view{"RiverSeedling_02_Translucent"},
            std::string_view{"RiverSeedling_03_Translucent"},
            std::string_view{"RiverSeedling_04_Translucent"},
            std::string_view{"RiverSeedling_05_Translucent"},
            std::string_view{"Moss_01"},
            std::string_view{"Moss_02"},
            std::string_view{"Moss_03"},
            std::string_view{"Shrub_01"},
            std::string_view{"Shrub_02"},
            std::string_view{"Shrub_03"},
            std::string_view{"Shrub_04"},
            std::string_view{"ShrubSorrel_01_Translucent"},
            std::string_view{"ShrubSorrel_02_Translucent"},
            std::string_view{"ShrubSorrel_03_Translucent"},
            std::string_view{"ShrubSorrel_04_Translucent"},
            std::string_view{"ShrubSorrel_05_Translucent"},
            std::string_view{"ShrubSorrel_06_Translucent"},
            std::string_view{"ShrubSorrel_07_Translucent"},
            std::string_view{"Nettle_01_Translucent"},
            std::string_view{"Nettle_02_Translucent"},
            std::string_view{"Nettle_03_Translucent"},
            std::string_view{"Nettle_04_Translucent"},
            std::string_view{"Nettle_05_Translucent"},
            std::string_view{"Nettle_06_Translucent"},
            std::string_view{"Anthurium_01_Translucent"},
            std::string_view{"Anthurium_02_Translucent"},
            std::string_view{"Anthurium_03_Translucent"},
            std::string_view{"Anthurium_04_Translucent"},
            std::string_view{"Anthurium_05_Translucent"},
            std::string_view{"Anthurium_06_Translucent"},
            std::string_view{"Grass_A_01"},
            std::string_view{"Grass_A_02"},
            std::string_view{"Grass_A_03"},
            std::string_view{"Grass_A_04"},
            std::string_view{"Grass_A_05"},
            std::string_view{"Grass_A_06"},
            std::string_view{"Grass_B_01"},
            std::string_view{"Grass_B_02"},
            std::string_view{"Grass_B_03"},
            std::string_view{"Grass_B_04"},
            std::string_view{"Grass_B_05"},
        };

        class ComScope final {
        public:
            ComScope() {
                const HRESULT result = CoInitializeEx(
                    nullptr,
                    COINIT_MULTITHREADED);
                if (SUCCEEDED(result)) {
                    uninitialize_ = true;
                }
                else if (result != RPC_E_CHANGED_MODE) {
                    log::Logger::g_logger << log::abrt(
                        "CoInitializeEx failed for impostor baking.");
                }
            }

            ~ComScope() {
                if (uninitialize_) {
                    CoUninitialize();
                }
            }

            ComScope(const ComScope&) = delete;
            ComScope& operator=(const ComScope&) = delete;

        private:
            bool uninitialize_ = false;
        };

        struct Bounds final {
            DirectX::XMFLOAT3 minimum{};
            DirectX::XMFLOAT3 maximum{};
            bool initialized = false;

            void add(const DirectX::XMFLOAT3& point) noexcept {
                if (!initialized) {
                    minimum = point;
                    maximum = point;
                    initialized = true;
                    return;
                }
                minimum.x = (std::min)(minimum.x, point.x);
                minimum.y = (std::min)(minimum.y, point.y);
                minimum.z = (std::min)(minimum.z, point.z);
                maximum.x = (std::max)(maximum.x, point.x);
                maximum.y = (std::max)(maximum.y, point.y);
                maximum.z = (std::max)(maximum.z, point.z);
            }

            [[nodiscard]] DirectX::XMFLOAT3 center() const noexcept {
                return {
                    (minimum.x + maximum.x) * 0.5f,
                    (minimum.y + maximum.y) * 0.5f,
                    (minimum.z + maximum.z) * 0.5f,
                };
            }
        };

        struct BakeDirection final {
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            DirectX::XMFLOAT3 forward{};
            DirectX::XMFLOAT3 right{};
            float card_width = 0.0f;
            float card_height = 0.0f;
        };

        struct BakeTarget final {
            std::uint32_t mesh = StaticScene::INVALID_INDEX;
            std::string name;
            Bounds bounds;
            float radius = 0.0f;
            float depth_min = 0.0f;
            float depth_range = 0.0f;
            float constant_roughness = 0.5f;
            bool bake_roughness = false;
            std::array<BakeDirection, DIRECTION_COUNT> directions{};
        };

        struct BakedDirection final {
            DirectX::ScratchImage albedo;
            DirectX::ScratchImage opacity;
            DirectX::ScratchImage normal;
            DirectX::ScratchImage depth;
            DirectX::ScratchImage roughness;
        };

        struct BakedTarget final {
            std::array<BakedDirection, DIRECTION_COUNT> directions{};
        };

        enum class RootParameter : std::uint32_t {
            CAMERA,
            MATERIAL,
            TEXTURES,
            SAMPLER,
            COUNT,
        };

        struct BakeCameraConstants final {
            DirectX::XMFLOAT4X4 object_to_view{};
            DirectX::XMFLOAT4X4 object_to_clip{};
        };
        static_assert(sizeof(BakeCameraConstants) == 128);

        struct BakeMaterialConstants final {
            DirectX::XMFLOAT4 base_color_opacity{};
            std::uint32_t base_color_texture = StaticScene::INVALID_INDEX;
            std::uint32_t opacity_texture = StaticScene::INVALID_INDEX;
            std::uint32_t base_color_channel = 0;
            std::uint32_t opacity_channel = 0;
            float roughness_value = 0.5f;
            std::uint32_t roughness_texture = StaticScene::INVALID_INDEX;
            std::uint32_t roughness_channel = 0;
            std::uint32_t normal_texture = StaticScene::INVALID_INDEX;
        };
        static_assert(sizeof(BakeMaterialConstants) == 48);

        struct TextureSource final {
            dx::Texture texture;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
            UINT mip_count = 0;
        };

        struct Readback final {
            dx::Buffer buffer;
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
            UINT row_count = 0;
            UINT64 row_size = 0;
        };

        [[nodiscard]] std::uint32_t checked_u32(
            std::size_t value,
            std::string_view) {
            return static_cast<std::uint32_t>(value);
        }

        [[nodiscard]] std::string_view string_at(
            const StaticScene& scene,
            std::uint32_t offset) {
            return scene.strings.data() + offset;
        }

        [[nodiscard]] std::uint32_t append_string(
            StaticScene& scene,
            std::string_view value) {
            const auto offset = checked_u32(
                scene.strings.size(),
                "Impostor string offset");
            scene.strings.insert(
                scene.strings.end(),
                value.begin(),
                value.end());
            scene.strings.push_back('\0');
            return offset;
        }

        uint32_t append_texture(
            StaticScene& scene,
            std::string_view name,
            std::string_view key) {
            const auto texture = checked_u32(
                scene.textures.size(),
                "Impostor texture index");
            scene.textures.push_back({
                .name = append_string(scene, name),
            });
            scene.texture_payload_refs.push_back({
                .texture = texture,
                .key = append_string(scene, key),
            });
            return texture;
        }

        [[nodiscard]] std::uint32_t ensure_sampler(StaticScene& scene) {
            if (!scene.samplers.empty()) {
                return 0;
            }
            scene.samplers.push_back({
                .filter = StaticScene::EnumSamplerFilter::MIN_MAG_MIP_LINEAR,
                .address_u = StaticScene::EnumSamplerAddressMode::CLAMP,
                .address_v = StaticScene::EnumSamplerAddressMode::CLAMP,
                .address_w = StaticScene::EnumSamplerAddressMode::CLAMP,
                .max_anisotropy = 1,
            });
            return 0;
        }

        [[nodiscard]] std::uint32_t aligned_dimension(float desired) {
            const auto unaligned = static_cast<std::uint32_t>(std::ceil(
                (std::max)(desired, 1.0f)));
            const auto with_alignment =
                (unaligned + DIMENSION_ALIGNMENT - 1) /
                DIMENSION_ALIGNMENT * DIMENSION_ALIGNMENT;
            return (std::max)(with_alignment, DIMENSION_ALIGNMENT);
        }

        [[nodiscard]] Bounds mesh_bounds(
            const StaticScene& scene,
            std::uint32_t mesh_index) {
            const auto& mesh = scene.meshes[mesh_index];
            const auto& lod = scene.mesh_lods[mesh.lod_offset];

            Bounds result;
            for (std::uint32_t local_submesh = 0;
                local_submesh < lod.submesh_count;
                ++local_submesh) {
                const auto& submesh = scene.submeshes[
                    lod.submesh_offset + local_submesh];
                for (std::uint32_t vertex = 0;
                    vertex < submesh.vertex_count;
                    ++vertex) {
                    result.add(scene.vertices[
                        submesh.vertex_offset + vertex].position);
                }
            }
            return result;
        }

        [[nodiscard]] float bounds_radius(const Bounds& bounds) noexcept {
            const auto center = bounds.center();
            const std::array corners{
                DirectX::XMFLOAT3{bounds.minimum.x, bounds.minimum.y, bounds.minimum.z},
                DirectX::XMFLOAT3{bounds.minimum.x, bounds.minimum.y, bounds.maximum.z},
                DirectX::XMFLOAT3{bounds.minimum.x, bounds.maximum.y, bounds.minimum.z},
                DirectX::XMFLOAT3{bounds.minimum.x, bounds.maximum.y, bounds.maximum.z},
                DirectX::XMFLOAT3{bounds.maximum.x, bounds.minimum.y, bounds.minimum.z},
                DirectX::XMFLOAT3{bounds.maximum.x, bounds.minimum.y, bounds.maximum.z},
                DirectX::XMFLOAT3{bounds.maximum.x, bounds.maximum.y, bounds.minimum.z},
                DirectX::XMFLOAT3{bounds.maximum.x, bounds.maximum.y, bounds.maximum.z},
            };
            float radius_squared = 0.0f;
            for (const auto& corner : corners) {
                const float x = corner.x - center.x;
                const float y = corner.y - center.y;
                const float z = corner.z - center.z;
                radius_squared = (std::max)(radius_squared, x * x + y * y + z * z);
            }
            return std::sqrt(radius_squared);
        }

        [[nodiscard]] float directional_extent(
            const Bounds& bounds,
            const DirectX::XMFLOAT3& axis) noexcept {
            return std::abs(axis.x) * (bounds.maximum.x - bounds.minimum.x) +
                std::abs(axis.y) * (bounds.maximum.y - bounds.minimum.y) +
                std::abs(axis.z) * (bounds.maximum.z - bounds.minimum.z);
        }

        [[nodiscard]] std::uint32_t texture_from_binding(
            const StaticScene& scene,
            std::uint32_t binding) {
            if (binding == StaticScene::INVALID_INDEX) {
                return StaticScene::INVALID_INDEX;
            }
            return scene.texture_bindings[binding].texture;
        }

        [[nodiscard]] BakeMaterialConstants material_constants(
            const StaticScene& scene,
            const StaticScene::Material& material) {
            const auto binding = [&scene](std::uint32_t binding_index)
                -> const StaticScene::TextureBinding* {
                if (binding_index == StaticScene::INVALID_INDEX) {
                    return nullptr;
                }
                return &scene.texture_bindings[binding_index];
            };

            BakeMaterialConstants result;
            result.base_color_opacity = {
                material.base_color.x,
                material.base_color.y,
                material.base_color.z,
                material.base_color.w * material.opacity,
            };
            result.roughness_value = material.roughness;

            if (const auto* base = binding(material.texture_binding_base_color)) {
                result.base_color_texture = base->texture;
                result.base_color_channel = static_cast<std::uint32_t>(base->channel);
            }
            if (const auto* opacity = binding(material.texture_binding_opacity)) {
                result.opacity_texture = opacity->texture;
                result.opacity_channel = static_cast<std::uint32_t>(opacity->channel);
            }
            if (const auto* roughness = binding(material.texture_binding_roughness)) {
                result.roughness_texture = roughness->texture;
                result.roughness_channel = static_cast<std::uint32_t>(roughness->channel);
            }
            if (const auto* normal = binding(material.texture_binding_normal)) {
                result.normal_texture = normal->texture;
            }
            return result;
        }

        [[nodiscard]] bool needs_roughness_bake(
            const StaticScene& scene,
            std::uint32_t mesh_index,
            float& constant_roughness) {
            const auto& mesh = scene.meshes[mesh_index];
            const auto& lod = scene.mesh_lods[mesh.lod_offset];
            bool initialized = false;
            bool variable = false;
            for (std::uint32_t local_submesh = 0;
                local_submesh < lod.submesh_count;
                ++local_submesh) {
                const auto& submesh = scene.submeshes[
                    lod.submesh_offset + local_submesh];
                const auto& material = scene.materials[submesh.material];
                variable = variable ||
                    material.texture_binding_roughness != StaticScene::INVALID_INDEX;
                if (!initialized) {
                    initialized = true;
                    constant_roughness = material.roughness;
                }
                else if (std::abs(constant_roughness - material.roughness) > 1.0e-4f) {
                    variable = true;
                }
            }
            return variable;
        }

        [[nodiscard]] std::vector<BakeTarget> plan_targets(
            const StaticScene& scene) {
            std::vector<BakeTarget> result;
            result.reserve(TARGET_MESH_NAMES.size());
            std::array<bool, TARGET_MESH_NAMES.size()> found{};

            for (std::uint32_t mesh_index = 0;
                mesh_index < scene.meshes.size();
                ++mesh_index) {
                const auto name = string_at(scene, scene.meshes[mesh_index].name);
                const auto expected = std::ranges::find(TARGET_MESH_NAMES, name);
                if (expected == TARGET_MESH_NAMES.end()) {
                    continue;
                }
                const auto target_index = static_cast<std::size_t>(
                    expected - TARGET_MESH_NAMES.begin());
                if (found[target_index]) {
                    log::Logger::g_logger << log::abrt(
                        "Impostor source mesh name is not unique.");
                }
                found[target_index] = true;

                const auto& mesh = scene.meshes[mesh_index];
                const float lod_error = scene.mesh_lods[
                    mesh.lod_offset + mesh.lod_count - 1].max_deviation;

                BakeTarget target;
                target.mesh = mesh_index;
                target.name = name;
                target.bounds = mesh_bounds(scene, mesh_index);
                target.radius = bounds_radius(target.bounds);
                target.bake_roughness = needs_roughness_bake(
                    scene,
                    mesh_index,
                    target.constant_roughness);

                const float camera_distance = target.radius + 0.001f;
                target.depth_min = camera_distance - target.radius;
                target.depth_range = 2.0f * target.radius;

                for (std::uint32_t direction = 0;
                    direction < DIRECTION_COUNT;
                    ++direction) {
                    const float angle = TWO_PI *
                        static_cast<float>(direction) /
                        static_cast<float>(DIRECTION_COUNT);
                    auto& output = target.directions[direction];
                    output.forward = { std::sin(angle), 0.0f, std::cos(angle) };
                    output.right = { std::cos(angle), 0.0f, -std::sin(angle) };
                    const float width = directional_extent(
                        target.bounds,
                        output.right);
                    const float height = target.bounds.maximum.y -
                        target.bounds.minimum.y;
                    output.width = aligned_dimension(
                        width * LOD_ENTRY_PIXELS / lod_error *
                        TEXELS_PER_ENTRY_PIXEL + 2.0f * BORDER_TEXELS);
                    output.height = aligned_dimension(
                        height * LOD_ENTRY_PIXELS / lod_error *
                        TEXELS_PER_ENTRY_PIXEL + 2.0f * BORDER_TEXELS);
                    output.card_width = width *
                        static_cast<float>(output.width) /
                        static_cast<float>(output.width - 2 * BORDER_TEXELS);
                    output.card_height = height *
                        static_cast<float>(output.height) /
                        static_cast<float>(output.height - 2 * BORDER_TEXELS);
                }
                result.push_back(target);
            }

            if (std::ranges::any_of(found, [](bool value) { return !value; })) {
                log::Logger::g_logger << log::abrt(
                    "A configured impostor source mesh was not found.");
            }
            return result;
        }

        [[nodiscard]] std::string lowercase_extension(
            const std::filesystem::path& path) {
            auto result = path.extension().string();
            std::ranges::transform(result, result.begin(), [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
            return result;
        }

        void load_texture(
            const std::filesystem::path& path,
            DirectX::TexMetadata& metadata,
            DirectX::ScratchImage& image) {
            const auto extension = lowercase_extension(path);
            HRESULT result = E_FAIL;
            if (extension == ".dds") {
                result = DirectX::LoadFromDDSFile(
                    path.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, image);
            }
            else if (extension == ".tga") {
                result = DirectX::LoadFromTGAFile(path.c_str(), &metadata, image);
            }
            else if (extension == ".hdr") {
                result = DirectX::LoadFromHDRFile(path.c_str(), &metadata, image);
            }
            else if (extension == ".exr") {
#if defined(FASTJUNGLE_HAS_OPENEXR)
                result = DirectX::LoadFromEXRFile(path.c_str(), &metadata, image);
#else
                log::Logger::g_logger << log::abrt(
                    "EXR source is unsupported by this cooker build.");
#endif
            }
            else {
                result = DirectX::LoadFromWICFile(
                    path.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, image);
            }
            if (FAILED(result)) {
                log::Logger::g_logger << log::abrt(
                    "Impostor source texture decode failed: " +
                    path.generic_string());
            }
        }

        [[nodiscard]] std::string_view texture_key(
            const StaticScene& scene,
            std::uint32_t texture) {
            const auto found = std::ranges::find_if(
                scene.texture_payload_refs,
                [texture](const auto& reference) {
                    return reference.texture == texture;
                });
            return string_at(scene, found->key);
        }

        void upload_source_texture(
            ID3D12Device* device,
            dx::ResourceUploader& uploader,
            const std::filesystem::path& path,
            TextureSource& destination) {
            DirectX::TexMetadata metadata{};
            DirectX::ScratchImage image;
            load_texture(path, metadata, image);

            D3D12_RESOURCE_DESC description{};
            description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            description.Width = metadata.width;
            description.Height = metadata.height;
            description.DepthOrArraySize = 1;
            description.MipLevels = static_cast<UINT16>(metadata.mipLevels);
            description.Format = metadata.format;
            description.SampleDesc.Count = 1;
            description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

            destination.texture.init(
                device,
                description,
                dx::TextureType::texture2d,
                D3D12_RESOURCE_STATE_COMMON);

            std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(
                metadata.mipLevels);
            std::vector<UINT> row_counts(metadata.mipLevels);
            std::vector<UINT64> row_sizes(metadata.mipLevels);
            UINT64 required_size = 0;
            device->GetCopyableFootprints(
                &description,
                0,
                static_cast<UINT>(metadata.mipLevels),
                0,
                footprints.data(),
                row_counts.data(),
                row_sizes.data(),
                &required_size);

            std::vector<dx::TextureSubresourceData> subresources(
                metadata.mipLevels);
            for (UINT mip = 0; mip < metadata.mipLevels; ++mip) {
                const auto* source = image.GetImage(mip, 0, 0);
                if (source == nullptr) {
                    log::Logger::g_logger << log::abrt(
                        "Impostor source texture mip is missing.");
                }
                subresources[mip] = {
                    reinterpret_cast<const std::byte*>(source->pixels),
                    source->rowPitch,
                    source->slicePitch,
                    footprints[mip],
                    row_counts[mip],
                    row_sizes[mip],
                };
            }
            uploader.upload_texture(
                destination.texture,
                dx::TextureUploadDesc{ subresources, required_size },
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            destination.format = metadata.format;
            destination.mip_count = static_cast<UINT>(metadata.mipLevels);
        }

        [[nodiscard]] D3D12_RESOURCE_DESC render_target_description(
            std::uint32_t width,
            std::uint32_t height,
            DXGI_FORMAT format) noexcept {
            D3D12_RESOURCE_DESC result{};
            result.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            result.Width = width;
            result.Height = height;
            result.DepthOrArraySize = 1;
            result.MipLevels = 1;
            result.Format = format;
            result.SampleDesc.Count = 1;
            result.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            result.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            return result;
        }

        [[nodiscard]] D3D12_RESOURCE_DESC depth_description(
            std::uint32_t width,
            std::uint32_t height) noexcept {
            auto result = render_target_description(
                width, height, DXGI_FORMAT_D32_FLOAT);
            result.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            return result;
        }

        [[nodiscard]] Readback make_readback(
            ID3D12Device* device,
            const D3D12_RESOURCE_DESC& description) {
            Readback result;
            UINT64 required_size = 0;
            device->GetCopyableFootprints(
                &description,
                0,
                1,
                0,
                &result.footprint,
                &result.row_count,
                &result.row_size,
                &required_size);
            result.buffer.init(
                device,
                required_size,
                D3D12_HEAP_TYPE_READBACK,
                D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATE_COPY_DEST);
            return result;
        }

        void copy_to_readback(
            ID3D12GraphicsCommandList* command_list,
            ID3D12Resource* source,
            const Readback& destination) {
            D3D12_TEXTURE_COPY_LOCATION source_location{};
            source_location.pResource = source;
            source_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            source_location.SubresourceIndex = 0;

            D3D12_TEXTURE_COPY_LOCATION destination_location{};
            destination_location.pResource = destination.buffer.get();
            destination_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            destination_location.PlacedFootprint = destination.footprint;
            command_list->CopyTextureRegion(
                &destination_location,
                0,
                0,
                0,
                &source_location,
                nullptr);
        }

        [[nodiscard]] DirectX::ScratchImage readback_image(
            const Readback& readback,
            DXGI_FORMAT format,
            std::uint32_t width,
            std::uint32_t height) {
            DirectX::ScratchImage result;
            if (FAILED(result.Initialize2D(format, width, height, 1, 1))) {
                log::Logger::g_logger << log::abrt(
                    "Failed to allocate impostor readback image.");
            }
            const auto* destination = result.GetImage(0, 0, 0);
            if (destination == nullptr) {
                log::Logger::g_logger << log::abrt(
                    "Impostor readback destination image is missing.");
            }
            MSVC_ASSUME(destination != nullptr);

            void* mapped = nullptr;
            const D3D12_RANGE read_range{
                0,
                static_cast<SIZE_T>(readback.buffer->GetDesc().Width),
            };
            dx::abort_failed(readback.buffer->Map(0, &read_range, &mapped));
            const auto* source = static_cast<const std::byte*>(mapped) +
                readback.footprint.Offset;
            for (std::uint32_t row = 0; row < height; ++row) {
                std::memcpy(
                    destination->pixels +
                        static_cast<std::size_t>(row) * destination->rowPitch,
                    source + static_cast<std::size_t>(row) *
                        readback.footprint.Footprint.RowPitch,
                    destination->rowPitch);
            }
            const D3D12_RANGE written_range{ 0, 0 };
            readback.buffer->Unmap(0, &written_range);
            return result;
        }

        [[nodiscard]] DirectX::ScratchImage encode_depth(
            const DirectX::ScratchImage& albedo,
            const DirectX::ScratchImage& linear_depth,
            float depth_min,
            float depth_range) {
            const auto& metadata = linear_depth.GetMetadata();
            const auto* color = albedo.GetImage(0, 0, 0);
            const auto* source = linear_depth.GetImage(0, 0, 0);
            if (color == nullptr || source == nullptr ||
                source->format != DXGI_FORMAT_R32_FLOAT ||
                color->format != DXGI_FORMAT_R8G8B8A8_UNORM) {
                log::Logger::g_logger << log::abrt(
                    "Impostor depth readback format is invalid.");
            }
            DirectX::ScratchImage result;
            if (FAILED(result.Initialize2D(
                DXGI_FORMAT_R16_UNORM,
                metadata.width,
                metadata.height,
                1,
                1))) {
                log::Logger::g_logger << log::abrt(
                    "Failed to allocate encoded impostor depth.");
            }
            const auto* destination = result.GetImage(0, 0, 0);
            if (destination == nullptr) {
                log::Logger::g_logger << log::abrt(
                    "Encoded impostor depth image is missing.");
            }

            for (std::size_t y = 0; y < metadata.height; ++y) {
                const auto* color_row = color->pixels + y * color->rowPitch;
                const auto* source_row = reinterpret_cast<const float*>(
                    source->pixels + y * source->rowPitch);
                auto* destination_row = reinterpret_cast<std::uint16_t*>(
                    destination->pixels + y * destination->rowPitch);
                for (std::size_t x = 0; x < metadata.width; ++x) {
                    const bool covered = color_row[x * 4 + 3] != 0;
                    const float encoded = covered
                        ? std::clamp(
                            (source_row[x] - depth_min) / depth_range,
                            0.0f,
                            1.0f)
                        : 0.0f;
                    destination_row[x] = static_cast<std::uint16_t>(
                        std::lround(encoded * 65535.0f));
                }
            }
            return result;
        }

        void split_albedo_opacity(
            const DirectX::ScratchImage& packed,
            DirectX::ScratchImage& albedo,
            DirectX::ScratchImage& opacity) {
            const auto* source = packed.GetImage(0, 0, 0);
            if (source == nullptr ||
                source->format != DXGI_FORMAT_R8G8B8A8_UNORM) {
                log::Logger::g_logger << log::abrt(
                    "Impostor albedo/opacity image format is invalid.");
            }
            if (FAILED(DirectX::TransformImage(
                *source,
                [](DirectX::XMVECTOR* output,
                    const DirectX::XMVECTOR* input,
                    std::size_t width,
                    std::size_t) {
                    for (std::size_t x = 0; x < width; ++x) {
                        output[x] = DirectX::XMVectorSetW(input[x], 1.0f);
                    }
                },
                albedo)) ||
                FAILED(DirectX::TransformImage(
                    *source,
                    [](DirectX::XMVECTOR* output,
                        const DirectX::XMVECTOR* input,
                        std::size_t width,
                        std::size_t) {
                        for (std::size_t x = 0; x < width; ++x) {
                            output[x] = DirectX::XMVectorSplatW(input[x]);
                        }
                    },
                    opacity))) {
                log::Logger::g_logger << log::abrt(
                    "Failed to split impostor albedo and opacity.");
            }
        }

        void dilate_impostor_attributes(
            DirectX::ScratchImage& albedo_alpha,
            DirectX::ScratchImage& normal,
            DirectX::ScratchImage* roughness) {
            auto* color = albedo_alpha.GetImage(0, 0, 0);
            auto* normal_image = normal.GetImage(0, 0, 0);
            auto* roughness_image = roughness == nullptr
                ? nullptr
                : roughness->GetImage(0, 0, 0);
            const std::size_t width = color->width;
            const std::size_t height = color->height;
            const std::size_t pixel_count = width * height;
            std::vector<std::int32_t> nearest(pixel_count, -1);
            std::deque<std::size_t> open;

            for (std::size_t y = 0; y < height; ++y) {
                const auto* row = color->pixels + y * color->rowPitch;
                for (std::size_t x = 0; x < width; ++x) {
                    const auto index = y * width + x;
                    if (row[x * 4 + 3] != 0) {
                        nearest[index] = static_cast<std::int32_t>(index);
                        open.push_back(index);
                    }
                }
            }

            const auto visit = [&nearest, &open](
                std::size_t from,
                std::size_t to) {
                if (nearest[to] == -1) {
                    nearest[to] = nearest[from];
                    open.push_back(to);
                }
            };
            while (!open.empty()) {
                const auto index = open.front();
                open.pop_front();
                const auto x = index % width;
                const auto y = index / width;
                if (x != 0) visit(index, index - 1);
                if (x + 1 < width) visit(index, index + 1);
                if (y != 0) visit(index, index - width);
                if (y + 1 < height) visit(index, index + width);
            }

            for (std::size_t y = 0; y < height; ++y) {
                auto* color_row = color->pixels + y * color->rowPitch;
                auto* normal_row =
                    normal_image->pixels + y * normal_image->rowPitch;
                auto* roughness_row = roughness_image == nullptr
                    ? nullptr
                    : roughness_image->pixels +
                        y * roughness_image->rowPitch;
                for (std::size_t x = 0; x < width; ++x) {
                    if (color_row[x * 4 + 3] != 0) {
                        continue;
                    }
                    const auto source_index = static_cast<std::size_t>(
                        nearest[y * width + x]);
                    const auto source_x = source_index % width;
                    const auto source_y = source_index / width;
                    const auto* source_color =
                        color->pixels + source_y * color->rowPitch +
                        source_x * 4;
                    const auto* source_normal =
                        normal_image->pixels +
                        source_y * normal_image->rowPitch +
                        source_x * 4;
                    std::copy_n(source_color, 3, color_row + x * 4);
                    std::copy_n(source_normal, 4, normal_row + x * 4);
                    if (roughness_row != nullptr) {
                        roughness_row[x] = roughness_image->pixels[
                            source_y * roughness_image->rowPitch + source_x];
                    }
                }
            }
        }

        [[nodiscard]] std::vector<BakedTarget> bake_targets(
            const StaticScene& scene,
            std::span<const BakeTarget> targets) {
            const ComScope com_scope;
            auto factory = dx::DeviceUtils::create_factory();
            auto device = dx::DeviceUtils::create_device(factory.Get());

            dx::CommandQueue queue;
            queue.init(device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
            dx::ResourceUploader uploader;
            uploader.init(
                device.Get(),
                queue,
                256ull * 1024ull * 1024ull,
                2);

            std::vector<bool> source_texture_used(scene.textures.size(), false);
            std::vector<bool> source_texture_srgb(scene.textures.size(), false);
            for (const auto& target : targets) {
                const auto& mesh = scene.meshes[target.mesh];
                const auto& lod = scene.mesh_lods[mesh.lod_offset];
                for (std::uint32_t local_submesh = 0;
                    local_submesh < lod.submesh_count;
                    ++local_submesh) {
                    const auto& submesh = scene.submeshes[
                        lod.submesh_offset + local_submesh];
                    const auto& material = scene.materials[submesh.material];
                    const auto mark = [&scene, &source_texture_used](
                        std::uint32_t binding) {
                        const auto texture = texture_from_binding(scene, binding);
                        if (texture != StaticScene::INVALID_INDEX) {
                            source_texture_used[texture] = true;
                        }
                    };
                    mark(material.texture_binding_base_color);
                    mark(material.texture_binding_opacity);
                    mark(material.texture_binding_normal);
                    mark(material.texture_binding_roughness);
                    if (material.texture_binding_base_color != StaticScene::INVALID_INDEX) {
                        const auto& binding = scene.texture_bindings[
                            material.texture_binding_base_color];
                        source_texture_srgb[binding.texture] =
                            binding.flags ==
                                StaticScene::EnumTextureBindingFlag::SRGB;
                    }
                }
            }

            std::vector<std::optional<TextureSource>> source_textures(
                scene.textures.size());
            for (std::uint32_t texture = 0;
                texture < scene.textures.size();
                ++texture) {
                if (!source_texture_used[texture]) {
                    continue;
                }
                source_textures[texture].emplace();
                upload_source_texture(
                    device.Get(),
                    uploader,
                    std::filesystem::path{texture_key(scene, texture)},
                    *source_textures[texture]);
            }
            uploader.flush();

            dx::Buffer vertex_buffer;
            dx::Buffer index_buffer;
            vertex_buffer.init(
                device.Get(),
                scene.vertices.size() * sizeof(StaticScene::Vertex),
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATE_COMMON);
            index_buffer.init(
                device.Get(),
                scene.indices.size() * sizeof(std::uint32_t),
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATE_COMMON);
            uploader.upload_buffer(
                vertex_buffer,
                std::as_bytes(std::span{scene.vertices}),
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            uploader.upload_buffer(
                index_buffer,
                std::as_bytes(std::span{scene.indices}),
                D3D12_RESOURCE_STATE_INDEX_BUFFER);
            uploader.flush();

            const UINT texture_descriptor_count = (std::max)(
                1u, checked_u32(scene.textures.size(), "Source texture count"));
            dx::DescriptorHeap source_srvs;
            source_srvs.init(
                device.Get(),
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                texture_descriptor_count,
                true);
            const auto source_srv_allocation = source_srvs.alloc(texture_descriptor_count);
            for (std::uint32_t texture = 0;
                texture < texture_descriptor_count;
                ++texture) {
                if (texture < source_textures.size() && source_textures[texture]) {
                    const auto& source = *source_textures[texture];
                    const DXGI_FORMAT view_format = source_texture_srgb[texture]
                        ? dx::FormatUtils::to_srgb(source.format)
                        : dx::FormatUtils::to_linear(source.format);
                    source.texture.create_srv(
                        device.Get(),
                        source_srv_allocation.get_cpu(texture),
                        { 0, source.mip_count, 0, 1 },
                        view_format);
                }
                else {
                    D3D12_SHADER_RESOURCE_VIEW_DESC null_view{};
                    null_view.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    null_view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    null_view.Shader4ComponentMapping =
                        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    null_view.Texture2D.MipLevels = 1;
                    device->CreateShaderResourceView(
                        nullptr,
                        &null_view,
                        source_srv_allocation.get_cpu(texture));
                }
            }

            dx::DescriptorHeap sampler_heap;
            sampler_heap.init(
                device.Get(),
                D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                1,
                true);
            const auto sampler = sampler_heap.alloc();
            D3D12_SAMPLER_DESC sampler_desc{};
            sampler_desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            sampler_desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            sampler_desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            sampler_desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            sampler_desc.MaxLOD = D3D12_FLOAT32_MAX;
            device->CreateSampler(&sampler_desc, sampler.get_cpu());

            dx::RootSignatureBuilder root_builder;
            root_builder.init(RootParameter::COUNT);
            root_builder.set_flags(
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);
            root_builder.set_constants(RootParameter::CAMERA)
                .reg(0)
                .count(sizeof(BakeCameraConstants) / sizeof(std::uint32_t))
                .vis_vertex()
                .add();
            root_builder.set_constants(RootParameter::MATERIAL)
                .reg(1)
                .count(sizeof(BakeMaterialConstants) / sizeof(std::uint32_t))
                .vis_pixel()
                .add();
            root_builder.set_resource_table(RootParameter::TEXTURES)
                .srv().reg(0).count(texture_descriptor_count)
                .flags(D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC)
                .add_range().vis_pixel().add();
            root_builder.set_sampler_table(RootParameter::SAMPLER)
                .sampler().reg(0).count(1)
                .add_range().vis_pixel().add();
            const auto root_signature = root_builder.build(device.Get());

            const std::filesystem::path shader_dir{
                FASTJUNGLE_COOKER_SHADER_OUTPUT_DIR};
            dx::Shader vertex_shader;
            dx::Shader pixel_shader;
            vertex_shader.load(shader_dir / "ImpostorBake.vs.dxil");
            pixel_shader.load(shader_dir / "ImpostorBake.ps.dxil");

            const std::array<D3D12_INPUT_ELEMENT_DESC, 3> input_layout{
                D3D12_INPUT_ELEMENT_DESC{
                    .SemanticName = "POSITION", .SemanticIndex = 0,
                    .Format = DXGI_FORMAT_R32G32B32_FLOAT, .InputSlot = 0,
                    .AlignedByteOffset = 0,
                    .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                },
                D3D12_INPUT_ELEMENT_DESC{
                    .SemanticName = "NORMAL", .SemanticIndex = 0,
                    .Format = DXGI_FORMAT_R32G32B32_FLOAT, .InputSlot = 0,
                    .AlignedByteOffset = 12,
                    .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                },
                D3D12_INPUT_ELEMENT_DESC{
                    .SemanticName = "TEXCOORD", .SemanticIndex = 0,
                    .Format = DXGI_FORMAT_R32G32_FLOAT, .InputSlot = 0,
                    .AlignedByteOffset = 24,
                    .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                },
            };
            auto pipeline_desc = dx::PSOUtils::default_graphics_desc();
            pipeline_desc.pRootSignature = root_signature.Get();
            pipeline_desc.VS = vertex_shader.get_bytecode();
            pipeline_desc.PS = pixel_shader.get_bytecode();
            pipeline_desc.InputLayout = {
                input_layout.data(), static_cast<UINT>(input_layout.size()) };
            pipeline_desc.NumRenderTargets = 4;
            pipeline_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            pipeline_desc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
            pipeline_desc.RTVFormats[2] = DXGI_FORMAT_R32_FLOAT;
            pipeline_desc.RTVFormats[3] = DXGI_FORMAT_R8_UNORM;
            pipeline_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
            pipeline_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
            const auto pipeline = dx::PSOUtils::create_graphics(
                device.Get(), pipeline_desc);

            dx::DescriptorHeap rtv_heap;
            rtv_heap.init(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 4, false);
            dx::DescriptorHeap dsv_heap;
            dsv_heap.init(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
            dx::CommandContext context;
            context.init(device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, 0);

            const D3D12_VERTEX_BUFFER_VIEW vertex_view{
                .BufferLocation = vertex_buffer->GetGPUVirtualAddress(),
                .SizeInBytes = checked_u32(
                    scene.vertices.size() * sizeof(StaticScene::Vertex),
                    "Impostor vertex byte count"),
                .StrideInBytes = sizeof(StaticScene::Vertex),
            };
            const D3D12_INDEX_BUFFER_VIEW index_view{
                .BufferLocation = index_buffer->GetGPUVirtualAddress(),
                .SizeInBytes = checked_u32(
                    scene.indices.size() * sizeof(std::uint32_t),
                    "Impostor index byte count"),
                .Format = DXGI_FORMAT_R32_UINT,
            };

            std::vector<BakedTarget> result(targets.size());
            for (std::size_t target_index = 0;
                target_index < targets.size();
                ++target_index) {
                const auto& target = targets[target_index];
                const auto& mesh = scene.meshes[target.mesh];
                const auto& lod = scene.mesh_lods[mesh.lod_offset];
                const auto center = target.bounds.center();

                for (std::uint32_t direction_index = 0;
                    direction_index < DIRECTION_COUNT;
                    ++direction_index) {
                    const auto& direction = target.directions[direction_index];
                    const auto color_desc = render_target_description(
                        direction.width, direction.height,
                        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
                    const auto normal_desc = render_target_description(
                        direction.width, direction.height,
                        DXGI_FORMAT_R8G8B8A8_UNORM);
                    const auto depth_data_desc = render_target_description(
                        direction.width, direction.height,
                        DXGI_FORMAT_R32_FLOAT);
                    const auto roughness_desc = render_target_description(
                        direction.width, direction.height,
                        DXGI_FORMAT_R8_UNORM);
                    const auto depth_stencil_desc = depth_description(
                        direction.width, direction.height);

                    const D3D12_CLEAR_VALUE color_clear{
                        .Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                        .Color = { 0.0f, 0.0f, 0.0f, 0.0f },
                    };
                    const D3D12_CLEAR_VALUE normal_clear{
                        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                        .Color = { 0.5f, 0.5f, 1.0f, 0.0f },
                    };
                    const D3D12_CLEAR_VALUE depth_data_clear{
                        .Format = DXGI_FORMAT_R32_FLOAT,
                        .Color = { 0.0f, 0.0f, 0.0f, 0.0f },
                    };
                    const D3D12_CLEAR_VALUE roughness_clear{
                        .Format = DXGI_FORMAT_R8_UNORM,
                        .Color = { target.constant_roughness, 0.0f, 0.0f, 0.0f },
                    };
                    const D3D12_CLEAR_VALUE depth_stencil_clear{
                        .Format = DXGI_FORMAT_D32_FLOAT,
                        .DepthStencil = { 1.0f, 0 },
                    };
                    dx::Texture color;
                    dx::Texture normal;
                    dx::Texture linear_depth;
                    dx::Texture roughness;
                    dx::Texture depth_stencil;
                    color.init(device.Get(), color_desc, dx::TextureType::texture2d,
                        D3D12_RESOURCE_STATE_RENDER_TARGET, &color_clear);
                    normal.init(device.Get(), normal_desc, dx::TextureType::texture2d,
                        D3D12_RESOURCE_STATE_RENDER_TARGET, &normal_clear);
                    linear_depth.init(device.Get(), depth_data_desc, dx::TextureType::texture2d,
                        D3D12_RESOURCE_STATE_RENDER_TARGET, &depth_data_clear);
                    roughness.init(device.Get(), roughness_desc, dx::TextureType::texture2d,
                        D3D12_RESOURCE_STATE_RENDER_TARGET, &roughness_clear);
                    depth_stencil.init(device.Get(), depth_stencil_desc,
                        dx::TextureType::texture2d,
                        D3D12_RESOURCE_STATE_DEPTH_WRITE, &depth_stencil_clear);

                    // DescriptorHeap::reset destroys the D3D12 heap.  Each
                    // direction is fully flushed before the next one, so a
                    // fresh four-RTV/one-DSV heap is the simple safe reuse.
                    rtv_heap.init(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 4, false);
                    dsv_heap.init(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
                    const auto rtvs = rtv_heap.alloc(4);
                    const auto dsv = dsv_heap.alloc();
                    color.create_rtv(device.Get(), rtvs.get_cpu(0), 0, 0, 1,
                        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
                    normal.create_rtv(device.Get(), rtvs.get_cpu(1), 0, 0, 1,
                        DXGI_FORMAT_R8G8B8A8_UNORM);
                    linear_depth.create_rtv(device.Get(), rtvs.get_cpu(2), 0, 0, 1,
                        DXGI_FORMAT_R32_FLOAT);
                    roughness.create_rtv(device.Get(), rtvs.get_cpu(3), 0, 0, 1,
                        DXGI_FORMAT_R8_UNORM);
                    depth_stencil.create_dsv(device.Get(), dsv.get_cpu(), 0, 0, 1,
                        DXGI_FORMAT_D32_FLOAT, D3D12_DSV_FLAG_NONE);

                    auto color_readback = make_readback(device.Get(), color_desc);
                    auto normal_readback = make_readback(device.Get(), normal_desc);
                    auto depth_readback = make_readback(device.Get(), depth_data_desc);
                    auto roughness_readback = make_readback(device.Get(), roughness_desc);

                    const DirectX::XMVECTOR target_position = DirectX::XMLoadFloat3(&center);
                    const DirectX::XMVECTOR forward = DirectX::XMLoadFloat3(
                        &direction.forward);
                    const DirectX::XMVECTOR eye = DirectX::XMVectorSubtract(
                        target_position,
                        DirectX::XMVectorScale(
                            forward,
                            target.radius + 0.001f));
                    const auto view = DirectX::XMMatrixLookAtLH(
                        eye,
                        target_position,
                        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
                    const auto projection = DirectX::XMMatrixOrthographicOffCenterLH(
                        -direction.card_width * 0.5f,
                        direction.card_width * 0.5f,
                        -direction.card_height * 0.5f,
                        direction.card_height * 0.5f,
                        0.0f,
                        target.depth_min + target.depth_range + 0.001f);
                    BakeCameraConstants camera;
                    DirectX::XMStoreFloat4x4(&camera.object_to_view, view);
                    DirectX::XMStoreFloat4x4(&camera.object_to_clip, view * projection);

                    context.reset(pipeline.Get());
                    const std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 4> rtv_handles{
                        rtvs.get_cpu(0), rtvs.get_cpu(1), rtvs.get_cpu(2), rtvs.get_cpu(3) };
                    const auto dsv_handle = dsv.get_cpu();
                    context->OMSetRenderTargets(
                        static_cast<UINT>(rtv_handles.size()), rtv_handles.data(), FALSE,
                        &dsv_handle);
                    const std::array<float, 4> clear_color{ 0, 0, 0, 0 };
                    context->ClearRenderTargetView(rtvs.get_cpu(0), clear_color.data(), 0, nullptr);
                    const std::array<float, 4> clear_normal{ 0.5f, 0.5f, 1.0f, 0.0f };
                    context->ClearRenderTargetView(rtvs.get_cpu(1), clear_normal.data(), 0, nullptr);
                    context->ClearRenderTargetView(rtvs.get_cpu(2), clear_color.data(), 0, nullptr);
                    const std::array<float, 4> clear_roughness{
                        target.constant_roughness, 0, 0, 0 };
                    context->ClearRenderTargetView(rtvs.get_cpu(3), clear_roughness.data(), 0, nullptr);
                    context->ClearDepthStencilView(dsv.get_cpu(), D3D12_CLEAR_FLAG_DEPTH,
                        1.0f, 0, 0, nullptr);
                    context.RSSetViewPortScissorRect(direction.width, direction.height);
                    context.SetDescriptorHeaps(source_srvs.get(), sampler_heap.get());
                    context->SetGraphicsRootSignature(root_signature.Get());
                    context->SetGraphicsRoot32BitConstants(
                        static_cast<UINT>(RootParameter::CAMERA),
                        sizeof(camera) / sizeof(std::uint32_t), &camera, 0);
                    context->SetGraphicsRootDescriptorTable(
                        static_cast<UINT>(RootParameter::TEXTURES),
                        source_srv_allocation.get_gpu());
                    context->SetGraphicsRootDescriptorTable(
                        static_cast<UINT>(RootParameter::SAMPLER), sampler.get_gpu());
                    context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                    context->IASetVertexBuffers(0, 1, &vertex_view);
                    context->IASetIndexBuffer(&index_view);

                    for (std::uint32_t local_submesh = 0;
                        local_submesh < lod.submesh_count;
                        ++local_submesh) {
                        const auto& submesh = scene.submeshes[
                            lod.submesh_offset + local_submesh];
                        const auto constants = material_constants(
                            scene, scene.materials[submesh.material]);
                        context->SetGraphicsRoot32BitConstants(
                            static_cast<UINT>(RootParameter::MATERIAL),
                            sizeof(constants) / sizeof(std::uint32_t),
                            &constants,
                            0);
                        context->DrawIndexedInstanced(
                            submesh.index_count,
                            1,
                            submesh.index_offset,
                            static_cast<INT>(submesh.vertex_offset),
                            0);
                    }

                    color.transition(context.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
                    normal.transition(context.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
                    linear_depth.transition(context.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
                    roughness.transition(context.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
                    copy_to_readback(context.get(), color.get(), color_readback);
                    copy_to_readback(context.get(), normal.get(), normal_readback);
                    copy_to_readback(context.get(), linear_depth.get(), depth_readback);
                    copy_to_readback(context.get(), roughness.get(), roughness_readback);
                    context.close();
                    queue.execute(context.get());
                    queue.flush();

                    auto& baked = result[target_index].directions[direction_index];
                    auto albedo_alpha = readback_image(
                        color_readback,
                        DXGI_FORMAT_R8G8B8A8_UNORM,
                        direction.width,
                        direction.height);
                    baked.normal = readback_image(
                        normal_readback,
                        DXGI_FORMAT_R8G8B8A8_UNORM,
                        direction.width,
                        direction.height);
                    if (target.bake_roughness) {
                        baked.roughness = readback_image(
                            roughness_readback,
                            DXGI_FORMAT_R8_UNORM,
                            direction.width,
                            direction.height);
                    }
                    dilate_impostor_attributes(
                        albedo_alpha,
                        baked.normal,
                        target.bake_roughness
                            ? &baked.roughness
                            : nullptr);
                    const auto raw_depth = readback_image(
                        depth_readback,
                        DXGI_FORMAT_R32_FLOAT,
                        direction.width,
                        direction.height);
                    baked.depth = encode_depth(
                        albedo_alpha,
                        raw_depth,
                        target.depth_min,
                        target.depth_range);
                    split_albedo_opacity(
                        albedo_alpha,
                        baked.albedo,
                        baked.opacity);
                }
            }
            return result;
        }

        [[nodiscard]] std::string texture_key_for(
            std::string_view mesh_name,
            std::uint32_t direction,
            std::string_view kind) {
            return "generated://impostor/" + std::string{mesh_name} + "/" +
                std::to_string(direction) + "/" + std::string{kind};
        }

        void append_card_mesh(
            StaticScene& scene,
            const BakeTarget& target,
            const BakeDirection& direction,
            std::uint32_t material,
            std::uint32_t direction_index) {
            const auto center = target.bounds.center();
            const float half_width = direction.card_width * 0.5f;
            const float half_height = direction.card_height * 0.5f;
            const auto point = [&center, &direction](float x, float y) {
                return DirectX::XMFLOAT3{
                    center.x + direction.right.x * x,
                    center.y + y,
                    center.z + direction.right.z * x,
                };
            };
            const auto vertex_offset = checked_u32(
                scene.vertices.size(), "Impostor card vertex offset");
            const auto index_offset = checked_u32(
                scene.indices.size(), "Impostor card index offset");
            const auto normal = DirectX::XMFLOAT3{
                -direction.forward.x, 0.0f, -direction.forward.z };
            scene.vertices.insert(scene.vertices.end(), {
                { point(-half_width, half_height), normal, { 0.0f, 0.0f } },
                { point( half_width, half_height), normal, { 1.0f, 0.0f } },
                { point( half_width,-half_height), normal, { 1.0f, 1.0f } },
                { point(-half_width,-half_height), normal, { 0.0f, 1.0f } },
            });
            scene.indices.insert(scene.indices.end(), { 0, 2, 1, 0, 3, 2 });

            const auto submesh = checked_u32(
                scene.submeshes.size(), "Impostor card submesh index");
            const auto name = std::string{target.name} + "_Impostor_" +
                std::to_string(direction_index);
            scene.submeshes.push_back({
                .name = append_string(scene, name),
                .vertex_offset = vertex_offset,
                .vertex_count = 4,
                .index_offset = index_offset,
                .index_count = 6,
                .material = material,
                .flags = StaticScene::EnumSubmeshFlag::DOUBLE_SIDED_AND_ALPHA_TESTED,
            });
            const auto lod = checked_u32(
                scene.mesh_lods.size(), "Impostor card LOD index");
            scene.mesh_lods.push_back({
                .submesh_offset = submesh,
                .submesh_count = 1,
                .max_deviation = 0.0f,
            });
            scene.meshes.push_back({
                .name = append_string(scene, name),
                .lod_offset = lod,
                .lod_count = 1,
            });
        }

        void append_scene_assets(
            StaticScene& scene,
            std::span<const BakeTarget> targets,
            std::vector<BakedTarget>* baked,
            ImpostorCookResult& result) {
            const auto sampler = ensure_sampler(scene);
            for (std::size_t target_index = 0;
                target_index < targets.size();
                ++target_index) {
                const auto& target = targets[target_index];
                const auto color_start = checked_u32(scene.textures.size(), "Impostor albedo texture offset");
                for (uint32_t direction = 0; direction < DIRECTION_COUNT; ++direction) {
                    const auto key = texture_key_for(target.name, direction, "albedo");
                    append_texture(scene, std::string{target.name} + "_ImpostorAlbedo", key);
                    if (baked != nullptr) {
                        result.generated_textures.push_back({
                            .key = key,
                            .image = std::move((*baked)[target_index]
                                .directions[direction].albedo),
                        });
                    }
                }
                const auto opacity_start = checked_u32(
                    scene.textures.size(), "Impostor opacity texture offset");
                for (std::uint32_t direction = 0;
                    direction < DIRECTION_COUNT;
                    ++direction) {
                    const auto key = texture_key_for(
                        target.name,
                        direction,
                        "opacity");
                    append_texture(
                        scene,
                        std::string{target.name} + "_ImpostorOpacity",
                        key);
                    if (baked != nullptr) {
                        result.generated_textures.push_back({
                            .key = key,
                            .image = std::move((*baked)[target_index]
                                .directions[direction].opacity),
                        });
                    }
                }
                const auto normal_start = checked_u32(
                    scene.textures.size(), "Impostor normal texture offset");
                for (std::uint32_t direction = 0;
                    direction < DIRECTION_COUNT;
                    ++direction) {
                    const auto key = texture_key_for(target.name, direction, "normal");
                    append_texture(scene, std::string{target.name} + "_ImpostorNormal", key);
                    if (baked != nullptr) {
                        result.generated_textures.push_back({
                            .key = key,
                            .image = std::move((*baked)[target_index]
                                .directions[direction].normal),
                        });
                    }
                }
                const auto depth_start = checked_u32(
                    scene.textures.size(), "Impostor depth texture offset");
                for (std::uint32_t direction = 0;
                    direction < DIRECTION_COUNT;
                    ++direction) {
                    const auto key = texture_key_for(target.name, direction, "depth");
                    append_texture(scene, std::string{target.name} + "_ImpostorDepth", key);
                    if (baked != nullptr) {
                        result.generated_textures.push_back({
                            .key = key,
                            .image = std::move((*baked)[target_index]
                                .directions[direction].depth),
                            .uncompressed_output_format = DXGI_FORMAT_R16_UNORM,
                        });
                    }
                }

                std::uint32_t roughness_start = StaticScene::INVALID_INDEX;
                if (target.bake_roughness) {
                    roughness_start = checked_u32(
                        scene.textures.size(), "Impostor roughness texture offset");
                    for (std::uint32_t direction = 0;
                        direction < DIRECTION_COUNT;
                        ++direction) {
                        const auto key = texture_key_for(target.name, direction, "roughness");
                        append_texture(scene, std::string{target.name} + "_ImpostorRoughness", key);
                        if (baked != nullptr) {
                            result.generated_textures.push_back({
                                .key = key,
                                .image = std::move((*baked)[target_index]
                                    .directions[direction].roughness),
                            });
                        }
                    }
                }

                const auto card_mesh_offset = checked_u32(
                    scene.meshes.size(), "Impostor card mesh offset");
                for (std::uint32_t direction = 0;
                    direction < DIRECTION_COUNT;
                    ++direction) {
                    const auto base_binding = checked_u32(
                        scene.texture_bindings.size(), "Impostor albedo binding");
                    scene.texture_bindings.push_back({
                        .texture = color_start + direction,
                        .sampler = sampler,
                        .channel = StaticScene::EnumTextureChannel::RGB,
                        .flags = StaticScene::EnumTextureBindingFlag::SRGB,
                    });
                    const auto opacity_binding = checked_u32(
                        scene.texture_bindings.size(),
                        "Impostor opacity binding");
                    scene.texture_bindings.push_back({
                        .texture = opacity_start + direction,
                        .sampler = sampler,
                        .channel = StaticScene::EnumTextureChannel::R,
                        .flags = StaticScene::EnumTextureBindingFlag::LINEAR,
                    });
                    const auto normal_binding = checked_u32(
                        scene.texture_bindings.size(), "Impostor normal binding");
                    scene.texture_bindings.push_back({
                        .texture = normal_start + direction,
                        .sampler = sampler,
                        .channel = StaticScene::EnumTextureChannel::RGB,
                        .flags = StaticScene::EnumTextureBindingFlag::LINEAR,
                    });
                    std::uint32_t roughness_binding = StaticScene::INVALID_INDEX;
                    if (roughness_start != StaticScene::INVALID_INDEX) {
                        roughness_binding = checked_u32(
                            scene.texture_bindings.size(),
                            "Impostor roughness binding");
                        scene.texture_bindings.push_back({
                            .texture = roughness_start + direction,
                            .sampler = sampler,
                            .channel = StaticScene::EnumTextureChannel::R,
                            .flags = StaticScene::EnumTextureBindingFlag::LINEAR,
                        });
                    }
                    const auto material = checked_u32(
                        scene.materials.size(), "Impostor material index");
                    scene.materials.push_back({
                        .name = append_string(scene,
                            std::string{target.name} + "_ImpostorMaterial"),
                        .base_color = { 1.0f, 1.0f, 1.0f, 1.0f },
                        .roughness = target.constant_roughness,
                        .opacity = 1.0f,
                        .opacity_threshold = 0.5f,
                        .texture_binding_base_color = base_binding,
                        .texture_binding_normal = normal_binding,
                        .texture_binding_roughness = roughness_binding,
                        .texture_binding_opacity = opacity_binding,
                    });
                    append_card_mesh(
                        scene,
                        target,
                        target.directions[direction],
                        material,
                        direction);
                }

                scene.impostors.push_back({
                    .mesh = target.mesh,
                    .card_mesh_offset = card_mesh_offset,
                    .depth_texture_offset = depth_start,
                    .direction_count = DIRECTION_COUNT,
                    .depth_min_range = { target.depth_min, target.depth_range },
                });
                ++result.impostor_count;
            }
        }
    } // namespace

    ImpostorCookResult ImpostorCooker::cook(
        scene::StaticScene& scene,
        bool bake_images) {
        const auto targets = plan_targets(scene);
        ImpostorCookResult result;
        if (bake_images) {
            auto baked = bake_targets(scene, targets);
            append_scene_assets(scene, targets, &baked, result);
        }
        else {
            append_scene_assets(scene, targets, nullptr, result);
        }
        return result;
    }

} // namespace fjr::cooker
