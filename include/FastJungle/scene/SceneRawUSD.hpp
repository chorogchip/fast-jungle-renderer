#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace fjr::scene::raw {

    enum RawUSDPrimState : uint32_t {
        RawUSDPrimStateActive = 1u << 0u,
        RawUSDPrimStateLoaded = 1u << 1u,
        RawUSDPrimStateDefined = 1u << 2u,
        RawUSDPrimStateAbstract = 1u << 3u,
        RawUSDPrimStateNativeInstance = 1u << 4u,
        RawUSDPrimStatePrototype = 1u << 5u,
        RawUSDPrimStatePointInstancer = 1u << 6u,
    };

    struct RawUSDLayerInfo {
        std::string identifier;
        std::string resolved_path;
        bool used_by_stage = false;
        bool is_root_layer = false;
    };

    struct RawUSDAssetInfo {
        std::string source_property_path;
        std::string authored_path;
        std::string resolved_path;
        std::string category;
        bool resolved = false;
    };

    struct RawUSDInventoryEntry {
        std::string relative_path;
        std::string category;
        uint64_t byte_count = 0;
        bool used_by_stage = false;
    };

    struct RawUSDDiagnostic {
        std::string severity;
        std::string message;
        std::string source_file;
        uint64_t source_line = 0;
    };

    struct RawUSDVariantSelection {
        std::string prim_path;
        std::string set_name;
        std::string selection;
        std::vector<std::string> variants;
    };

    struct RawUSDPrimInfo {
        std::string path;
        std::string type_name;
        std::string native_prototype_type;
        std::vector<std::string> applied_schemas;
        std::vector<std::string> authored_metadata_keys;
        std::vector<std::string> custom_data_keys;
        std::vector<std::string> prim_stack_layers;
        uint32_t state_flags = 0;
        uint64_t property_begin = 0;
        uint64_t property_count = 0;
    };

    struct RawUSDPropertyInfo {
        std::string path;
        std::string kind;
        std::string type_name;
        std::string resolved_value_source;
        std::vector<std::string> authored_metadata_keys;
        std::vector<std::string> custom_data_keys;
        std::vector<std::string> property_stack_layers;
        std::vector<std::string> connections_or_targets;
        uint64_t time_sample_count = 0;
        bool authored = false;
        bool has_value_clip = false;
    };

    struct RawUSDMaterialBindingInfo {
        std::string prim_path;
        std::string purpose;
        std::string binding_kind;
        std::string strength;
        std::string relationship_path;
        std::vector<std::string> targets;
    };

    struct RawUSDPointInstancerInfo {
        std::string prim_path;
        std::vector<std::string> prototype_targets;
        std::string proto_indices_property_path;
        uint64_t proto_indices_time_sample_count = 0;
        std::vector<std::string> instance_attribute_paths;
    };

    struct RawUSDStatistics {
        uint64_t prim_count = 0;
        uint64_t property_count = 0;
        uint64_t authored_property_count = 0;
        uint64_t attribute_count = 0;
        uint64_t relationship_count = 0;
        uint64_t property_time_sample_count = 0;
        uint64_t value_clip_property_count = 0;
        uint64_t native_instance_count = 0;
        uint64_t prototype_count = 0;
        uint64_t prototype_prim_count = 0;
        uint64_t prototype_property_count = 0;
        uint64_t point_instancer_count = 0;
        uint64_t mesh_count = 0;
        uint64_t primvar_count = 0;
        uint64_t material_count = 0;
        uint64_t shader_count = 0;
        uint64_t node_graph_count = 0;
        uint64_t camera_count = 0;
        uint64_t light_count = 0;
        uint64_t render_settings_count = 0;
        uint64_t unknown_schema_count = 0;
        std::map<std::string, uint64_t> prim_type_counts;
        std::map<std::string, uint64_t> prototype_type_counts;
        std::map<std::string, uint64_t> applied_schema_counts;
        std::map<std::string, uint64_t> shader_id_counts;
    };

    struct RawUSDStageInfo {
        std::string root_layer_identifier;
        std::string root_layer_resolved_path;
        std::string default_prim_path;
        std::string up_axis;
        double meters_per_unit = 0.0;
        double start_time_code = 0.0;
        double end_time_code = 0.0;
        double frames_per_second = 0.0;
        double time_codes_per_second = 0.0;
        std::string resolver_context;
        std::vector<std::string> load_rules;
        std::map<std::string, std::string> root_layer_metadata;
    };

    // Owns a LoadAll USD stage and records only metadata/index information.
    // Geometry, property values, time samples, materials, and layer specs stay
    // in OpenUSD and are queried from the retained raw document on demand.
    class SceneRawUSD {
    public:
#if 0
        static std::unique_ptr<SceneRawUSD> open(
            const std::filesystem::path& root_path);

        ~SceneRawUSD();
        SceneRawUSD(SceneRawUSD&&) noexcept;
        SceneRawUSD& operator=(SceneRawUSD&&) noexcept;
        SceneRawUSD(const SceneRawUSD&) = delete;
        SceneRawUSD& operator=(const SceneRawUSD&) = delete;


        const std::filesystem::path& root_path() const;
        const RawUSDStageInfo& stage_info() const;
        const RawUSDStatistics& statistics() const;
        const std::vector<RawUSDLayerInfo>& layers() const;
        const std::vector<RawUSDInventoryEntry>& inventory() const;
        const std::vector<RawUSDAssetInfo>& asset_references() const;
        const std::vector<RawUSDDiagnostic>& diagnostics() const;
        const std::vector<RawUSDVariantSelection>& variant_selections() const;
        const std::vector<RawUSDPrimInfo>& prims() const;
        const std::vector<RawUSDPropertyInfo>& properties() const;
        const std::vector<RawUSDMaterialBindingInfo>& material_bindings() const;
        const std::vector<RawUSDPointInstancerInfo>& point_instancers() const;

        void write_manifest(const std::filesystem::path& output_path) const;

    private:
        class Impl;
        friend class SceneRawUSDUsdAccess;

        explicit SceneRawUSD(std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> impl_;
#endif
    };

} // namespace scene::raw
