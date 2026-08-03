#include "FastJungle/cooker/StaticSceneBuilder.hpp"

#include "FastJungle/core/util/Logger.hpp"

#include <DirectXTex.h>

#if defined(FASTJUNGLE_HAS_OPENEXR)
#include <DirectXTexEXR.h>
#endif

#include <objbase.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <limits>
#include <queue>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace fjr::cooker {

    namespace {

        using SourceScene = JungleScene;
        using StaticScene = scene::StaticScene;

        constexpr std::uint32_t TEXTURE_BINDING_SRGB = 1u << 0u;


        constexpr std::uint32_t SUBMESH_DOUBLE_SIDED = 1u << 0u;
        constexpr std::uint32_t SUBMESH_ALPHA_TESTED = 1u << 1u;

        constexpr std::uint32_t SAMPLER_FILTER_LINEAR = 0u;
        constexpr std::uint32_t SAMPLER_ADDRESS_WRAP = 0u;
        constexpr std::uint32_t SAMPLER_ADDRESS_CLAMP = 1u;
        constexpr std::uint32_t SAMPLER_ADDRESS_MIRROR = 2u;
        constexpr std::uint32_t SAMPLER_ADDRESS_BORDER = 3u;

        constexpr float VECTOR_EPSILON = 1.0e-10f;
        constexpr float MATRIX_DETERMINANT_EPSILON = 1.0e-12f;

        template<typename... Parts>
        [[noreturn]] void fail(Parts&&... parts) {
            auto& logger = log::Logger::g_logger;
            logger << "[StaticSceneBuilder] ";
            (logger << ... << std::forward<Parts>(parts));
            logger << '\n';
            logger.abort();
        }

        [[noreturn]] void fail_hresult(
            std::string_view operation,
            HRESULT result,
            const std::filesystem::path& path = {}) {

            auto& logger = log::Logger::g_logger;
            logger << "[StaticSceneBuilder] " << operation;
            if (!path.empty()) {
                logger << ": " << path.generic_string();
            }
            logger << " (HRESULT="
                << static_cast<std::uint32_t>(result)
                << ")\n";
            logger.abort();
        }

        [[nodiscard]] std::uint32_t checked_u32(
            std::size_t value,
            std::string_view subject) {

            if (value > std::numeric_limits<std::uint32_t>::max()) {
                fail(subject, " exceeds the 32-bit StaticScene limit.");
            }
            return static_cast<std::uint32_t>(value);
        }

        [[nodiscard]] std::string path_leaf(std::string_view path) {
            const auto separator = path.find_last_of('/');
            return separator == std::string_view::npos
                ? std::string{ path }
            : std::string{ path.substr(separator + 1) };
        }

        [[nodiscard]] std::string lowercase_extension(
            const std::filesystem::path& path) {

            auto result = path.extension().string();
            std::ranges::transform(
                result,
                result.begin(),
                [](unsigned char value) {
                    return static_cast<char>(std::tolower(value));
                });
            return result;
        }

        [[nodiscard]] std::string normalized_texture_key(
            const std::filesystem::path& path) {

            std::error_code error;
            auto absolute = std::filesystem::absolute(path, error);
            if (error) {
                absolute = path;
            }

            auto result = absolute.lexically_normal().generic_string();
            std::ranges::transform(
                result,
                result.begin(),
                [](unsigned char value) {
                    return static_cast<char>(std::tolower(value));
                });
            return result;
        }

        class ComScope {
        public:
            ComScope() {
                const HRESULT result = CoInitializeEx(
                    nullptr,
                    COINIT_MULTITHREADED);
                if (SUCCEEDED(result)) {
                    uninitialize_ = true;
                } else if (result != RPC_E_CHANGED_MODE) {
                    fail_hresult("CoInitializeEx failed", result);
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

        struct SamplerKey {
            std::uint32_t filter = SAMPLER_FILTER_LINEAR;
            std::uint32_t address_u = SAMPLER_ADDRESS_WRAP;
            std::uint32_t address_v = SAMPLER_ADDRESS_WRAP;
            std::uint32_t address_w = SAMPLER_ADDRESS_WRAP;
            std::uint32_t max_anisotropy = 1;

            bool operator==(const SamplerKey&) const = default;
        };

        struct SamplerKeyHash {
            [[nodiscard]] std::size_t operator()(
                const SamplerKey& value) const noexcept {

                std::size_t result = value.filter;
                const auto combine = [&result](std::uint32_t part) {
                    result ^= static_cast<std::size_t>(part) +
                        0x9e3779b9u + (result << 6u) + (result >> 2u);
                    };
                combine(value.address_u);
                combine(value.address_v);
                combine(value.address_w);
                combine(value.max_anisotropy);
                return result;
            }
        };

        struct ResolvedTextureBinding {
            std::filesystem::path path;
            std::string source_output;
            std::string uv_primvar;
            bool srgb = false;
            SamplerKey sampler{};
        };

        struct MaterialRecord {
            std::uint32_t index = StaticScene::INVALID_INDEX;
            std::string uv_primvar;
            bool has_textures = false;
            bool alpha_tested = false;
        };

        struct FaceGroup {
            std::string material_path;
            std::vector<std::uint32_t> faces;
        };

        class Builder {
        public:
            explicit Builder(const SourceScene& source)
                : source_(source),
                result_(std::make_unique<StaticScene>()),
                world_cache_(source.nodes.size()),
                world_states_(source.nodes.size(), 0),
                selected_nodes_(source.nodes.size(), false),
                selected_parent_(
                    source.nodes.size(),
                    SourceScene::INVALID_INDEX),
                selected_children_(source.nodes.size()) {

                result_->names.push_back('\0');
                name_offsets_.emplace(std::string{}, 0u);

                material_sources_.reserve(source.materials.size());
                for (const auto& material : source.materials) {
                    if (!material_sources_.emplace(
                        material.prim_path,
                        &material).second) {
                        fail("Duplicate material path: ", material.prim_path);
                    }
                }

                shader_sources_.reserve(source.shader_nodes.size());
                for (const auto& shader : source.shader_nodes) {
                    if (!shader_sources_.emplace(
                        shader.prim_path,
                        &shader).second) {
                        fail("Duplicate shader path: ", shader.prim_path);
                    }
                }
            }

            [[nodiscard]] std::unique_ptr<StaticScene> run() {
                select_pyramid_spatial_nodes();
                build_pyramid_node_tree();

                if (result_->root_node_pyramid == StaticScene::INVALID_INDEX) {
                    fail("The Pyramid root node was not produced.");
                }
                if (result_->meshes.empty() || result_->instances.empty()) {
                    fail("The Pyramid contains no renderable mesh instances.");
                }
                return std::move(result_);
            }

        private:
            [[nodiscard]] std::uint32_t add_name(std::string_view name) {
                const auto existing = name_offsets_.find(std::string{ name });
                if (existing != name_offsets_.end()) {
                    return existing->second;
                }

                const std::size_t required = name.size() + 1u;
                if (required > std::numeric_limits<std::uint32_t>::max() ||
                    result_->names.size() >
                    std::numeric_limits<std::uint32_t>::max() - required) {
                    fail("The StaticScene name table exceeds 4 GiB.");
                }

                const auto offset = checked_u32(
                    result_->names.size(),
                    "Name offset");
                result_->names.insert(
                    result_->names.end(),
                    name.begin(),
                    name.end());
                result_->names.push_back('\0');
                name_offsets_.emplace(std::string{ name }, offset);
                return offset;
            }

            [[nodiscard]] const SourceScene::ShaderNode& find_shader(
                std::string_view path) const {

                const auto iterator = shader_sources_.find(std::string{ path });
                if (iterator == shader_sources_.end()) {
                    fail("Missing shader: ", path);
                }
                return *iterator->second;
            }

            [[nodiscard]] const SourceScene::ShaderInput* find_input(
                const SourceScene::ShaderNode& shader,
                std::string_view name) const noexcept {

                for (const auto& input : shader.inputs) {
                    if (input.name == name) {
                        return &input;
                    }
                }
                return nullptr;
            }

            [[nodiscard]] const SourceScene::ShaderOutput* find_output(
                const SourceScene::Material& material,
                std::string_view name) const noexcept {

                for (const auto& output : material.outputs) {
                    if (output.name == name) {
                        return &output;
                    }
                }
                return nullptr;
            }

            [[nodiscard]] float read_float(
                const SourceScene::ShaderInput* input,
                float fallback,
                std::string_view material_path) const {

                if (input == nullptr ||
                    input->value.kind == SourceScene::ShaderValueKind::Empty) {
                    return fallback;
                }
                if (input->value.kind !=
                    SourceScene::ShaderValueKind::Float) {
                    fail("Expected float input '", input->name,
                        "' in material ", material_path);
                }
                return std::get<float>(input->value.data);
            }

            [[nodiscard]] SourceScene::Float3 read_float3(
                const SourceScene::ShaderInput* input,
                SourceScene::Float3 fallback,
                std::string_view material_path) const {

                if (input == nullptr ||
                    input->value.kind == SourceScene::ShaderValueKind::Empty) {
                    return fallback;
                }
                if (input->value.kind !=
                    SourceScene::ShaderValueKind::Float3) {
                    fail("Expected float3 input '", input->name,
                        "' in material ", material_path);
                }
                return std::get<SourceScene::Float3>(input->value.data);
            }

            [[nodiscard]] std::string read_string(
                const SourceScene::ShaderInput* input) const {

                if (input == nullptr ||
                    input->value.kind == SourceScene::ShaderValueKind::Empty) {
                    return {};
                }
                if (input->value.kind != SourceScene::ShaderValueKind::String &&
                    input->value.kind != SourceScene::ShaderValueKind::Token) {
                    fail("Expected string/token input '", input->name, "'.");
                }
                return std::get<std::string>(input->value.data);
            }

            [[nodiscard]] std::uint32_t texture_channel(
                std::string_view source_output) const {

                if (source_output == "r") {
                    return 0u;
                }
                if (source_output == "g") {
                    return 1u;
                }
                if (source_output == "b") {
                    return 2u;
                }
                if (source_output == "a") {
                    return 3u;
                }
                return StaticScene::INVALID_INDEX;
            }

            [[nodiscard]] std::uint32_t texture_address_mode(
                std::string_view value,
                std::string_view shader_path) const {

                if (value.empty() || value == "repeat" ||
                    value == "useMetadata") {
                    return SAMPLER_ADDRESS_WRAP;
                }
                if (value == "clamp") {
                    return SAMPLER_ADDRESS_CLAMP;
                }
                if (value == "mirror") {
                    return SAMPLER_ADDRESS_MIRROR;
                }
                if (value == "black") {
                    return SAMPLER_ADDRESS_BORDER;
                }
                fail("Unsupported texture address mode '", value,
                    "' on ", shader_path);
            }

            [[nodiscard]] ResolvedTextureBinding resolve_texture_binding(
                const SourceScene::ShaderInput& surface_input,
                bool default_srgb) const {

                if (surface_input.connections.size() != 1u) {
                    fail("Material input '", surface_input.name,
                        "' must have exactly one texture connection.");
                }

                const auto& connection = surface_input.connections.front();
                const auto& texture_shader = find_shader(
                    connection.source_prim_path);
                if (texture_shader.shader_id != "UsdUVTexture") {
                    fail("Unsupported connected shader '",
                        texture_shader.shader_id,
                        "' for material input '", surface_input.name, "'.");
                }

                const auto* file_input = find_input(texture_shader, "file");
                if (file_input == nullptr ||
                    file_input->value.kind !=
                    SourceScene::ShaderValueKind::Asset) {
                    fail("UsdUVTexture has no asset file input: ",
                        texture_shader.prim_path);
                }

                const auto& asset = std::get<SourceScene::AssetReference>(
                    file_input->value.data);
                if (!asset.resolved_file_exists || asset.resolved_path.empty()) {
                    fail("Unresolved texture asset: ", asset.authored_path);
                }

                ResolvedTextureBinding result;
                result.path = std::filesystem::path{ asset.resolved_path };
                result.source_output = connection.source_name;
                result.srgb = default_srgb;

                const auto color_space = read_string(
                    find_input(texture_shader, "sourceColorSpace"));
                if (color_space == "sRGB") {
                    result.srgb = true;
                } else if (color_space == "raw") {
                    result.srgb = false;
                } else if (!color_space.empty() && color_space != "auto") {
                    fail("Unsupported sourceColorSpace '", color_space,
                        "' on ", texture_shader.prim_path);
                }

                result.sampler.address_u = texture_address_mode(
                    read_string(find_input(texture_shader, "wrapS")),
                    texture_shader.prim_path);
                result.sampler.address_v = texture_address_mode(
                    read_string(find_input(texture_shader, "wrapT")),
                    texture_shader.prim_path);

                const auto* st_input = find_input(texture_shader, "st");
                if (st_input != nullptr && !st_input->connections.empty()) {
                    if (st_input->connections.size() != 1u) {
                        fail("UsdUVTexture has multiple st connections: ",
                            texture_shader.prim_path);
                    }
                    const auto& uv_reader = find_shader(
                        st_input->connections.front().source_prim_path);
                    if (uv_reader.shader_id != "UsdPrimvarReader_float2") {
                        fail("Unsupported UV reader '", uv_reader.shader_id,
                            "' on ", texture_shader.prim_path);
                    }
                    result.uv_primvar = read_string(
                        find_input(uv_reader, "varname"));
                    if (result.uv_primvar.empty()) {
                        fail("UsdPrimvarReader_float2 has no varname: ",
                            uv_reader.prim_path);
                    }
                } else {
                    result.uv_primvar = "st";
                }

                return result;
            }

            [[nodiscard]] std::uint32_t get_sampler(
                const SamplerKey& key) {

                const auto existing = sampler_cache_.find(key);
                if (existing != sampler_cache_.end()) {
                    return existing->second;
                }

                StaticScene::Sampler sampler;
                sampler.filter = key.filter;
                sampler.address_u = key.address_u;
                sampler.address_v = key.address_v;
                sampler.address_w = key.address_w;
                sampler.max_anisotropy = key.max_anisotropy;
                sampler.name = add_name(
                    "sampler_" + std::to_string(key.filter) + "_" +
                    std::to_string(key.address_u) + "_" +
                    std::to_string(key.address_v) + "_" +
                    std::to_string(key.address_w) + "_" +
                    std::to_string(key.max_anisotropy));

                const auto index = checked_u32(
                    result_->samplers.size(),
                    "Sampler index");
                result_->samplers.push_back(sampler);
                sampler_cache_.emplace(key, index);
                return index;
            }

            void load_texture_image(
                const std::filesystem::path& path,
                DirectX::TexMetadata& metadata,
                DirectX::ScratchImage& image) const {

                std::error_code file_error;
                if (!std::filesystem::is_regular_file(path, file_error) ||
                    file_error) {
                    fail("Texture file does not exist: ", path.generic_string());
                }

                const auto extension = lowercase_extension(path);
                HRESULT result = E_FAIL;

                if (extension == ".dds") {
                    result = DirectX::LoadFromDDSFile(
                        path.c_str(),
                        DirectX::DDS_FLAGS_NONE,
                        &metadata,
                        image);
                } else if (extension == ".tga") {
                    result = DirectX::LoadFromTGAFile(
                        path.c_str(),
                        &metadata,
                        image);
                } else if (extension == ".hdr") {
                    result = DirectX::LoadFromHDRFile(
                        path.c_str(),
                        &metadata,
                        image);
                } else if (extension == ".exr") {
#if defined(FASTJUNGLE_HAS_OPENEXR)
                    result = DirectX::LoadFromEXRFile(
                        path.c_str(),
                        &metadata,
                        image);
#else
                    fail("EXR texture support is disabled: ",
                        path.generic_string());
#endif
                } else {
                    result = DirectX::LoadFromWICFile(
                        path.c_str(),
                        DirectX::WIC_FLAGS_NONE,
                        &metadata,
                        image);
                }

                if (FAILED(result)) {
                    fail_hresult("Texture decode failed", result, path);
                }
            }

            [[nodiscard]] std::uint32_t get_texture(
                const std::filesystem::path& path) {

                const auto key = normalized_texture_key(path);
                const auto existing = texture_cache_.find(key);
                if (existing != texture_cache_.end()) {
                    return existing->second;
                }

                DirectX::TexMetadata metadata{};
                DirectX::ScratchImage decoded;
                load_texture_image(path, metadata, decoded);

                if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D ||
                    metadata.arraySize != 1u || metadata.depth != 1u) {
                    fail("StaticScene currently accepts only one 2D texture: ",
                        path.generic_string());
                }

                const DirectX::Image* image = decoded.GetImage(0, 0, 0);
                if (image == nullptr || image->pixels == nullptr ||
                    image->width == 0 || image->height == 0 ||
                    image->rowPitch == 0 || image->slicePitch == 0 ||
                    image->format == DXGI_FORMAT_UNKNOWN) {
                    fail("Texture decoder returned an invalid image: ",
                        path.generic_string());
                }

                StaticScene::Texture texture;
                texture.name = add_name(path.filename().generic_string());
                texture.width = checked_u32(image->width, "Texture width");
                texture.height = checked_u32(image->height, "Texture height");
                texture.dxgi_format = static_cast<std::uint32_t>(image->format);
                texture.row_pitch = checked_u32(
                    image->rowPitch,
                    "Texture row pitch");
                texture.data_byte_offset = checked_u32(
                    result_->texture_datas.size(),
                    "Texture data offset");
                texture.data_size = checked_u32(
                    image->slicePitch,
                    "Texture data size");

                if (result_->texture_datas.size() >
                    std::numeric_limits<std::uint32_t>::max() -
                    image->slicePitch) {
                    fail("StaticScene texture data exceeds 4 GiB.");
                }

                const auto* first = reinterpret_cast<const std::byte*>(
                    image->pixels);
                result_->texture_datas.insert(
                    result_->texture_datas.end(),
                    first,
                    first + image->slicePitch);

                const auto index = checked_u32(
                    result_->textures.size(),
                    "Texture index");
                result_->textures.push_back(texture);
                texture_cache_.emplace(key, index);
                return index;
            }

            [[nodiscard]] std::uint32_t add_texture_binding(
                std::string_view semantic_name,
                const ResolvedTextureBinding& source) {

                StaticScene::TextureBinding binding;
                binding.name = add_name(semantic_name);
                binding.texture = get_texture(source.path);
                binding.sampler = get_sampler(source.sampler);
                binding.channel = texture_channel(source.source_output);
                binding.flags = source.srgb ? TEXTURE_BINDING_SRGB : 0u;

                const auto index = checked_u32(
                    result_->texture_bindings.size(),
                    "Texture binding index");
                result_->texture_bindings.push_back(binding);
                return index;
            }

            void merge_uv_primvar(
                std::string& destination,
                const ResolvedTextureBinding& binding,
                std::string_view material_path) const {

                if (destination.empty()) {
                    destination = binding.uv_primvar;
                } else if (destination != binding.uv_primvar) {
                    fail("Material uses multiple UV primvars: ", material_path);
                }
            }

            [[nodiscard]] MaterialRecord get_material(
                std::string_view material_path) {

                const auto cached = material_cache_.find(
                    std::string{ material_path });
                if (cached != material_cache_.end()) {
                    return cached->second;
                }

                const auto source_material_iterator = material_sources_.find(
                    std::string{ material_path });
                if (source_material_iterator == material_sources_.end()) {
                    fail("Missing material: ", material_path);
                }
                const auto& source_material =
                    *source_material_iterator->second;

                const auto* surface_output = find_output(
                    source_material,
                    "surface");
                if (surface_output == nullptr ||
                    surface_output->connections.size() != 1u) {
                    fail("Material must have exactly one surface connection: ",
                        material_path);
                }

                const auto& surface = find_shader(
                    surface_output->connections.front().source_prim_path);
                if (surface.shader_id != "UsdPreviewSurface") {
                    fail("Material surface is not UsdPreviewSurface: ",
                        material_path);
                }

                StaticScene::Material material;
                material.name = add_name(path_leaf(material_path));
                material.base_color = {
                    0.18f, 0.18f, 0.18f, 1.0f
                };
                material.emissive_roughness = {
                    0.0f, 0.0f, 0.0f, 0.5f
                };
                material.surface = {
                    0.0f, 1.0f, 0.0f, 0.0f
                };
                material.options = { 0u, 0u, 0u, 0u };

                std::string uv_primvar;
                bool has_textures = false;

                const auto* diffuse = find_input(surface, "diffuseColor");
                if (diffuse != nullptr && !diffuse->connections.empty()) {
                    const auto binding = resolve_texture_binding(
                        *diffuse,
                        true);
                    merge_uv_primvar(uv_primvar, binding, material_path);
                    material.texture_binding_base_color = add_texture_binding(
                        "base_color",
                        binding);
                    material.base_color = { 1.0f, 1.0f, 1.0f, 1.0f };
                    has_textures = true;
                } else {
                    const auto value = read_float3(
                        diffuse,
                        { 0.18f, 0.18f, 0.18f },
                        material_path);
                    material.base_color = { value.x, value.y, value.z, 1.0f };
                }

                const auto* normal = find_input(surface, "normal");
                if (normal != nullptr && !normal->connections.empty()) {
                    const auto binding = resolve_texture_binding(
                        *normal,
                        false);
                    merge_uv_primvar(uv_primvar, binding, material_path);
                    material.texture_binding_normal = add_texture_binding(
                        "normal",
                        binding);
                    has_textures = true;
                }

                const auto* roughness = find_input(surface, "roughness");
                material.emissive_roughness.w = read_float(
                    roughness,
                    0.5f,
                    material_path);
                if (roughness != nullptr && !roughness->connections.empty()) {
                    const auto binding = resolve_texture_binding(
                        *roughness,
                        false);
                    merge_uv_primvar(uv_primvar, binding, material_path);
                    if (texture_channel(binding.source_output) ==
                        StaticScene::INVALID_INDEX) {
                        fail("Roughness texture must use a scalar channel: ",
                            material_path);
                    }
                    material.texture_binding_roughness = add_texture_binding(
                        "roughness",
                        binding);
                    has_textures = true;
                }

                material.surface.x = read_float(
                    find_input(surface, "metallic"),
                    0.0f,
                    material_path);

                const auto* opacity = find_input(surface, "opacity");
                material.surface.y = read_float(
                    opacity,
                    1.0f,
                    material_path);
                if (opacity != nullptr && !opacity->connections.empty()) {
                    const auto binding = resolve_texture_binding(
                        *opacity,
                        false);
                    merge_uv_primvar(uv_primvar, binding, material_path);
                    if (texture_channel(binding.source_output) ==
                        StaticScene::INVALID_INDEX) {
                        fail("Opacity texture must use a scalar channel: ",
                            material_path);
                    }
                    material.texture_binding_opacity = add_texture_binding(
                        "opacity",
                        binding);
                    has_textures = true;
                }

                material.surface.z = read_float(
                    find_input(surface, "opacityThreshold"),
                    0.0f,
                    material_path);

                const auto* emissive = find_input(surface, "emissiveColor");
                if (emissive != nullptr && !emissive->connections.empty()) {
                    fail("StaticScene has no emissive texture binding: ",
                        material_path);
                }
                const auto emissive_value = read_float3(
                    emissive,
                    { 0.0f, 0.0f, 0.0f },
                    material_path);
                material.emissive_roughness.x = emissive_value.x;
                material.emissive_roughness.y = emissive_value.y;
                material.emissive_roughness.z = emissive_value.z;

                MaterialRecord record;
                record.index = checked_u32(
                    result_->materials.size(),
                    "Material index");
                record.uv_primvar = uv_primvar.empty()
                    ? "st"
                    : std::move(uv_primvar);
                record.has_textures = has_textures;
                record.alpha_tested = material.surface.z > 0.0f;

                result_->materials.push_back(material);
                material_cache_.emplace(
                    std::string{ material_path },
                    record);
                return record;
            }

            [[nodiscard]] bool is_selected_spatial_node(
                const SourceScene::Node& node) const noexcept {

                const bool spatial =
                    node.prim_kind == SourceScene::PrimKind::Transform ||
                    node.prim_kind == SourceScene::PrimKind::Mesh;
                const bool active =
                    (node.flags & SourceScene::NodeActive) != 0u;
                const bool visible =
                    (node.flags & SourceScene::NodeVisible) != 0u;
                const bool inside_native_prototype =
                    (node.flags &
                        SourceScene::NodeInsideNativePrototype) != 0u;

                return spatial && active && visible &&
                    !inside_native_prototype &&
                    node.object_kind == SourceScene::ObjectKind::Pyramid;
            }

            void select_pyramid_spatial_nodes() {
                std::uint32_t selected_count = 0;
                for (std::uint32_t index = 0;
                    index < source_.nodes.size();
                    ++index) {

                    const auto& node = source_.nodes[index];
                    if (!is_selected_spatial_node(node)) {
                        continue;
                    }
                    if ((node.flags & SourceScene::NodeNativeInstance) != 0u) {
                        fail("Native instances are not supported in the Pyramid pass: ",
                            node.path);
                    }
                    selected_nodes_[index] = true;
                    ++selected_count;
                }

                if (selected_count == 0u) {
                    fail("No active, visible Pyramid spatial node was found.");
                }

                std::vector<std::uint32_t> roots;
                for (std::uint32_t index = 0;
                    index < source_.nodes.size();
                    ++index) {

                    if (!selected_nodes_[index]) {
                        continue;
                    }

                    auto parent = source_.nodes[index].parent;
                    std::size_t traversed = 0;
                    while (parent != SourceScene::INVALID_INDEX) {
                        if (parent >= source_.nodes.size()) {
                            fail("Node parent index is out of range: ",
                                source_.nodes[index].path);
                        }
                        if (++traversed > source_.nodes.size()) {
                            fail("Cycle in the JungleScene node hierarchy.");
                        }
                        if (selected_nodes_[parent]) {
                            selected_parent_[index] = parent;
                            break;
                        }
                        parent = source_.nodes[parent].parent;
                    }

                    if (selected_parent_[index] ==
                        SourceScene::INVALID_INDEX) {
                        roots.push_back(index);
                    } else {
                        selected_children_[selected_parent_[index]].push_back(
                            index);
                    }
                }

                if (roots.size() != 1u) {
                    fail("Expected one Pyramid spatial root, found ",
                        roots.size(), ".");
                }
                pyramid_source_root_ = roots.front();
            }

            [[nodiscard]] DirectX::XMMATRIX load_matrix(
                const SourceScene::Matrix4x4& source,
                std::string_view subject) const {

                DirectX::XMFLOAT4X4 stored;
                for (std::size_t row = 0; row < 4u; ++row) {
                    for (std::size_t column = 0; column < 4u; ++column) {
                        const double value = source.values[row * 4u + column];
                        if (!std::isfinite(value) ||
                            value > std::numeric_limits<float>::max() ||
                            value < -std::numeric_limits<float>::max()) {
                            fail("Non-finite or out-of-range transform on ",
                                subject);
                        }
                        stored.m[row][column] = static_cast<float>(value);
                    }
                }
                return DirectX::XMLoadFloat4x4(&stored);
            }

            [[nodiscard]] DirectX::XMMATRIX world_matrix(
                std::uint32_t node_index) {

                if (node_index >= source_.nodes.size()) {
                    fail("World-transform node index is out of range.");
                }
                if (world_states_[node_index] == 2u) {
                    return DirectX::XMLoadFloat4x4(
                        &world_cache_[node_index]);
                }
                if (world_states_[node_index] == 1u) {
                    fail("Cycle in JungleScene transforms at ",
                        source_.nodes[node_index].path);
                }

                world_states_[node_index] = 1u;
                const auto& node = source_.nodes[node_index];
                auto result = load_matrix(node.local_transform, node.path);

                const bool resets_transform =
                    (node.flags & SourceScene::NodeResetsTransform) != 0u;
                if (!resets_transform &&
                    node.parent != SourceScene::INVALID_INDEX) {
                    if (node.parent >= source_.nodes.size()) {
                        fail("Node parent index is out of range: ", node.path);
                    }
                    result = DirectX::XMMatrixMultiply(
                        result,
                        world_matrix(node.parent));
                }

                DirectX::XMStoreFloat4x4(
                    &world_cache_[node_index],
                    result);
                world_states_[node_index] = 2u;
                return result;
            }

            [[nodiscard]] DirectX::XMFLOAT4X4 relative_transform(
                std::uint32_t source_node,
                std::uint32_t source_parent) {

                auto result = world_matrix(source_node);
                if (source_parent != SourceScene::INVALID_INDEX) {
                    DirectX::XMVECTOR determinant;
                    const auto inverse_parent = DirectX::XMMatrixInverse(
                        &determinant,
                        world_matrix(source_parent));
                    const float determinant_value =
                        DirectX::XMVectorGetX(determinant);
                    if (!std::isfinite(determinant_value) ||
                        std::abs(determinant_value) <
                        MATRIX_DETERMINANT_EPSILON) {
                        fail("Non-invertible parent transform: ",
                            source_.nodes[source_parent].path);
                    }
                    result = DirectX::XMMatrixMultiply(
                        result,
                        inverse_parent);
                }

                DirectX::XMFLOAT4X4 stored;
                DirectX::XMStoreFloat4x4(&stored, result);
                const float* values = &stored._11;
                for (std::size_t i = 0; i < 16u; ++i) {
                    if (!std::isfinite(values[i])) {
                        fail("Non-finite relative transform: ",
                            source_.nodes[source_node].path);
                    }
                }
                return stored;
            }

            [[nodiscard]] std::vector<std::size_t> face_corner_offsets(
                const SourceScene::Mesh& mesh) const {

                std::vector<std::size_t> result(
                    mesh.face_vertex_counts.size());
                std::size_t corner = 0;
                for (std::size_t face = 0;
                    face < mesh.face_vertex_counts.size();
                    ++face) {

                    const auto count = mesh.face_vertex_counts[face];
                    if (count < 0) {
                        fail("Negative face vertex count on ", mesh.prim_path);
                    }
                    result[face] = corner;
                    const auto unsigned_count = static_cast<std::size_t>(count);
                    if (corner > std::numeric_limits<std::size_t>::max() -
                        unsigned_count) {
                        fail("Face corner count overflow on ", mesh.prim_path);
                    }
                    corner += unsigned_count;
                }

                if (corner != mesh.face_vertex_indices.size()) {
                    fail("Face counts do not match face indices on ",
                        mesh.prim_path);
                }
                return result;
            }

            [[nodiscard]] std::vector<std::uint8_t> hole_faces(
                const SourceScene::Mesh& mesh) const {

                std::vector<std::uint8_t> result(
                    mesh.face_vertex_counts.size(),
                    0u);
                for (const auto signed_face : mesh.hole_indices) {
                    if (signed_face < 0 ||
                        static_cast<std::size_t>(signed_face) >= result.size()) {
                        fail("Invalid hole face on ", mesh.prim_path);
                    }
                    result[static_cast<std::size_t>(signed_face)] = 1u;
                }
                return result;
            }

            [[nodiscard]] std::vector<FaceGroup> build_face_groups(
                const SourceScene::Mesh& mesh) const {

                if (mesh.material_path.empty()) {
                    fail("Mesh has no default material: ", mesh.prim_path);
                }

                std::vector<std::string> face_materials(
                    mesh.face_vertex_counts.size(),
                    mesh.material_path);
                std::vector<std::uint8_t> subset_owned(
                    mesh.face_vertex_counts.size(),
                    0u);

                for (const auto& subset : source_.mesh_subsets) {
                    if (subset.mesh_path != mesh.prim_path) {
                        continue;
                    }
                    if (subset.element_type != "face") {
                        fail("Only face material subsets are supported: ",
                            subset.prim_path);
                    }
                    if (subset.material_path.empty()) {
                        fail("Material subset is unbound: ", subset.prim_path);
                    }
                    if (!subset.family_type.empty() &&
                        subset.family_type != "nonOverlapping" &&
                        subset.family_type != "partition") {
                        fail("Unsupported material subset family type '",
                            subset.family_type, "': ", subset.prim_path);
                    }

                    for (const auto signed_face : subset.indices) {
                        if (signed_face < 0 ||
                            static_cast<std::size_t>(signed_face) >=
                            face_materials.size()) {
                            fail("Material subset face is out of range: ",
                                subset.prim_path);
                        }
                        const auto face = static_cast<std::size_t>(
                            signed_face);
                        if (subset_owned[face] != 0u) {
                            fail("Overlapping material subsets on mesh ",
                                mesh.prim_path);
                        }
                        subset_owned[face] = 1u;
                        face_materials[face] = subset.material_path;
                    }
                }

                std::vector<FaceGroup> groups;
                std::unordered_map<std::string, std::size_t> group_indices;
                for (std::uint32_t face = 0;
                    face < face_materials.size();
                    ++face) {

                    const auto& material_path = face_materials[face];
                    const auto [iterator, inserted] = group_indices.emplace(
                        material_path,
                        groups.size());
                    if (inserted) {
                        groups.push_back({ material_path, {} });
                    }
                    groups[iterator->second].faces.push_back(face);
                }
                return groups;
            }

            [[nodiscard]] std::size_t interpolation_index(
                std::string_view interpolation,
                std::size_t face,
                std::size_t corner,
                std::uint32_t point,
                std::string_view subject) const {

                if (interpolation.empty() || interpolation == "constant") {
                    return 0u;
                }
                if (interpolation == "uniform") {
                    return face;
                }
                if (interpolation == "vertex" ||
                    interpolation == "varying") {
                    return point;
                }
                if (interpolation == "faceVarying") {
                    return corner;
                }
                fail("Unsupported interpolation '", interpolation,
                    "' on ", subject);
            }

            [[nodiscard]] const SourceScene::Primvar* find_uv_primvar(
                const SourceScene::Mesh& mesh,
                const MaterialRecord& material) const {

                if (!material.has_textures) {
                    return nullptr;
                }
                for (const auto& primvar : mesh.primvars) {
                    if (primvar.name == material.uv_primvar) {
                        if (primvar.storage !=
                            SourceScene::PrimvarStorage::Float2) {
                            fail("UV primvar is not float2: ",
                                material.uv_primvar, " on ", mesh.prim_path);
                        }
                        return &primvar;
                    }
                }
                fail("Missing UV primvar '", material.uv_primvar,
                    "' on ", mesh.prim_path);
            }

            [[nodiscard]] SourceScene::Float2 read_uv(
                const SourceScene::Mesh& mesh,
                const SourceScene::Primvar* primvar,
                std::size_t face,
                std::size_t corner,
                std::uint32_t point) const {

                if (primvar == nullptr) {
                    return {};
                }

                const auto source_index = interpolation_index(
                    primvar->interpolation,
                    face,
                    corner,
                    point,
                    mesh.prim_path);

                std::int64_t value_index = static_cast<std::int64_t>(
                    source_index);
                if (!primvar->indices.empty()) {
                    if (source_index >= primvar->indices.size()) {
                        fail("UV index stream is too small on ",
                            mesh.prim_path);
                    }
                    value_index = primvar->indices[source_index];
                }

                const auto* values = std::get_if<
                    std::vector<SourceScene::Float2>>(&primvar->data);
                if (values == nullptr || value_index < 0 ||
                    static_cast<std::size_t>(value_index) >= values->size()) {
                    fail("UV value index is invalid on ", mesh.prim_path);
                }
                return (*values)[static_cast<std::size_t>(value_index)];
            }

            [[nodiscard]] DirectX::XMVECTOR read_normal(
                const SourceScene::Mesh& mesh,
                std::size_t face,
                std::size_t corner,
                std::uint32_t point) const {

                if (mesh.normals.empty()) {
                    return DirectX::XMVectorZero();
                }

                const auto index = interpolation_index(
                    mesh.normals_interpolation,
                    face,
                    corner,
                    point,
                    mesh.prim_path);
                if (index >= mesh.normals.size()) {
                    fail("Normal index is invalid on ", mesh.prim_path);
                }

                const auto& normal = mesh.normals[index];
                return DirectX::XMVectorSet(
                    normal.x,
                    normal.y,
                    normal.z,
                    0.0f);
            }

            void calculate_triangle_basis(
                std::array<StaticScene::Vertex, 3>& triangle) const {

                const auto p0 = DirectX::XMLoadFloat3(&triangle[0].position);
                const auto p1 = DirectX::XMLoadFloat3(&triangle[1].position);
                const auto p2 = DirectX::XMLoadFloat3(&triangle[2].position);
                const auto edge1 = DirectX::XMVectorSubtract(p1, p0);
                const auto edge2 = DirectX::XMVectorSubtract(p2, p0);

                auto face_normal = DirectX::XMVector3Cross(edge1, edge2);
                if (DirectX::XMVectorGetX(
                    DirectX::XMVector3LengthSq(face_normal)) <
                    VECTOR_EPSILON) {
                    face_normal = DirectX::XMVectorSet(
                        0.0f, 0.0f, 1.0f, 0.0f);
                } else {
                    face_normal = DirectX::XMVector3Normalize(face_normal);
                }

                for (auto& vertex : triangle) {
                    auto normal = DirectX::XMLoadFloat3(&vertex.normal);
                    if (DirectX::XMVectorGetX(
                        DirectX::XMVector3LengthSq(normal)) <
                        VECTOR_EPSILON) {
                        normal = face_normal;
                    } else {
                        normal = DirectX::XMVector3Normalize(normal);
                    }
                    DirectX::XMStoreFloat3(&vertex.normal, normal);
                }

                const float du1 = triangle[1].uv.x - triangle[0].uv.x;
                const float dv1 = triangle[1].uv.y - triangle[0].uv.y;
                const float du2 = triangle[2].uv.x - triangle[0].uv.x;
                const float dv2 = triangle[2].uv.y - triangle[0].uv.y;
                const float determinant = du1 * dv2 - du2 * dv1;

                DirectX::XMVECTOR tangent;
                DirectX::XMVECTOR bitangent;
                if (std::abs(determinant) > 1.0e-8f) {
                    const float inverse = 1.0f / determinant;
                    tangent = DirectX::XMVectorScale(
                        DirectX::XMVectorSubtract(
                            DirectX::XMVectorScale(edge1, dv2),
                            DirectX::XMVectorScale(edge2, dv1)),
                        inverse);
                    bitangent = DirectX::XMVectorScale(
                        DirectX::XMVectorSubtract(
                            DirectX::XMVectorScale(edge2, du1),
                            DirectX::XMVectorScale(edge1, du2)),
                        inverse);
                } else {
                    tangent = edge1;
                    bitangent = edge2;
                }

                for (auto& vertex : triangle) {
                    const auto normal = DirectX::XMLoadFloat3(&vertex.normal);
                    auto orthogonal = DirectX::XMVectorSubtract(
                        tangent,
                        DirectX::XMVectorScale(
                            normal,
                            DirectX::XMVectorGetX(
                                DirectX::XMVector3Dot(normal, tangent))));

                    if (DirectX::XMVectorGetX(
                        DirectX::XMVector3LengthSq(orthogonal)) <
                        VECTOR_EPSILON) {
                        const auto axis = std::abs(vertex.normal.z) < 0.9f
                            ? DirectX::XMVectorSet(
                                0.0f, 0.0f, 1.0f, 0.0f)
                            : DirectX::XMVectorSet(
                                0.0f, 1.0f, 0.0f, 0.0f);
                        orthogonal = DirectX::XMVector3Cross(axis, normal);
                    }
                    orthogonal = DirectX::XMVector3Normalize(orthogonal);

                    float sign = 1.0f;
                    if (DirectX::XMVectorGetX(
                        DirectX::XMVector3LengthSq(bitangent)) >=
                        VECTOR_EPSILON) {
                        sign = DirectX::XMVectorGetX(
                            DirectX::XMVector3Dot(
                                DirectX::XMVector3Cross(
                                    normal,
                                    orthogonal),
                                bitangent)) < 0.0f
                            ? -1.0f
                            : 1.0f;
                    }

                    DirectX::XMStoreFloat4(
                        &vertex.tangent,
                        DirectX::XMVectorSetW(orthogonal, sign));
                }
            }

            [[nodiscard]] bool cook_submesh(
                const SourceScene::Mesh& mesh,
                const FaceGroup& group,
                const MaterialRecord& material,
                const std::vector<std::size_t>& corners,
                const std::vector<std::uint8_t>& holes) {

                const auto* uv_primvar = find_uv_primvar(mesh, material);

                std::uint64_t triangle_count = 0;
                for (const auto face : group.faces) {
                    if (holes[face] != 0u) {
                        continue;
                    }
                    const auto count = mesh.face_vertex_counts[face];
                    if (count >= 3) {
                        triangle_count += static_cast<std::uint64_t>(count - 2);
                    }
                }
                if (triangle_count == 0u) {
                    return false;
                }
                if (triangle_count >
                    std::numeric_limits<std::uint32_t>::max() / 3u) {
                    fail("Submesh is too large: ", group.material_path);
                }

                StaticScene::Submesh submesh;
                submesh.name = add_name(path_leaf(group.material_path));
                submesh.vertex_offset = checked_u32(
                    result_->vertices.size(),
                    "Submesh vertex offset");
                submesh.index_offset = checked_u32(
                    result_->indices_local.size(),
                    "Submesh index offset");
                submesh.material = material.index;
                if (mesh.double_sided) {
                    submesh.flags |= SUBMESH_DOUBLE_SIDED;
                }
                if (material.alpha_tested) {
                    submesh.flags |= SUBMESH_ALPHA_TESTED;
                }

                const auto additional_vertices = static_cast<std::size_t>(
                    triangle_count * 3u);
                if (result_->vertices.size() >
                    std::numeric_limits<std::uint32_t>::max() -
                    additional_vertices ||
                    result_->indices_local.size() >
                    std::numeric_limits<std::uint32_t>::max() -
                    additional_vertices) {
                    fail("StaticScene geometry exceeds 32-bit element offsets.");
                }
                result_->vertices.reserve(
                    result_->vertices.size() + additional_vertices);
                result_->indices_local.reserve(
                    result_->indices_local.size() + additional_vertices);

                const bool left_handed = mesh.orientation == "leftHanded";
                if (!left_handed && mesh.orientation != "rightHanded") {
                    fail("Unsupported mesh orientation '", mesh.orientation,
                        "' on ", mesh.prim_path);
                }

                for (const auto face : group.faces) {
                    if (holes[face] != 0u) {
                        continue;
                    }

                    const auto count = static_cast<std::size_t>(
                        mesh.face_vertex_counts[face]);
                    if (count < 3u) {
                        continue;
                    }
                    const auto first_corner = corners[face];

                    for (std::size_t triangle_index = 1u;
                        triangle_index + 1u < count;
                        ++triangle_index) {

                        std::array<std::size_t, 3> triangle_corners{
                            first_corner,
                            first_corner + triangle_index,
                            first_corner + triangle_index + 1u
                        };
                        if (left_handed) {
                            std::swap(
                                triangle_corners[1],
                                triangle_corners[2]);
                        }

                        std::array<StaticScene::Vertex, 3> triangle{};
                        for (std::size_t vertex_index = 0;
                            vertex_index < triangle.size();
                            ++vertex_index) {

                            const auto corner = triangle_corners[vertex_index];
                            const auto signed_point =
                                mesh.face_vertex_indices[corner];
                            if (signed_point < 0 ||
                                static_cast<std::size_t>(signed_point) >=
                                mesh.points.size()) {
                                fail("Point index is invalid on ", mesh.prim_path);
                            }
                            const auto point = static_cast<std::uint32_t>(
                                signed_point);
                            const auto& position = mesh.points[point];
                            if (!std::isfinite(position.x) ||
                                !std::isfinite(position.y) ||
                                !std::isfinite(position.z)) {
                                fail("Non-finite point on ", mesh.prim_path);
                            }
                            triangle[vertex_index].position = {
                                position.x,
                                position.y,
                                position.z
                            };

                            const auto normal = read_normal(
                                mesh,
                                face,
                                corner,
                                point);
                            DirectX::XMStoreFloat3(
                                &triangle[vertex_index].normal,
                                normal);

                            const auto uv = read_uv(
                                mesh,
                                uv_primvar,
                                face,
                                corner,
                                point);
                            if (!std::isfinite(uv.x) || !std::isfinite(uv.y)) {
                                fail("Non-finite UV on ", mesh.prim_path);
                            }
                            triangle[vertex_index].uv = { uv.x, uv.y };
                        }

                        calculate_triangle_basis(triangle);
                        for (const auto& vertex : triangle) {
                            const auto local_index = checked_u32(
                                result_->vertices.size() -
                                submesh.vertex_offset,
                                "Local vertex index");
                            result_->vertices.push_back(vertex);
                            result_->indices_local.push_back(local_index);
                        }
                    }
                }

                submesh.vertex_count = checked_u32(
                    result_->vertices.size() - submesh.vertex_offset,
                    "Submesh vertex count");
                submesh.index_count = checked_u32(
                    result_->indices_local.size() - submesh.index_offset,
                    "Submesh index count");
                result_->submeshes.push_back(submesh);
                return true;
            }

            [[nodiscard]] std::uint32_t cook_mesh(
                std::uint32_t source_mesh_index,
                std::string_view instance_name) {

                const auto cached = mesh_cache_.find(source_mesh_index);
                if (cached != mesh_cache_.end()) {
                    return cached->second;
                }
                if (source_mesh_index >= source_.meshes.size()) {
                    fail("Mesh payload is out of range for ", instance_name);
                }

                const auto& mesh = source_.meshes[source_mesh_index];
                if (mesh.subdivision_scheme != "none") {
                    fail("Subdivision is not cooked yet on ", mesh.prim_path);
                }

                const auto corners = face_corner_offsets(mesh);
                const auto holes = hole_faces(mesh);
                const auto groups = build_face_groups(mesh);

                StaticScene::Mesh destination;
                destination.name = add_name(path_leaf(mesh.prim_path));
                destination.submesh_offset = checked_u32(
                    result_->submeshes.size(),
                    "Mesh submesh offset");

                for (const auto& group : groups) {
                    const auto material = get_material(group.material_path);
                    if (cook_submesh(
                        mesh,
                        group,
                        material,
                        corners,
                        holes)) {
                        ++destination.submesh_count;
                    }
                }

                if (destination.submesh_count == 0u) {
                    fail("Mesh produced no triangles: ", mesh.prim_path);
                }

                const auto destination_index = checked_u32(
                    result_->meshes.size(),
                    "Mesh index");
                result_->meshes.push_back(destination);
                mesh_cache_.emplace(source_mesh_index, destination_index);
                return destination_index;
            }

            [[nodiscard]] StaticScene::Node make_node(
                std::uint32_t source_node_index,
                std::uint32_t source_parent_index) {

                const auto& source_node = source_.nodes[source_node_index];
                StaticScene::Node destination;
                destination.name = add_name(source_node.name);
                destination.local_transform = relative_transform(
                    source_node_index,
                    source_parent_index);

                if (source_node.prim_kind == SourceScene::PrimKind::Mesh) {
                    if (source_node.payload == SourceScene::INVALID_INDEX) {
                        fail("Mesh node has no payload: ", source_node.path);
                    }
                    const auto mesh_index = cook_mesh(
                        source_node.payload,
                        source_node.name);

                    StaticScene::Instance instance;
                    instance.name = add_name(source_node.name);
                    instance.mesh = mesh_index;
                    destination.instance_optional = checked_u32(
                        result_->instances.size(),
                        "Instance index");
                    result_->instances.push_back(instance);
                }
                return destination;
            }

            void build_pyramid_node_tree() {
                result_->root_node_pyramid = 0u;
                result_->nodes.push_back(make_node(
                    pyramid_source_root_,
                    SourceScene::INVALID_INDEX));

                struct PendingNode {
                    std::uint32_t source_index;
                    std::uint32_t destination_index;
                };
                std::queue<PendingNode> pending;
                pending.push({ pyramid_source_root_, 0u });

                while (!pending.empty()) {
                    const auto current = pending.front();
                    pending.pop();

                    const auto& children = selected_children_[
                        current.source_index];
                    if (children.empty()) {
                        continue;
                    }

                    const auto first_child = checked_u32(
                        result_->nodes.size(),
                        "Child node offset");
                    result_->nodes[current.destination_index]
                        .child_node_offset = first_child;
                    result_->nodes[current.destination_index]
                        .child_node_count = checked_u32(
                            children.size(),
                            "Child node count");

                    for (const auto source_child : children) {
                        const auto destination_child = checked_u32(
                            result_->nodes.size(),
                            "Node index");
                        result_->nodes.push_back(make_node(
                            source_child,
                            current.source_index));
                        pending.push({ source_child, destination_child });
                    }
                }
            }

        private:
            const SourceScene& source_;
            std::unique_ptr<StaticScene> result_;
            ComScope com_scope_{};

            std::unordered_map<std::string, std::uint32_t> name_offsets_;
            std::unordered_map<
                std::string,
                const SourceScene::Material*> material_sources_;
            std::unordered_map<
                std::string,
                const SourceScene::ShaderNode*> shader_sources_;

            std::unordered_map<std::string, MaterialRecord> material_cache_;
            std::unordered_map<std::uint32_t, std::uint32_t> mesh_cache_;
            std::unordered_map<std::string, std::uint32_t> texture_cache_;
            std::unordered_map<
                SamplerKey,
                std::uint32_t,
                SamplerKeyHash> sampler_cache_;

            std::vector<DirectX::XMFLOAT4X4> world_cache_;
            std::vector<std::uint8_t> world_states_;

            std::vector<bool> selected_nodes_;
            std::vector<std::uint32_t> selected_parent_;
            std::vector<std::vector<std::uint32_t>> selected_children_;
            std::uint32_t pyramid_source_root_ = SourceScene::INVALID_INDEX;
        };

    } // namespace

    std::unique_ptr<scene::StaticScene> StaticSceneBuilder::build(
        const JungleScene& src) {

        try {
            return Builder{ src }.run();
        } catch (const std::exception& exception) {
            fail("Unexpected C++ exception: ", exception.what());
        } catch (...) {
            fail("Unexpected non-standard exception.");
        }
    }

} // namespace fjr::cooker
