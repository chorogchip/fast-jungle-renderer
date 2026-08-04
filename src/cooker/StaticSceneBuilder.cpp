#include "FastJungle/cooker/StaticSceneBuilder.hpp"

#include "FastJungle/scene/StaticScene.hpp"

#include <pxr/base/plug/registry.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/quath.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/listOp.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/primSpec.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/subset.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/lightAPI.h>
#include <pxr/usd/usdShade/connectableAPI.h>
#include <pxr/usd/usdShade/input.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/output.h>
#include <pxr/usd/usdShade/shader.h>

#include <Windows.h>

#include <algorithm>
#include <bit>
#include <cctype>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fjr::cooker {

    namespace {

        using StaticScene = scene::StaticScene;
        using ObjectKind = StaticScene::EnumObjectKind;
        using AddressMode = StaticScene::EnumSamplerAddressMode;
        using TextureChannel = StaticScene::EnumTextureChannel;
        using BindingFlag = StaticScene::EnumTextureBindingFlag;
        using SubmeshFlag = StaticScene::EnumSubmeshFlag;

        constexpr float VECTOR_EPSILON = 1.0e-10f;
        constexpr std::size_t MAX_PROTOTYPE_DEPTH = 64;

        [[noreturn]] void fail(std::string message) {
            throw std::runtime_error(std::move(message));
        }

        template<typename... Parts>
        [[noreturn]] void fail(Parts&&... parts) {
            std::string message;
            (message.append(std::forward<Parts>(parts)), ...);
            fail(std::move(message));
        }

        [[nodiscard]] std::uint32_t checked_u32(
            std::size_t value,
            std::string_view subject) {

            if (value > std::numeric_limits<std::uint32_t>::max()) {
                fail(std::string{subject}, " exceeds uint32_t.");
            }
            return static_cast<std::uint32_t>(value);
        }

        [[nodiscard]] std::array<std::uint32_t, 8> vertex_words(
            const StaticScene::Vertex& vertex) noexcept {

            static_assert(sizeof(StaticScene::Vertex) ==
                sizeof(std::array<std::uint32_t, 8>));
            return std::bit_cast<std::array<std::uint32_t, 8>>(vertex);
        }

        [[nodiscard]] std::uint64_t hash_vertex(
            const StaticScene::Vertex& vertex) noexcept {

            std::uint64_t hash = 14695981039346656037ull;
            for (const auto word : vertex_words(vertex)) {
                hash ^= word;
                hash *= 1099511628211ull;
            }
            return hash;
        }

        [[nodiscard]] bool vertices_equal(
            const StaticScene::Vertex& left,
            const StaticScene::Vertex& right) noexcept {

            return vertex_words(left) == vertex_words(right);
        }

        class VertexIndexer final {
        public:
            VertexIndexer(
                std::vector<StaticScene::Vertex>& vertices,
                std::uint32_t vertex_offset)
                : vertices_(vertices), vertex_offset_(vertex_offset) {}

            [[nodiscard]] std::uint32_t get_or_add(
                const StaticScene::Vertex& vertex) {

                if (slots_.empty() ||
                    (unique_count_ + 1) * 10 > slots_.size() * 7) {
                    rehash(slots_.empty() ? 16 : slots_.size() * 2);
                }

                const std::size_t slot = find_slot(vertex, slots_);
                const std::uint32_t existing = slots_[slot];
                if (existing != StaticScene::INVALID_INDEX) {
                    return existing;
                }

                const std::uint32_t local_index = checked_u32(
                    vertices_.size() - vertex_offset_,
                    "Indexed submesh vertex count");
                vertices_.push_back(vertex);
                slots_[slot] = local_index;
                ++unique_count_;
                return local_index;
            }

        private:
            [[nodiscard]] std::size_t find_slot(
                const StaticScene::Vertex& vertex,
                const std::vector<std::uint32_t>& slots) const noexcept {

                const std::size_t mask = slots.size() - 1;
                std::size_t slot = static_cast<std::size_t>(
                    hash_vertex(vertex)) & mask;
                while (slots[slot] != StaticScene::INVALID_INDEX &&
                    !vertices_equal(
                        vertices_[vertex_offset_ + slots[slot]],
                        vertex)) {
                    slot = (slot + 1) & mask;
                }
                return slot;
            }

            void rehash(std::size_t capacity) {
                std::vector<std::uint32_t> replacement(
                    capacity,
                    StaticScene::INVALID_INDEX);
                for (std::uint32_t local_index = 0;
                     local_index < unique_count_;
                     ++local_index) {
                    const auto& vertex =
                        vertices_[vertex_offset_ + local_index];
                    const std::size_t slot = find_slot(vertex, replacement);
                    replacement[slot] = local_index;
                }
                slots_ = std::move(replacement);
            }

            std::vector<StaticScene::Vertex>& vertices_;
            std::uint32_t vertex_offset_ = 0;
            std::uint32_t unique_count_ = 0;
            std::vector<std::uint32_t> slots_;
        };

        [[nodiscard]] std::filesystem::path executable_directory() {
            std::vector<wchar_t> buffer(1024);
            for (;;) {
                const DWORD length = GetModuleFileNameW(
                    nullptr,
                    buffer.data(),
                    static_cast<DWORD>(buffer.size()));
                if (length == 0) {
                    fail("GetModuleFileNameW failed.");
                }
                if (length < buffer.size() - 1) {
                    return std::filesystem::path{
                        std::wstring_view{buffer.data(), length}}
                        .parent_path();
                }
                buffer.resize(buffer.size() * 2);
            }
        }

        void register_openusd_plugins() {
            static std::once_flag once;
            std::call_once(once, [] {
                const auto runtime_root = executable_directory() / "openusd";
                const std::array manifests{
                    runtime_root / "lib/usd/plugInfo.json",
                    runtime_root / "plugin/usd/plugInfo.json"
                };

                for (const auto& manifest : manifests) {
                    if (!std::filesystem::is_regular_file(manifest)) {
                        fail(
                            "Missing OpenUSD plugin manifest: ",
                            manifest.generic_string());
                    }
                    pxr::PlugRegistry::GetInstance().RegisterPlugins(
                        manifest.generic_string());
                }
            });
        }

        [[nodiscard]] std::string path_leaf(std::string_view path) {
            const auto separator = path.find_last_of('/');
            return separator == std::string_view::npos
                ? std::string{path}
                : std::string{path.substr(separator + 1)};
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

        [[nodiscard]] bool is_visible(const pxr::UsdPrim& prim) {
            if (!prim || !prim.IsActive()) {
                return false;
            }
            const pxr::UsdGeomImageable imageable{prim};
            return !imageable ||
                imageable.ComputeVisibility() != pxr::UsdGeomTokens->invisible;
        }

        class CoordinateConverter final {
        public:
            explicit CoordinateConverter(double meters_per_unit)
                : scale_(static_cast<float>(meters_per_unit)) {

                if (!(scale_ > 0.0f) || !std::isfinite(scale_)) {
                    fail("Invalid stage metersPerUnit.");
                }

                const float inverse = 1.0f / scale_;
                source_to_target_ = DirectX::XMMatrixSet(
                    scale_, 0.0f,   0.0f,   0.0f,
                    0.0f,   0.0f,   scale_, 0.0f,
                    0.0f,   scale_, 0.0f,   0.0f,
                    0.0f,   0.0f,   0.0f,   1.0f);
                target_to_source_ = DirectX::XMMatrixSet(
                    inverse, 0.0f,    0.0f,    0.0f,
                    0.0f,    0.0f,    inverse, 0.0f,
                    0.0f,    inverse, 0.0f,    0.0f,
                    0.0f,    0.0f,    0.0f,    1.0f);
            }

            [[nodiscard]] DirectX::XMFLOAT3 position(
                const pxr::GfVec3f& source) const noexcept {

                return {
                    source[0] * scale_,
                    source[2] * scale_,
                    source[1] * scale_
                };
            }

            [[nodiscard]] DirectX::XMFLOAT3 direction(
                const pxr::GfVec3f& source) const noexcept {

                DirectX::XMFLOAT3 result{
                    source[0],
                    source[2],
                    source[1]
                };
                const auto value = DirectX::XMLoadFloat3(&result);
                const float length_sq = DirectX::XMVectorGetX(
                    DirectX::XMVector3LengthSq(value));
                if (length_sq > VECTOR_EPSILON) {
                    DirectX::XMStoreFloat3(
                        &result,
                        DirectX::XMVector3Normalize(value));
                }
                return result;
            }

            [[nodiscard]] DirectX::XMFLOAT4 orientation(
                const pxr::GfQuath& source) const noexcept {

                const auto imaginary = source.GetImaginary();
                DirectX::XMFLOAT4 result{
                    -static_cast<float>(imaginary[0]),
                    -static_cast<float>(imaginary[2]),
                    -static_cast<float>(imaginary[1]),
                    static_cast<float>(source.GetReal())
                };

                auto quaternion = DirectX::XMLoadFloat4(&result);
                if (DirectX::XMVectorGetX(
                    DirectX::XMVector4LengthSq(quaternion)) <=
                    VECTOR_EPSILON) {
                    return {0.0f, 0.0f, 0.0f, 1.0f};
                }
                quaternion = DirectX::XMQuaternionNormalize(quaternion);
                DirectX::XMStoreFloat4(&result, quaternion);
                return result;
            }

            [[nodiscard]] DirectX::XMFLOAT3 scale(
                const pxr::GfVec3f& source) const noexcept {

                return {source[0], source[2], source[1]};
            }

            [[nodiscard]] DirectX::XMMATRIX matrix(
                const pxr::GfMatrix4d& source) const noexcept {

                DirectX::XMFLOAT4X4 stored;
                for (std::size_t row = 0; row < 4; ++row) {
                    for (std::size_t column = 0; column < 4; ++column) {
                        stored.m[row][column] = static_cast<float>(
                            source[static_cast<int>(row)]
                                  [static_cast<int>(column)]);
                    }
                }

                const auto source_matrix = DirectX::XMLoadFloat4x4(&stored);
                return DirectX::XMMatrixMultiply(
                    DirectX::XMMatrixMultiply(
                        target_to_source_,
                        source_matrix),
                    source_to_target_);
            }

        private:
            float scale_ = 1.0f;
            DirectX::XMMATRIX source_to_target_{};
            DirectX::XMMATRIX target_to_source_{};
        };

        [[nodiscard]] DirectX::XMFLOAT4X4 store_matrix(
            DirectX::FXMMATRIX source) noexcept {

            DirectX::XMFLOAT4X4 result;
            DirectX::XMStoreFloat4x4(&result, source);
            return result;
        }

        struct SamplerKey {
            AddressMode address_u = AddressMode::WRAP;
            AddressMode address_v = AddressMode::WRAP;

            bool operator==(const SamplerKey&) const = default;
        };

        struct SamplerKeyHash {
            [[nodiscard]] std::size_t operator()(
                const SamplerKey& value) const noexcept {

                return static_cast<std::size_t>(value.address_u) |
                    (static_cast<std::size_t>(value.address_v) << 8u);
            }
        };

        struct ResolvedTextureBinding {
            std::filesystem::path path;
            std::string uv_primvar = "st";
            TextureChannel channel = TextureChannel::RGBA;
            bool srgb = false;
            SamplerKey sampler{};
        };

        enum class AlphaMode {
            Opaque,
            Tested,
            Blended
        };

        struct MaterialRecord {
            std::uint32_t index = StaticScene::INVALID_INDEX;
            std::string uv_primvar = "st";
            bool has_textures = false;
            AlphaMode alpha_mode = AlphaMode::Opaque;
        };

        struct FaceGroup {
            std::string material_path;
            pxr::UsdShadeMaterial material;
            std::vector<std::uint32_t> faces;
        };

        struct UvData {
            pxr::TfToken interpolation;
            pxr::VtVec2fArray values;
        };

        struct PendingMatrixBatch {
            std::uint32_t name = StaticScene::INVALID_INDEX;
            std::uint32_t source_prim_path = StaticScene::INVALID_INDEX;
            std::uint32_t source_layer = StaticScene::INVALID_INDEX;
            std::uint32_t prototype = StaticScene::INVALID_INDEX;
            std::vector<StaticScene::MatrixInstance> instances;
        };

        class Builder final {
        public:
            explicit Builder(pxr::UsdStageRefPtr stage)
                : stage_(std::move(stage)),
                  converter_(pxr::UsdGeomGetStageMetersPerUnit(stage_)),
                  xform_cache_(pxr::UsdTimeCode::Default()),
                  result_(std::make_unique<StaticScene>()) {

                result_->strings.push_back('\0');
                string_offsets_.emplace(std::string{}, 0u);
                build_source_provenance();

                result_->point_instances.reserve(8'674'676);
                result_->point_batches.reserve(778);
                result_->prototypes.reserve(256);
                result_->prototype_parts.reserve(512);
                result_->meshes.reserve(160);
                result_->submeshes.reserve(256);
                result_->materials.reserve(192);
                result_->textures.reserve(600);
                result_->texture_bindings.reserve(800);
            }

            [[nodiscard]] StaticSceneBuild run() {
                if (pxr::UsdGeomGetStageUpAxis(stage_) != pxr::UsdGeomTokens->z) {
                    fail("Intel Jungle root stage must be Z-up.");
                }

                collect_point_instancers();
                build_point_batches();
                build_direct_scene();
                flatten_matrix_batches();

                if (result_->vertices.empty() || result_->meshes.empty()) {
                    fail("Intel Jungle produced no renderable meshes.");
                }
                if (result_->point_batches.empty()) {
                    fail("Intel Jungle produced no PointInstancer batches.");
                }

                result_->info.vertex_count_after_indexing =
                    result_->vertices.size();

                return {
                    .scene = std::move(result_),
                    .texture_paths = std::move(texture_paths_)
                };
            }

        private:
            [[nodiscard]] std::uint32_t add_string(std::string_view value) {
                const auto existing = string_offsets_.find(std::string{value});
                if (existing != string_offsets_.end()) {
                    return existing->second;
                }

                const auto offset = checked_u32(
                    result_->strings.size(),
                    "String table offset");
                result_->strings.insert(
                    result_->strings.end(),
                    value.begin(),
                    value.end());
                result_->strings.push_back('\0');
                string_offsets_.emplace(std::string{value}, offset);
                return offset;
            }

            [[nodiscard]] std::uint32_t add_source_group(
                std::string_view name) {

                const auto existing = source_group_indices_.find(
                    std::string{name});
                if (existing != source_group_indices_.end()) {
                    return existing->second;
                }

                const auto index = checked_u32(
                    result_->source_groups.size(),
                    "Source group index");
                result_->source_groups.push_back({add_string(name)});
                source_group_indices_.emplace(std::string{name}, index);
                source_group_names_.emplace_back(name);
                return index;
            }

            [[nodiscard]] static std::string source_group_name(
                const std::filesystem::path& authored_path) {

                bool next_is_group = false;
                for (const auto& component : authored_path) {
                    const auto value = component.generic_string();
                    if (next_is_group && !value.empty()) {
                        return value;
                    }
                    next_is_group = value == "elements";
                }
                for (const auto& component : authored_path) {
                    if (component.generic_string() == "cameras") {
                        return "Cameras";
                    }
                }
                return "Root";
            }

            void add_source_layer(
                std::string_view authored_path,
                const std::filesystem::path& real_path,
                std::string_view group_name) {

                StaticScene::SourceLayer layer;
                layer.name = add_string(
                    std::filesystem::path{authored_path}
                        .filename().generic_string());
                layer.path = add_string(authored_path);
                layer.group = add_source_group(group_name);

                const auto index = checked_u32(
                    result_->source_layers.size(),
                    "Source layer index");
                result_->source_layers.push_back(layer);
                source_layer_indices_.emplace(
                    normalized_texture_key(real_path),
                    index);
            }

            void build_source_provenance() {
                const auto root = stage_->GetRootLayer();
                if (!root) {
                    fail("OpenUSD stage has no root layer.");
                }

                const std::filesystem::path root_path{
                    root->GetRealPath()};
                add_source_layer(
                    root_path.filename().generic_string(),
                    root_path,
                    "Root");

                const auto root_directory = root_path.parent_path();
                for (const auto& authored : root->GetSubLayerPaths()) {
                    const auto authored_string =
                        static_cast<std::string>(authored);
                    const std::filesystem::path authored_path{
                        authored_string};
                    const auto real_path =
                        (root_directory / authored_path).lexically_normal();
                    add_source_layer(
                        authored_string,
                        real_path,
                        source_group_name(authored_path));
                }
            }

            [[nodiscard]] std::uint32_t source_layer_for_prim(
                const pxr::UsdPrim& prim) const {

                // A referenced asset's child prim stack contains the asset
                // layer, while the root USDA sublayer that owns the object is
                // authored on an ancestor. Use the closest authored owner.
                for (auto current = prim; current; current = current.GetParent()) {
                    for (const auto& spec : current.GetPrimStack()) {
                        if (!spec || !spec->GetLayer()) {
                            continue;
                        }
                        const auto& layer = spec->GetLayer();
                        std::filesystem::path path{layer->GetRealPath()};
                        if (path.empty()) {
                            path = layer->GetIdentifier();
                        }
                        const auto found = source_layer_indices_.find(
                            normalized_texture_key(path));
                        if (found != source_layer_indices_.end()) {
                            return found->second;
                        }
                    }
                }
                return StaticScene::INVALID_INDEX;
            }

            [[nodiscard]] ObjectKind source_layer_kind(
                std::uint32_t source_layer) const {

                if (source_layer >= result_->source_layers.size()) {
                    return ObjectKind::UNKNOWN;
                }
                const auto group =
                    result_->source_layers[source_layer].group;
                if (group >= source_group_names_.size()) {
                    return ObjectKind::UNKNOWN;
                }

                const auto& name = source_group_names_[group];
                static const std::unordered_map<std::string, ObjectKind>
                    kinds{
                        {"Pyramid", ObjectKind::PYRAMID},
                        {"River", ObjectKind::RIVER},
                        {"Creek", ObjectKind::CREEK},
                        {"Terrain", ObjectKind::TERRAIN},
                        {"Banyan", ObjectKind::BANYAN},
                        {"Anthurium", ObjectKind::ANTHURIUM},
                        {"Grass_A", ObjectKind::GRASS_A},
                        {"Grass_B", ObjectKind::GRASS_B},
                        {"Pyramid_Grass_B", ObjectKind::PYRAMID_GRASS_B},
                        {"Pyramid_Moss", ObjectKind::PYRAMID_MOSS},
                        {"QueenForest", ObjectKind::QUEEN_FOREST},
                        {"RiverForest", ObjectKind::RIVER_FOREST},
                        {"RiverSapling", ObjectKind::RIVER_SAPLING},
                        {"RiverSeedling", ObjectKind::RIVER_SEEDLING},
                        {"Shrub", ObjectKind::SHRUB},
                        {"ShrubSorrel", ObjectKind::SHRUB_SORREL},
                        {"Nettle", ObjectKind::NETTLE},
                    };
                const auto found = kinds.find(name);
                return found == kinds.end()
                    ? ObjectKind::UNKNOWN
                    : found->second;
            }

            [[nodiscard]] DirectX::XMMATRIX local_transform(
                const pxr::UsdPrim& prim,
                bool& resets_stack) const {

                pxr::GfMatrix4d local{1.0};
                resets_stack = false;
                const pxr::UsdGeomXformable xformable{prim};
                if (xformable) {
                    xformable.GetLocalTransformation(&local, &resets_stack);
                }
                return converter_.matrix(local);
            }

            [[nodiscard]] DirectX::XMMATRIX world_transform(
                const pxr::UsdPrim& prim) {

                return converter_.matrix(
                    xform_cache_.GetLocalToWorldTransform(prim));
            }

            void collect_point_instancers() {
                for (const auto& prim : stage_->Traverse()) {
                    if (!prim.IsA<pxr::UsdGeomPointInstancer>() ||
                        !is_visible(prim)) {
                        continue;
                    }

                    const pxr::UsdGeomPointInstancer instancer{prim};
                    pxr::SdfPathVector targets;
                    instancer.GetPrototypesRel().GetTargets(&targets);
                    if (targets.size() != 1) {
                        fail(
                            "Intel Jungle PointInstancer must have one prototype: ",
                            prim.GetPath().GetString());
                    }

                    point_instancers_.push_back(prim);
                    point_target_paths_.push_back(targets.front());
                }
            }

            [[nodiscard]] bool is_under_point_target(
                const pxr::SdfPath& path) const {

                for (const auto& root : point_target_paths_) {
                    if (path == root || path.HasPrefix(root)) {
                        return true;
                    }
                }
                return false;
            }

            void build_point_batches() {
                for (const auto& prim : point_instancers_) {
                    const pxr::UsdGeomPointInstancer source{prim};
                    pxr::SdfPathVector targets;
                    source.GetPrototypesRel().GetTargets(&targets);
                    const auto prototype_prim = stage_->GetPrimAtPath(
                        targets.front());
                    if (!prototype_prim) {
                        fail(
                            "Missing PointInstancer prototype: ",
                            targets.front().GetString());
                    }

                    const auto source_layer = source_layer_for_prim(prim);
                    if (source_layer == StaticScene::INVALID_INDEX) {
                        fail(
                            "PointInstancer has no root USDA source layer: ",
                            prim.GetPath().GetString());
                    }
                    const auto kind = source_layer_kind(source_layer);
                    const auto prototype = get_hierarchical_prototype(
                        prototype_prim,
                        kind,
                        "point:" + targets.front().GetString());

                    pxr::VtIntArray prototype_indices;
                    pxr::VtVec3fArray positions;
                    pxr::VtQuathArray orientations;
                    pxr::VtVec3fArray scales;
                    source.GetProtoIndicesAttr().Get(&prototype_indices);
                    source.GetPositionsAttr().Get(&positions);
                    source.GetOrientationsAttr().Get(&orientations);
                    source.GetScalesAttr().Get(&scales);

                    if (prototype_indices.size() != positions.size() ||
                        orientations.size() != positions.size() ||
                        scales.size() != positions.size()) {
                        fail(
                            "PointInstancer array sizes differ: ",
                            prim.GetPath().GetString());
                    }
                    if (std::ranges::any_of(
                        prototype_indices,
                        [](int value) { return value != 0; })) {
                        fail(
                            "PointInstancer uses a nonzero prototype index: ",
                            prim.GetPath().GetString());
                    }

                    pxr::VtInt64Array ids;
                    source.GetIdsAttr().Get(&ids);

                    std::unordered_set<std::int64_t> hidden_ids;
                    pxr::SdfInt64ListOp inactive_ids;
                    if (prim.GetMetadata(
                        pxr::UsdGeomTokens->inactiveIds,
                        &inactive_ids)) {
                        for (const auto id : inactive_ids.GetExplicitItems()) {
                            hidden_ids.insert(id);
                        }
                    }
                    pxr::VtInt64Array invisible_ids;
                    if (source.GetInvisibleIdsAttr().Get(&invisible_ids)) {
                        hidden_ids.insert(
                            invisible_ids.begin(),
                            invisible_ids.end());
                    }

                    StaticScene::PointBatch batch;
                    batch.name = add_string(prim.GetName().GetString());
                    batch.source_prim_path = add_string(
                        prim.GetPath().GetString());
                    batch.source_layer = source_layer;
                    batch.prototype = prototype;
                    batch.instance_offset = checked_u32(
                        result_->point_instances.size(),
                        "Point instance offset");
                    batch.local_to_world = store_matrix(world_transform(prim));

                    for (std::size_t index = 0; index < positions.size(); ++index) {
                        const std::int64_t id = ids.size() == positions.size()
                            ? ids[index]
                            : static_cast<std::int64_t>(index);
                        if (hidden_ids.contains(id)) {
                            continue;
                        }

                        result_->point_instances.push_back({
                            converter_.position(positions[index]),
                            converter_.orientation(orientations[index]),
                            converter_.scale(scales[index])
                        });
                    }

                    batch.instance_count = checked_u32(
                        result_->point_instances.size() -
                            batch.instance_offset,
                        "Point instance count");
                    result_->point_batches.push_back(batch);
                }
            }

            void build_direct_scene() {
                for (const auto& prim : stage_->Traverse()) {
                    if (prim.IsA<pxr::UsdGeomCamera>()) {
                        if (is_visible(prim)) {
                            import_camera(prim);
                        }
                        continue;
                    }
                    if (prim.IsA<pxr::UsdLuxDomeLight>()) {
                        if (is_visible(prim)) {
                            import_environment_light(prim);
                        }
                        continue;
                    }
                    if (prim.IsA<pxr::UsdGeomPointInstancer>()) {
                        continue;
                    }
                    if (is_under_point_target(prim.GetPath())) {
                        continue;
                    }

                    if (prim.IsInstance()) {
                        if (!is_visible(prim)) {
                            continue;
                        }
                        const auto native_prototype = prim.GetPrototype();
                        if (!native_prototype) {
                            continue;
                        }
                        const auto source_layer = source_layer_for_prim(prim);
                        if (source_layer == StaticScene::INVALID_INDEX) {
                            fail(
                                "Instance has no root USDA source layer: ",
                                prim.GetPath().GetString());
                        }
                        const auto kind = source_layer_kind(source_layer);
                        const auto prototype = get_hierarchical_prototype(
                            native_prototype,
                            kind,
                            "native:" +
                                native_prototype.GetPath().GetString() + ":" +
                                std::to_string(static_cast<std::uint32_t>(kind)));
                        append_matrix_instance(
                            prototype,
                            prim.GetName().GetString(),
                            prim.GetPath().GetString(),
                            source_layer,
                            world_transform(prim));
                        continue;
                    }

                    if (prim.IsA<pxr::UsdGeomMesh>() && is_visible(prim)) {
                        const auto source_layer = source_layer_for_prim(prim);
                        if (source_layer == StaticScene::INVALID_INDEX) {
                            fail(
                                "Mesh has no root USDA source layer: ",
                                prim.GetPath().GetString());
                        }
                        const auto kind = source_layer_kind(source_layer);
                        const auto prototype = get_direct_mesh_prototype(
                            prim,
                            kind);
                        append_matrix_instance(
                            prototype,
                            prim.GetName().GetString(),
                            prim.GetPath().GetString(),
                            source_layer,
                            world_transform(prim));
                    }
                }
            }

            [[nodiscard]] std::uint32_t get_hierarchical_prototype(
                const pxr::UsdPrim& root,
                ObjectKind kind,
                std::string key) {

                const auto cached = prototype_cache_.find(key);
                if (cached != prototype_cache_.end()) {
                    return cached->second;
                }

                StaticScene::Prototype prototype;
                prototype.name = add_string(root.GetName().GetString());
                prototype.object_kind = kind;
                prototype.part_offset = checked_u32(
                    result_->prototype_parts.size(),
                    "Prototype part offset");

                collect_prototype_parts(
                    root,
                    DirectX::XMMatrixIdentity(),
                    prototype,
                    0);

                prototype.part_count = checked_u32(
                    result_->prototype_parts.size() - prototype.part_offset,
                    "Prototype part count");
                if (prototype.part_count == 0) {
                    fail(
                        "Prototype has no mesh parts: ",
                        root.GetPath().GetString());
                }

                const auto index = checked_u32(
                    result_->prototypes.size(),
                    "Prototype index");
                result_->prototypes.push_back(prototype);
                prototype_cache_.emplace(std::move(key), index);
                return index;
            }

            void collect_prototype_parts(
                const pxr::UsdPrim& prim,
                DirectX::FXMMATRIX parent_transform,
                StaticScene::Prototype& prototype,
                std::size_t depth) {

                if (!prim || depth > MAX_PROTOTYPE_DEPTH) {
                    fail("Prototype nesting is invalid.");
                }

                bool resets_stack = false;
                const auto local = local_transform(prim, resets_stack);
                const auto current = resets_stack
                    ? local
                    : DirectX::XMMatrixMultiply(local, parent_transform);

                if (prim.IsInstance()) {
                    const auto native_prototype = prim.GetPrototype();
                    if (!native_prototype) {
                        fail(
                            "Prototype instance has no native prototype: ",
                            prim.GetPath().GetString());
                    }
                    collect_prototype_parts(
                        native_prototype,
                        current,
                        prototype,
                        depth + 1);
                    return;
                }

                if (prim.IsA<pxr::UsdGeomMesh>()) {
                    const auto mesh = cook_mesh(prim);
                    StaticScene::PrototypePart part;
                    part.mesh = mesh;
                    part.local_transform = store_matrix(current);
                    result_->prototype_parts.push_back(part);
                }

                for (const auto& child : prim.GetChildren()) {
                    collect_prototype_parts(
                        child,
                        current,
                        prototype,
                        depth + 1);
                }
            }

            [[nodiscard]] std::uint32_t get_direct_mesh_prototype(
                const pxr::UsdPrim& mesh_prim,
                ObjectKind kind) {

                const std::string key =
                    "direct:" + mesh_prim.GetPath().GetString();
                const auto cached = prototype_cache_.find(key);
                if (cached != prototype_cache_.end()) {
                    return cached->second;
                }

                const auto mesh = cook_mesh(mesh_prim);
                StaticScene::Prototype prototype;
                prototype.name = add_string(mesh_prim.GetName().GetString());
                prototype.object_kind = kind;
                prototype.part_offset = checked_u32(
                    result_->prototype_parts.size(),
                    "Prototype part offset");
                prototype.part_count = 1;

                result_->prototype_parts.push_back({
                    mesh,
                    StaticScene::IDENTITY_TRANSFORM
                });

                const auto index = checked_u32(
                    result_->prototypes.size(),
                    "Prototype index");
                result_->prototypes.push_back(prototype);
                prototype_cache_.emplace(key, index);
                return index;
            }

            void append_matrix_instance(
                std::uint32_t prototype,
                std::string_view name,
                std::string_view source_prim_path,
                std::uint32_t source_layer,
                DirectX::FXMMATRIX world) {

                PendingMatrixBatch pending;
                pending.name = add_string(name);
                pending.source_prim_path = add_string(source_prim_path);
                pending.source_layer = source_layer;
                pending.prototype = prototype;
                pending.instances.push_back({store_matrix(world)});
                pending_matrix_batches_.push_back(std::move(pending));
            }

            void flatten_matrix_batches() {
                for (auto& pending : pending_matrix_batches_) {
                    StaticScene::MatrixBatch batch;
                    batch.name = pending.name;
                    batch.source_prim_path = pending.source_prim_path;
                    batch.source_layer = pending.source_layer;
                    batch.prototype = pending.prototype;
                    batch.instance_offset = checked_u32(
                        result_->matrix_instances.size(),
                        "Matrix instance offset");
                    batch.instance_count = checked_u32(
                        pending.instances.size(),
                        "Matrix instance count");

                    result_->matrix_instances.insert(
                        result_->matrix_instances.end(),
                        pending.instances.begin(),
                        pending.instances.end());

                    result_->matrix_batches.push_back(batch);
                }
            }

            [[nodiscard]] std::uint32_t cook_mesh(
                const pxr::UsdPrim& prim) {

                const auto key = prim.GetPath().GetString();
                const auto cached = mesh_cache_.find(key);
                if (cached != mesh_cache_.end()) {
                    return cached->second;
                }

                const pxr::UsdGeomMesh mesh{prim};
                pxr::TfToken subdivision;
                mesh.GetSubdivisionSchemeAttr().Get(&subdivision);
                if (!subdivision.IsEmpty() &&
                    subdivision != pxr::UsdGeomTokens->none) {
                    fail("Subdivision mesh is not expected: ", key);
                }

                pxr::VtVec3fArray points;
                pxr::VtIntArray face_counts;
                pxr::VtIntArray face_indices;
                pxr::VtIntArray hole_indices;
                pxr::VtVec3fArray normals;
                mesh.GetPointsAttr().Get(&points);
                mesh.GetFaceVertexCountsAttr().Get(&face_counts);
                mesh.GetFaceVertexIndicesAttr().Get(&face_indices);
                mesh.GetHoleIndicesAttr().Get(&hole_indices);
                mesh.GetNormalsAttr().Get(&normals);

                std::vector<std::size_t> corner_offsets(face_counts.size());
                std::size_t corner_count = 0;
                for (std::size_t face = 0; face < face_counts.size(); ++face) {
                    corner_offsets[face] = corner_count;
                    corner_count += static_cast<std::size_t>(face_counts[face]);
                }
                if (corner_count != face_indices.size()) {
                    fail("Mesh face topology differs: ", key);
                }

                std::vector<std::uint8_t> holes(face_counts.size(), 0u);
                for (const int face : hole_indices) {
                    if (face >= 0 &&
                        static_cast<std::size_t>(face) < holes.size()) {
                        holes[static_cast<std::size_t>(face)] = 1u;
                    }
                }

                bool double_sided = false;
                mesh.GetDoubleSidedAttr().Get(&double_sided);
                pxr::TfToken orientation;
                mesh.GetOrientationAttr().Get(&orientation);
                const bool reverse_winding =
                    orientation == pxr::UsdGeomTokens->leftHanded;
                const auto normal_interpolation =
                    mesh.GetNormalsInterpolation();

                auto groups = build_face_groups(prim, face_counts.size());

                StaticScene::Mesh destination;
                destination.name = add_string(prim.GetName().GetString());
                destination.submesh_offset = checked_u32(
                    result_->submeshes.size(),
                    "Mesh submesh offset");

                for (const auto& group : groups) {
                    const auto material = get_material(
                        group.material_path,
                        group.material);
                    UvData uv;
                    if (material.has_textures) {
                        uv = read_uv_data(prim, material.uv_primvar);
                    }

                    StaticScene::Submesh submesh;
                    submesh.name = add_string(path_leaf(group.material_path));
                    submesh.vertex_offset = checked_u32(
                        result_->vertices.size(),
                        "Submesh vertex offset");
                    submesh.index_offset = checked_u32(
                        result_->indices.size(),
                        "Submesh index offset");
                    submesh.material = material.index;
                    submesh.flags = make_submesh_flags(
                        double_sided,
                        material.alpha_mode);
                    VertexIndexer vertex_indexer{
                        result_->vertices,
                        submesh.vertex_offset};

                    for (const auto face : group.faces) {
                        if (holes[face] != 0u) {
                            continue;
                        }
                        const auto count = static_cast<std::size_t>(
                            face_counts[face]);
                        if (count < 3) {
                            continue;
                        }

                        const auto first = corner_offsets[face];
                        for (std::size_t triangle_index = 1;
                            triangle_index + 1 < count;
                            ++triangle_index) {

                            std::array<std::size_t, 3> corners{
                                first,
                                first + triangle_index,
                                first + triangle_index + 1
                            };
                            if (reverse_winding) {
                                std::swap(corners[1], corners[2]);
                            }

                            std::array<StaticScene::Vertex, 3> triangle{};
                            for (std::size_t vertex = 0; vertex < 3; ++vertex) {
                                const auto corner = corners[vertex];
                                const int point_index = face_indices[corner];
                                if (point_index < 0 ||
                                    static_cast<std::size_t>(point_index) >=
                                        points.size()) {
                                    fail("Mesh point index is invalid: ", key);
                                }

                                triangle[vertex].position = converter_.position(
                                    points[static_cast<std::size_t>(point_index)]);
                                if (!normals.empty()) {
                                    const auto normal_index = interpolation_index(
                                        normal_interpolation,
                                        face,
                                        corner,
                                        static_cast<std::uint32_t>(point_index));
                                    if (normal_index >= normals.size()) {
                                        fail("Mesh normal index is invalid: ", key);
                                    }
                                    triangle[vertex].normal = converter_.direction(
                                        normals[normal_index]);
                                }
                                if (!uv.values.empty()) {
                                    const auto uv_index = interpolation_index(
                                        uv.interpolation,
                                        face,
                                        corner,
                                        static_cast<std::uint32_t>(point_index));
                                    if (uv_index >= uv.values.size()) {
                                        fail("Mesh UV index is invalid: ", key);
                                    }
                                    const auto value = uv.values[uv_index];
                                    triangle[vertex].uv = {
                                        value[0],
                                        1.0f - value[1]
                                    };
                                }
                            }

                            normalize_triangle_normals(triangle);
                            if (result_->info.vertex_count_before_indexing >
                                std::numeric_limits<std::uint64_t>::max() -
                                    triangle.size()) {
                                fail("Triangle vertex count exceeds uint64_t.");
                            }
                            result_->info.vertex_count_before_indexing +=
                                triangle.size();
                            for (const auto& vertex : triangle) {
                                const auto local_index =
                                    vertex_indexer.get_or_add(vertex);
                                result_->indices.push_back(local_index);
                            }
                        }
                    }

                    submesh.vertex_count = checked_u32(
                        result_->vertices.size() - submesh.vertex_offset,
                        "Submesh vertex count");
                    submesh.index_count = checked_u32(
                        result_->indices.size() - submesh.index_offset,
                        "Submesh index count");
                    if (submesh.index_count == 0) {
                        continue;
                    }

                    result_->submeshes.push_back(submesh);
                    ++destination.submesh_count;
                }

                if (destination.submesh_count == 0) {
                    fail("Mesh produced no triangles: ", key);
                }

                const auto index = checked_u32(
                    result_->meshes.size(),
                    "Mesh index");
                result_->meshes.push_back(destination);
                mesh_cache_.emplace(key, index);
                return index;
            }

            [[nodiscard]] std::vector<FaceGroup> build_face_groups(
                const pxr::UsdPrim& mesh_prim,
                std::size_t face_count) const {

                const pxr::UsdShadeMaterialBindingAPI mesh_binding{mesh_prim};
                const auto default_material =
                    mesh_binding.ComputeBoundMaterial();
                const std::string default_path = default_material
                    ? default_material.GetPath().GetString()
                    : std::string{};

                std::vector<std::string> face_materials(
                    face_count,
                    default_path);
                std::unordered_map<std::string, pxr::UsdShadeMaterial>
                    materials;
                materials.emplace(default_path, default_material);

                for (const auto& child : mesh_prim.GetChildren()) {
                    if (!child.IsA<pxr::UsdGeomSubset>()) {
                        continue;
                    }
                    const pxr::UsdGeomSubset subset{child};
                    pxr::TfToken element_type;
                    subset.GetElementTypeAttr().Get(&element_type);
                    if (!element_type.IsEmpty() &&
                        element_type != pxr::UsdGeomTokens->face) {
                        continue;
                    }

                    const pxr::UsdShadeMaterialBindingAPI binding{child};
                    const auto material = binding.ComputeBoundMaterial();
                    const std::string path = material
                        ? material.GetPath().GetString()
                        : default_path;
                    materials.emplace(path, material ? material : default_material);

                    pxr::VtIntArray indices;
                    subset.GetIndicesAttr().Get(&indices);
                    for (const int face : indices) {
                        if (face >= 0 &&
                            static_cast<std::size_t>(face) < face_count) {
                            face_materials[static_cast<std::size_t>(face)] = path;
                        }
                    }
                }

                std::vector<FaceGroup> result;
                std::unordered_map<std::string, std::size_t> group_indices;
                for (std::uint32_t face = 0; face < face_count; ++face) {
                    const auto& path = face_materials[face];
                    const auto [iterator, inserted] = group_indices.emplace(
                        path,
                        result.size());
                    if (inserted) {
                        result.push_back({path, materials[path], {}});
                    }
                    result[iterator->second].faces.push_back(face);
                }
                return result;
            }

            [[nodiscard]] UvData read_uv_data(
                const pxr::UsdPrim& mesh_prim,
                std::string_view name) const {

                const pxr::UsdGeomPrimvarsAPI primvars{mesh_prim};
                const auto primvar = primvars.GetPrimvar(
                    pxr::TfToken{std::string{name}});
                if (!primvar) {
                    fail(
                        "Missing UV primvar '",
                        std::string{name},
                        "' on ",
                        mesh_prim.GetPath().GetString());
                }

                UvData result;
                result.interpolation = primvar.GetInterpolation();
                if (!primvar.ComputeFlattened(&result.values)) {
                    fail(
                        "Unable to flatten UV primvar on ",
                        mesh_prim.GetPath().GetString());
                }
                return result;
            }

            [[nodiscard]] static std::size_t interpolation_index(
                const pxr::TfToken& interpolation,
                std::size_t face,
                std::size_t corner,
                std::uint32_t point) {

                if (interpolation.IsEmpty() ||
                    interpolation == pxr::UsdGeomTokens->constant) {
                    return 0;
                }
                if (interpolation == pxr::UsdGeomTokens->uniform) {
                    return face;
                }
                if (interpolation == pxr::UsdGeomTokens->vertex ||
                    interpolation == pxr::UsdGeomTokens->varying) {
                    return point;
                }
                if (interpolation == pxr::UsdGeomTokens->faceVarying) {
                    return corner;
                }
                fail("Unsupported mesh interpolation: ", interpolation.GetString());
            }

            static void normalize_triangle_normals(
                std::array<StaticScene::Vertex, 3>& triangle) {

                const auto p0 = DirectX::XMLoadFloat3(&triangle[0].position);
                const auto p1 = DirectX::XMLoadFloat3(&triangle[1].position);
                const auto p2 = DirectX::XMLoadFloat3(&triangle[2].position);
                const auto edge1 = DirectX::XMVectorSubtract(p1, p0);
                const auto edge2 = DirectX::XMVectorSubtract(p2, p0);

                auto face_normal = DirectX::XMVectorNegate(
                    DirectX::XMVector3Cross(edge1, edge2));
                if (DirectX::XMVectorGetX(
                    DirectX::XMVector3LengthSq(face_normal)) <=
                    VECTOR_EPSILON) {
                    face_normal = DirectX::XMVectorSet(
                        0.0f, 1.0f, 0.0f, 0.0f);
                }
                else {
                    face_normal = DirectX::XMVector3Normalize(face_normal);
                }

                for (auto& vertex : triangle) {
                    auto normal = DirectX::XMLoadFloat3(&vertex.normal);
                    if (DirectX::XMVectorGetX(
                        DirectX::XMVector3LengthSq(normal)) <=
                        VECTOR_EPSILON) {
                        normal = face_normal;
                    }
                    else {
                        normal = DirectX::XMVector3Normalize(normal);
                    }
                    DirectX::XMStoreFloat3(&vertex.normal, normal);
                }
            }

            [[nodiscard]] MaterialRecord get_material(
                const std::string& path,
                const pxr::UsdShadeMaterial& source) {

                const std::string key = path.empty() ? "__fallback__" : path;
                const auto cached = material_cache_.find(key);
                if (cached != material_cache_.end()) {
                    return cached->second;
                }

                StaticScene::Material material;
                material.name = add_string(
                    path.empty() ? "DefaultMaterial" : path_leaf(path));
                MaterialRecord record;

                if (source) {
                    const auto surface = find_surface_shader(source);
                    material.base_color = read_float3_as_color(
                        surface,
                        "diffuseColor",
                        {0.18f, 0.18f, 0.18f});
                    material.emissive = read_float3(
                        surface,
                        "emissiveColor",
                        {0.0f, 0.0f, 0.0f});
                    material.roughness = read_float(
                        surface,
                        "roughness",
                        0.5f);
                    material.metallic = read_float(
                        surface,
                        "metallic",
                        0.0f);
                    material.opacity = read_float(
                        surface,
                        "opacity",
                        1.0f);
                    material.opacity_threshold = read_float(
                        surface,
                        "opacityThreshold",
                        0.0f);

                    std::string uv_primvar;
                    bool has_opacity_texture = false;
                    if (resolve_material_texture(
                        surface,
                        "diffuseColor",
                        true,
                        material.texture_binding_base_color,
                        uv_primvar)) {
                        material.base_color = {1.0f, 1.0f, 1.0f, 1.0f};
                    }
                    resolve_material_texture(
                        surface,
                        "normal",
                        false,
                        material.texture_binding_normal,
                        uv_primvar);
                    resolve_material_texture(
                        surface,
                        "roughness",
                        false,
                        material.texture_binding_roughness,
                        uv_primvar);
                    has_opacity_texture = resolve_material_texture(
                        surface,
                        "opacity",
                        false,
                        material.texture_binding_opacity,
                        uv_primvar);
                    if (resolve_material_texture(
                        surface,
                        "emissiveColor",
                        true,
                        material.texture_binding_emissive,
                        uv_primvar)) {
                        material.emissive = {1.0f, 1.0f, 1.0f};
                    }

                    record.uv_primvar = uv_primvar.empty()
                        ? "st"
                        : std::move(uv_primvar);
                    record.has_textures =
                        material.texture_binding_base_color !=
                            StaticScene::INVALID_INDEX ||
                        material.texture_binding_normal !=
                            StaticScene::INVALID_INDEX ||
                        material.texture_binding_roughness !=
                            StaticScene::INVALID_INDEX ||
                        material.texture_binding_opacity !=
                            StaticScene::INVALID_INDEX ||
                        material.texture_binding_emissive !=
                            StaticScene::INVALID_INDEX;

                    if (material.opacity_threshold > 0.0f) {
                        record.alpha_mode = AlphaMode::Tested;
                    }
                    else if (material.opacity < 1.0f || has_opacity_texture) {
                        record.alpha_mode = AlphaMode::Blended;
                    }
                }

                record.index = checked_u32(
                    result_->materials.size(),
                    "Material index");
                result_->materials.push_back(material);
                material_cache_.emplace(key, record);
                return record;
            }

            [[nodiscard]] pxr::UsdShadeShader find_surface_shader(
                const pxr::UsdShadeMaterial& material) const {

                for (const auto& output : material.GetOutputs()) {
                    if (output.GetBaseName() != pxr::TfToken{"surface"}) {
                        continue;
                    }
                    pxr::SdfPathVector invalid_paths;
                    const auto sources = output.GetConnectedSources(&invalid_paths);
                    if (sources.empty()) {
                        continue;
                    }
                    const pxr::UsdShadeShader shader{
                        sources.front().source.GetPrim()};
                    pxr::TfToken shader_id;
                    if (shader && shader.GetShaderId(&shader_id) &&
                        shader_id == pxr::TfToken{"UsdPreviewSurface"}) {
                        return shader;
                    }
                }
                fail(
                    "Material has no UsdPreviewSurface: ",
                    material.GetPath().GetString());
            }

            [[nodiscard]] static pxr::UsdShadeInput find_input(
                const pxr::UsdShadeShader& shader,
                std::string_view name) {

                return shader.GetInput(pxr::TfToken{std::string{name}});
            }

            [[nodiscard]] static float read_float(
                const pxr::UsdShadeShader& shader,
                std::string_view name,
                float fallback) {

                const auto input = find_input(shader, name);
                float result = fallback;
                if (input) {
                    input.Get(&result);
                }
                return result;
            }

            [[nodiscard]] static DirectX::XMFLOAT3 read_float3(
                const pxr::UsdShadeShader& shader,
                std::string_view name,
                const DirectX::XMFLOAT3& fallback) {

                const auto input = find_input(shader, name);
                pxr::GfVec3f result{fallback.x, fallback.y, fallback.z};
                if (input) {
                    input.Get(&result);
                }
                return {result[0], result[1], result[2]};
            }

            [[nodiscard]] static DirectX::XMFLOAT4 read_float3_as_color(
                const pxr::UsdShadeShader& shader,
                std::string_view name,
                const DirectX::XMFLOAT3& fallback) {

                const auto value = read_float3(shader, name, fallback);
                return {value.x, value.y, value.z, 1.0f};
            }

            [[nodiscard]] bool resolve_material_texture(
                const pxr::UsdShadeShader& surface,
                std::string_view input_name,
                bool default_srgb,
                std::uint32_t& destination,
                std::string& uv_primvar) {

                const auto input = find_input(surface, input_name);
                if (!input) {
                    return false;
                }

                pxr::SdfPathVector invalid_paths;
                const auto sources = input.GetConnectedSources(&invalid_paths);
                if (sources.empty()) {
                    return false;
                }

                const auto resolved = resolve_texture_binding(
                    sources.front(),
                    default_srgb);
                if (uv_primvar.empty()) {
                    uv_primvar = resolved.uv_primvar;
                }
                else if (uv_primvar != resolved.uv_primvar) {
                    fail(
                        "Material uses multiple UV primvars: ",
                        surface.GetPath().GetString());
                }
                destination = add_texture_binding(resolved);
                return true;
            }

            [[nodiscard]] ResolvedTextureBinding resolve_texture_binding(
                const pxr::UsdShadeConnectionSourceInfo& connection,
                bool default_srgb) {

                const pxr::UsdShadeShader texture_shader{
                    connection.source.GetPrim()};
                pxr::TfToken shader_id;
                if (!texture_shader ||
                    !texture_shader.GetShaderId(&shader_id) ||
                    shader_id != pxr::TfToken{"UsdUVTexture"}) {
                    fail("Connected shader is not UsdUVTexture.");
                }

                const auto file_input = texture_shader.GetInput(
                    pxr::TfToken{"file"});
                pxr::SdfAssetPath asset;
                if (!file_input || !file_input.Get(&asset)) {
                    fail(
                        "UsdUVTexture has no file: ",
                        texture_shader.GetPath().GetString());
                }

                const std::string resolved_path = asset.GetResolvedPath();
                if (resolved_path.empty()) {
                    fail("Texture asset is unresolved: ", asset.GetAssetPath());
                }

                ResolvedTextureBinding result;
                result.path = std::filesystem::path{resolved_path};
                result.channel = texture_channel(
                    connection.sourceName.GetString());
                result.srgb = default_srgb;

                const auto color_space = read_token_or_string(
                    texture_shader.GetInput(pxr::TfToken{"sourceColorSpace"}));
                if (color_space == "sRGB") {
                    result.srgb = true;
                }
                else if (color_space == "raw") {
                    result.srgb = false;
                }

                result.sampler.address_u = texture_address_mode(
                    read_token_or_string(
                        texture_shader.GetInput(pxr::TfToken{"wrapS"})));
                result.sampler.address_v = texture_address_mode(
                    read_token_or_string(
                        texture_shader.GetInput(pxr::TfToken{"wrapT"})));

                const auto st = texture_shader.GetInput(pxr::TfToken{"st"});
                if (st) {
                    pxr::SdfPathVector invalid_paths;
                    const auto st_sources = st.GetConnectedSources(&invalid_paths);
                    if (!st_sources.empty()) {
                        const pxr::UsdShadeShader reader{
                            st_sources.front().source.GetPrim()};
                        pxr::TfToken reader_id;
                        if (!reader ||
                            !reader.GetShaderId(&reader_id) ||
                            reader_id !=
                                pxr::TfToken{"UsdPrimvarReader_float2"}) {
                            fail("Texture st source is not a float2 primvar reader.");
                        }
                        const auto varname = read_token_or_string(
                            reader.GetInput(pxr::TfToken{"varname"}));
                        if (!varname.empty()) {
                            result.uv_primvar = varname;
                        }
                    }
                }
                return result;
            }

            [[nodiscard]] static std::string read_token_or_string(
                const pxr::UsdShadeInput& input) {

                if (!input) {
                    return {};
                }
                pxr::VtValue value;
                if (!input.Get(&value) || value.IsEmpty()) {
                    return {};
                }
                if (value.IsHolding<pxr::TfToken>()) {
                    return value.UncheckedGet<pxr::TfToken>().GetString();
                }
                if (value.IsHolding<std::string>()) {
                    return value.UncheckedGet<std::string>();
                }
                return {};
            }

            [[nodiscard]] static AddressMode texture_address_mode(
                std::string_view value) {

                if (value.empty() || value == "repeat" ||
                    value == "useMetadata") {
                    return AddressMode::WRAP;
                }
                if (value == "mirror") {
                    return AddressMode::MIRROR;
                }
                if (value == "clamp") {
                    return AddressMode::CLAMP;
                }
                if (value == "black") {
                    return AddressMode::BORDER;
                }
                fail("Unsupported texture address mode: ", std::string{value});
            }

            [[nodiscard]] static TextureChannel texture_channel(
                std::string_view output) {

                if (output == "r") return TextureChannel::R;
                if (output == "g") return TextureChannel::G;
                if (output == "b") return TextureChannel::B;
                if (output == "a") return TextureChannel::A;
                return TextureChannel::RGBA;
            }

            [[nodiscard]] std::uint32_t add_texture_binding(
                const ResolvedTextureBinding& source) {

                StaticScene::TextureBinding binding;
                binding.texture = get_texture(source.path);
                binding.sampler = get_sampler(source.sampler);
                binding.channel = source.channel;
                binding.flags = source.srgb
                    ? BindingFlag::SRGB
                    : BindingFlag::LINEAR;

                const auto index = checked_u32(
                    result_->texture_bindings.size(),
                    "Texture binding index");
                result_->texture_bindings.push_back(binding);
                return index;
            }

            [[nodiscard]] std::uint32_t get_sampler(
                const SamplerKey& key) {

                const auto cached = sampler_cache_.find(key);
                if (cached != sampler_cache_.end()) {
                    return cached->second;
                }

                StaticScene::Sampler sampler;
                sampler.address_u = key.address_u;
                sampler.address_v = key.address_v;
                const auto index = checked_u32(
                    result_->samplers.size(),
                    "Sampler index");
                result_->samplers.push_back(sampler);
                sampler_cache_.emplace(key, index);
                return index;
            }

            [[nodiscard]] std::uint32_t get_texture(
                const std::filesystem::path& path) {

                const auto key = normalized_texture_key(path);
                const auto cached = texture_cache_.find(key);
                if (cached != texture_cache_.end()) {
                    return cached->second;
                }

                StaticScene::Texture texture;
                texture.name = add_string(path.filename().generic_string());
                const auto index = checked_u32(
                    result_->textures.size(),
                    "Texture index");
                result_->textures.push_back(texture);
                texture_paths_.push_back(path.generic_string());
                texture_cache_.emplace(key, index);
                return index;
            }

            [[nodiscard]] static SubmeshFlag make_submesh_flags(
                bool double_sided,
                AlphaMode alpha_mode) noexcept {

                std::uint32_t flags = double_sided
                    ? static_cast<std::uint32_t>(SubmeshFlag::DOUBLE_SIDED)
                    : 0u;
                if (alpha_mode == AlphaMode::Tested) {
                    flags |= static_cast<std::uint32_t>(
                        SubmeshFlag::ALPHA_TESTED);
                }
                else if (alpha_mode == AlphaMode::Blended) {
                    flags |= static_cast<std::uint32_t>(
                        SubmeshFlag::ALPHA_BLENDED);
                }
                return static_cast<SubmeshFlag>(flags);
            }

            void import_camera(const pxr::UsdPrim& prim) {
                const pxr::UsdGeomCamera source{prim};
                auto& camera = result_->camera;
                camera.name = add_string(prim.GetName().GetString());
                camera.world_transform = store_matrix(world_transform(prim));
                source.GetFocalLengthAttr().Get(&camera.focal_length);
                source.GetHorizontalApertureAttr().Get(
                    &camera.horizontal_aperture);
                source.GetVerticalApertureAttr().Get(
                    &camera.vertical_aperture);
                source.GetHorizontalApertureOffsetAttr().Get(
                    &camera.horizontal_aperture_offset);
                source.GetVerticalApertureOffsetAttr().Get(
                    &camera.vertical_aperture_offset);
                source.GetFocusDistanceAttr().Get(&camera.focus_distance);
                source.GetFStopAttr().Get(&camera.f_stop);
                pxr::GfVec2f clipping;
                if (source.GetClippingRangeAttr().Get(&clipping)) {
                    camera.clipping_range = {clipping[0], clipping[1]};
                }
            }

            void import_environment_light(const pxr::UsdPrim& prim) {
                const pxr::UsdLuxDomeLight dome{prim};
                const pxr::UsdLuxLightAPI light{prim};
                auto& destination = result_->environment_light;
                destination.name = add_string(prim.GetName().GetString());
                destination.world_transform = store_matrix(world_transform(prim));

                pxr::GfVec3f color{1.0f};
                light.GetColorAttr().Get(&color);
                destination.color = {color[0], color[1], color[2]};
                light.GetIntensityAttr().Get(&destination.intensity);
                light.GetExposureAttr().Get(&destination.exposure);

                pxr::SdfAssetPath texture;
                if (dome.GetTextureFileAttr().Get(&texture) &&
                    !texture.GetResolvedPath().empty()) {
                    destination.texture = get_texture(
                        std::filesystem::path{texture.GetResolvedPath()});
                }
            }

        private:
            pxr::UsdStageRefPtr stage_;
            CoordinateConverter converter_;
            pxr::UsdGeomXformCache xform_cache_;
            std::unique_ptr<StaticScene> result_;

            std::vector<pxr::UsdPrim> point_instancers_;
            std::vector<pxr::SdfPath> point_target_paths_;

            std::unordered_map<std::string, std::uint32_t> string_offsets_;
            std::unordered_map<std::string, std::uint32_t>
                source_group_indices_;
            std::vector<std::string> source_group_names_;
            std::unordered_map<std::string, std::uint32_t>
                source_layer_indices_;
            std::unordered_map<std::string, std::uint32_t> mesh_cache_;
            std::unordered_map<std::string, std::uint32_t> prototype_cache_;
            std::unordered_map<std::string, MaterialRecord> material_cache_;
            std::unordered_map<std::string, std::uint32_t> texture_cache_;
            std::vector<std::string> texture_paths_;
            std::unordered_map<SamplerKey, std::uint32_t, SamplerKeyHash>
                sampler_cache_;

            std::vector<PendingMatrixBatch> pending_matrix_batches_;
        };

    } // namespace

    StaticSceneBuild StaticSceneBuilder::build(
        const std::filesystem::path& root_layer) {

        register_openusd_plugins();

        const auto absolute = std::filesystem::absolute(root_layer);
        if (!std::filesystem::is_regular_file(absolute)) {
            fail(
                "Intel Jungle root layer does not exist: ",
                absolute.generic_string());
        }

        auto stage = pxr::UsdStage::Open(
            absolute.generic_string(),
            pxr::UsdStage::LoadAll);
        if (!stage) {
            fail("OpenUSD could not open: ", absolute.generic_string());
        }

        return Builder{std::move(stage)}.run();
    }

} // namespace fjr::cooker
