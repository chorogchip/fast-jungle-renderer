#if 0

#include "FastJungle/scene/SceneRawUSD.hpp"
#include "FastJungle/scene/SceneRawUSDUsdAccess.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "util/minmax_remover.h"

#include <pxr/base/js/json.h>
#include <pxr/base/arch/fileSystem.h>
#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/diagnosticMgr.h>
#include <pxr/base/tf/getenv.h>
#include <pxr/base/tf/stringUtils.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/ar/resolverContext.h>
#include <pxr/usd/ar/resolverContextBinder.h>
#include <pxr/usd/pcp/errors.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/property.h>
#include <pxr/usd/usd/relationship.h>
#include <pxr/usd/usd/resolveInfo.h>
#include <pxr/usd/usd/schemaRegistry.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/variantSets.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdLux/lightAPI.h>
#include <pxr/usd/usdRender/settings.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/nodeGraph.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdUtils/dependencies.h>

namespace scene::raw {

    namespace pxr = ::pxr;

    namespace {

        class DiagnosticCollector final : public pxr::TfDiagnosticMgr::Delegate {
        public:
            explicit DiagnosticCollector(std::vector<RawUSDDiagnostic>& diagnostics)
                : diagnostics_(diagnostics) {}

            void IssueStatus(const pxr::TfStatus& status) override {
                append_("status", status);
            }

            void IssueWarning(const pxr::TfWarning& warning) override {
                append_("warning", warning);
            }

            void IssueError(const pxr::TfError& error) override {
                append_("error", error);
            }

            void IssueFatalError(
                const pxr::TfCallContext& context,
                const std::string& message) override {
                std::fprintf(
                    stderr,
                    "OpenUSD fatal: %s (%s:%zu)\n",
                    message.c_str(),
                    context.GetFile() ? context.GetFile() : "",
                    static_cast<size_t>(context.GetLine()));
                std::fflush(stderr);
                diagnostics_.push_back({
                    "fatal",
                    message,
                    context.GetFile() ? context.GetFile() : "",
                    context.GetLine(),
                    });
                _UnhandledAbort();
            }

        private:
            template <typename T>
            void append_(const char* severity, const T& diagnostic) {
                diagnostics_.push_back({
                    severity,
                    diagnostic.GetCommentary(),
                    diagnostic.GetSourceFileName(),
                    diagnostic.GetSourceLineNumber(),
                    });
            }

            std::vector<RawUSDDiagnostic>& diagnostics_;
        };

        std::string path_string_(const std::filesystem::path& path) {
            return path.generic_string();
        }

        void register_openusd_plugins_() {
            std::vector<std::string> paths;
            const auto configured_paths = pxr::TfGetenv("PXR_PLUGINPATH_NAME");
            if (!configured_paths.empty()) {
                const auto environment_paths = pxr::TfStringSplit(
                    configured_paths,
                    ARCH_PATH_LIST_SEP);
                paths.insert(paths.end(), environment_paths.begin(), environment_paths.end());
            }

            // Monolithic static OpenUSD does not necessarily pull in its plugin
            // bootstrap constructor. Register the SDK's manifests explicitly.
            paths.emplace_back(TVBPERF_OPENUSD_PLUGIN_ROOT);
            paths.emplace_back(TVBPERF_OPENUSD_SHADER_PLUGIN_ROOT);

            std::set<std::string> registered_paths;
            auto& registry = pxr::PlugRegistry::GetInstance();
            for (const auto& path : paths) {
                if (path.empty() || !registered_paths.insert(path).second) {
                    continue;
                }
                registry.RegisterPlugins(path);
            }
        }

        std::string absolute_path_key_(const std::filesystem::path& path) {
            std::error_code error;
            const auto canonical = std::filesystem::weakly_canonical(path, error);
            const auto normalized = error
                ? std::filesystem::absolute(path, error).lexically_normal()
                : canonical.lexically_normal();
            return path_string_(normalized);
        }

        std::string inventory_category_(const std::filesystem::path& path) {
            auto extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
                });

            if (extension == ".usd" || extension == ".usda" || extension == ".usdc" ||
                extension == ".usdz") {
                return "usd";
            }
            if (extension == ".hdr" || extension == ".exr" || extension == ".png" ||
                extension == ".jpg" || extension == ".jpeg" || extension == ".tga" ||
                extension == ".dds" || extension == ".tif" || extension == ".tiff" ||
                extension == ".bmp") {
                return "texture";
            }
            return "other";
        }

        std::string resolve_source_name_(pxr::UsdResolveInfoSource source) {
            switch (source) {
            case pxr::UsdResolveInfoSourceNone:
                return "none";
            case pxr::UsdResolveInfoSourceFallback:
                return "fallback";
            case pxr::UsdResolveInfoSourceDefault:
                return "default";
            case pxr::UsdResolveInfoSourceTimeSamples:
                return "time_samples";
            case pxr::UsdResolveInfoSourceValueClips:
                return "value_clips";
            case pxr::UsdResolveInfoSourceSpline:
                return "spline";
            }
            return "unknown";
        }

        std::string load_rule_name_(pxr::UsdStageLoadRules::Rule rule) {
            switch (rule) {
            case pxr::UsdStageLoadRules::AllRule:
                return "all";
            case pxr::UsdStageLoadRules::OnlyRule:
                return "only";
            case pxr::UsdStageLoadRules::NoneRule:
                return "none";
            }
            return "unknown";
        }

        std::vector<std::string> token_names_(const pxr::TfTokenVector& values) {
            std::vector<std::string> result;
            result.reserve(values.size());
            for (const auto& value : values) {
                result.push_back(value.GetString());
            }
            return result;
        }

        std::vector<std::string> metadata_keys_(const pxr::UsdObject& object) {
            std::vector<std::string> keys;
            const auto metadata = object.GetAllAuthoredMetadata();
            keys.reserve(metadata.size());
            for (const auto& [key, value] : metadata) {
                (void)value;
                keys.push_back(key);
            }
            return keys;
        }

        std::vector<std::string> custom_data_keys_(const pxr::UsdObject& object) {
            std::vector<std::string> keys;
            const auto custom_data = object.GetCustomData();
            keys.reserve(custom_data.size());
            for (const auto& [key, value] : custom_data) {
                (void)value;
                keys.push_back(key);
            }
            return keys;
        }

        std::vector<std::string> prim_stack_layers_(const pxr::UsdPrim& prim) {
            std::vector<std::string> layers;
            const auto stack = prim.GetPrimStack();
            layers.reserve(stack.size());
            for (const auto& spec : stack) {
                if (spec && spec->GetLayer()) {
                    layers.push_back(spec->GetLayer()->GetIdentifier());
                }
            }
            return layers;
        }

        std::vector<std::string> property_stack_layers_(const pxr::UsdProperty& property) {
            std::vector<std::string> layers;
            const auto stack = property.GetPropertyStack();
            layers.reserve(stack.size());
            for (const auto& spec : stack) {
                if (spec && spec->GetLayer()) {
                    layers.push_back(spec->GetLayer()->GetIdentifier());
                }
            }
            return layers;
        }

        std::vector<std::string> path_values_(const pxr::SdfPathVector& paths) {
            std::vector<std::string> result;
            result.reserve(paths.size());
            for (const auto& path : paths) {
                result.push_back(path.GetString());
            }
            return result;
        }

        std::string resolved_asset_path_(const pxr::SdfAssetPath& asset_path) {
            if (!asset_path.GetResolvedPath().empty()) {
                return asset_path.GetResolvedPath();
            }
            if (asset_path.GetAssetPath().empty()) {
                return {};
            }
            return pxr::ArGetResolver().Resolve(asset_path.GetAssetPath()).GetPathString();
        }

        bool is_runtime_prototype_(const pxr::UsdPrim& prim) {
            return prim.IsInPrototype();
        }

        uint32_t prim_state_flags_(const pxr::UsdPrim& prim, bool runtime_prototype) {
            uint32_t flags = 0;
            if (prim.IsActive()) {
                flags |= RawUSDPrimStateActive;
            }
            if (prim.IsLoaded()) {
                flags |= RawUSDPrimStateLoaded;
            }
            if (prim.IsDefined()) {
                flags |= RawUSDPrimStateDefined;
            }
            if (prim.IsAbstract()) {
                flags |= RawUSDPrimStateAbstract;
            }
            if (prim.IsInstance()) {
                flags |= RawUSDPrimStateNativeInstance;
            }
            if (runtime_prototype) {
                flags |= RawUSDPrimStatePrototype;
            }
            if (prim.IsA<pxr::UsdGeomPointInstancer>()) {
                flags |= RawUSDPrimStatePointInstancer;
            }
            return flags;
        }

        void write_string_array_(pxr::JsWriter& writer, const std::vector<std::string>& values) {
            writer.BeginArray();
            for (const auto& value : values) {
                writer.WriteValue(value);
            }
            writer.EndArray();
        }

        void write_string_count_map_(
            pxr::JsWriter& writer,
            const std::map<std::string, uint64_t>& values) {
            writer.BeginObject();
            for (const auto& [key, value] : values) {
                writer.WriteKeyValue(key, value);
            }
            writer.EndObject();
        }

    } // namespace

    class SceneRawUSD::Impl {
    public:
        std::filesystem::path root_path;
        pxr::UsdStageRefPtr stage;
        pxr::SdfLayerHandle root_layer;
        pxr::SdfLayerHandleVector used_layers;
        pxr::ArResolverContext resolver_context;
        pxr::UsdStageLoadRules load_rules;
        RawUSDStageInfo stage_info;
        RawUSDStatistics statistics;
        std::vector<RawUSDLayerInfo> layers;
        std::vector<RawUSDInventoryEntry> inventory;
        std::vector<RawUSDAssetInfo> asset_references;
        std::vector<RawUSDDiagnostic> diagnostics;
        std::vector<RawUSDVariantSelection> variant_selections;
        std::vector<RawUSDPrimInfo> prims;
        std::vector<RawUSDPropertyInfo> properties;
        std::vector<RawUSDMaterialBindingInfo> material_bindings;
        std::vector<RawUSDPointInstancerInfo> point_instancers;

        void append_asset_(
            const std::string& property_path,
            const pxr::SdfAssetPath& asset_path,
            const char* category,
            std::set<std::string>& used_path_keys) {
            const auto resolved_path = resolved_asset_path_(asset_path);
            asset_references.push_back({
                property_path,
                asset_path.GetAssetPath(),
                resolved_path,
                category,
                !resolved_path.empty(),
                });
            if (!resolved_path.empty()) {
                used_path_keys.insert(absolute_path_key_(resolved_path));
            }
        }

        void inspect_attribute_assets_(
            const pxr::UsdAttribute& attribute,
            std::set<std::string>& used_path_keys) {
            const auto type_name = attribute.GetTypeName();
            if (type_name.GetScalarType() != pxr::SdfValueTypeNames->Asset) {
                return;
            }

            if (type_name.IsArray()) {
                pxr::VtArray<pxr::SdfAssetPath> assets;
                if (attribute.Get(&assets)) {
                    for (const auto& asset : assets) {
                        append_asset_(attribute.GetPath().GetString(), asset, "attribute_asset", used_path_keys);
                    }
                }
                return;
            }

            pxr::SdfAssetPath asset;
            if (attribute.Get(&asset)) {
                append_asset_(attribute.GetPath().GetString(), asset, "attribute_asset", used_path_keys);
            }
        }

        void inspect_property_(
            const pxr::UsdProperty& property,
            std::set<std::string>& used_path_keys,
            bool retain_index) {
            RawUSDPropertyInfo info;
            info.path = property.GetPath().GetString();
            info.authored = property.IsAuthored();
            info.authored_metadata_keys = metadata_keys_(property);
            info.custom_data_keys = custom_data_keys_(property);
            info.property_stack_layers = property_stack_layers_(property);

            if (const auto attribute = property.As<pxr::UsdAttribute>()) {
                info.kind = "attribute";
                info.type_name = attribute.GetTypeName().GetAsToken().GetString();
                const auto resolve_info = attribute.GetResolveInfo();
                info.resolved_value_source = resolve_source_name_(resolve_info.GetSource());
                info.time_sample_count = attribute.GetNumTimeSamples();
                info.has_value_clip = resolve_info.GetSource() == pxr::UsdResolveInfoSourceValueClips;

                pxr::SdfPathVector connections;
                attribute.GetConnections(&connections);
                info.connections_or_targets = path_values_(connections);

                inspect_attribute_assets_(attribute, used_path_keys);
                ++statistics.attribute_count;
                statistics.property_time_sample_count += info.time_sample_count;
                if (info.has_value_clip) {
                    ++statistics.value_clip_property_count;
                }
                if (attribute.GetTypeName().GetType().IsUnknown()) {
                    ++statistics.unknown_schema_count;
                }
            } else if (const auto relationship = property.As<pxr::UsdRelationship>()) {
                info.kind = "relationship";
                info.resolved_value_source = "targets";
                pxr::SdfPathVector targets;
                relationship.GetTargets(&targets);
                info.connections_or_targets = path_values_(targets);
                ++statistics.relationship_count;
            } else {
                info.kind = "unknown";
                ++statistics.unknown_schema_count;
            }

            ++statistics.property_count;
            if (info.authored) {
                ++statistics.authored_property_count;
            }
            if (retain_index) {
                properties.push_back(std::move(info));
            }
        }

        void inspect_material_bindings_(const pxr::UsdPrim& prim) {
            if (!prim.HasAPI<pxr::UsdShadeMaterialBindingAPI>()) {
                return;
            }

            const pxr::UsdShadeMaterialBindingAPI binding_api(prim);
            for (const auto& purpose : pxr::UsdShadeMaterialBindingAPI::GetMaterialPurposes()) {
                const auto direct = binding_api.GetDirectBindingRel(purpose);
                if (direct && direct.HasAuthoredTargets()) {
                    pxr::SdfPathVector targets;
                    direct.GetTargets(&targets);
                    material_bindings.push_back({
                        prim.GetPath().GetString(),
                        purpose.GetString(),
                        "direct",
                        pxr::UsdShadeMaterialBindingAPI::GetMaterialBindingStrength(direct).GetString(),
                        direct.GetPath().GetString(),
                        path_values_(targets),
                        });
                }

                for (const auto& collection : binding_api.GetCollectionBindingRels(purpose)) {
                    pxr::SdfPathVector targets;
                    collection.GetTargets(&targets);
                    material_bindings.push_back({
                        prim.GetPath().GetString(),
                        purpose.GetString(),
                        "collection",
                        pxr::UsdShadeMaterialBindingAPI::GetMaterialBindingStrength(collection).GetString(),
                        collection.GetPath().GetString(),
                        path_values_(targets),
                        });
                }
            }
        }

        void inspect_variants_(const pxr::UsdPrim& prim) {
            std::vector<std::string> names;
            const auto sets = prim.GetVariantSets();
            sets.GetNames(&names);
            for (const auto& name : names) {
                const auto set = sets.GetVariantSet(name);
                variant_selections.push_back({
                    prim.GetPath().GetString(),
                    name,
                    set.GetVariantSelection(),
                    set.GetVariantNames(),
                    });
            }
        }

        void inspect_point_instancer_(const pxr::UsdGeomPointInstancer& point_instancer) {
            RawUSDPointInstancerInfo info;
            info.prim_path = point_instancer.GetPath().GetString();

            const auto prototypes = point_instancer.GetPrototypesRel();
            pxr::SdfPathVector prototype_targets;
            prototypes.GetTargets(&prototype_targets);
            info.prototype_targets = path_values_(prototype_targets);

            const auto proto_indices = point_instancer.GetProtoIndicesAttr();
            if (proto_indices) {
                info.proto_indices_property_path = proto_indices.GetPath().GetString();
                info.proto_indices_time_sample_count = proto_indices.GetNumTimeSamples();
            }

            const std::array instance_attributes{
                point_instancer.GetIdsAttr(),
                point_instancer.GetPositionsAttr(),
                point_instancer.GetOrientationsAttr(),
                point_instancer.GetScalesAttr(),
                point_instancer.GetVelocitiesAttr(),
                point_instancer.GetAccelerationsAttr(),
                point_instancer.GetAngularVelocitiesAttr(),
                point_instancer.GetInvisibleIdsAttr(),
            };
            for (const auto& attribute : instance_attributes) {
                if (attribute) {
                    info.instance_attribute_paths.push_back(attribute.GetPath().GetString());
                }
            }
            point_instancers.push_back(std::move(info));
        }

        void inspect_prim_(
            const pxr::UsdPrim& prim,
            std::set<std::string>& used_path_keys,
            bool retain_index,
            bool runtime_prototype) {
            const auto type_name = prim.GetTypeName().GetString();
            if (runtime_prototype) {
                ++statistics.prototype_prim_count;
                ++statistics.prototype_type_counts[type_name.empty() ? "<untyped>" : type_name];
                statistics.prototype_property_count += prim.GetProperties().size();
            } else {
                ++statistics.prim_count;
                ++statistics.prim_type_counts[type_name.empty() ? "<untyped>" : type_name];
            }

            const auto schema_type = pxr::UsdSchemaRegistry::GetTypeFromSchemaTypeName(prim.GetTypeName());
            if (!type_name.empty() && schema_type.IsUnknown()) {
                ++statistics.unknown_schema_count;
            }

            std::string native_prototype_type;
            if (prim.IsInstance()) {
                ++statistics.native_instance_count;
                native_prototype_type = prim.GetPrototype().GetTypeName().GetString();
            }
            if (prim.IsA<pxr::UsdGeomPointInstancer>()) {
                ++statistics.point_instancer_count;
                if (retain_index) {
                    inspect_point_instancer_(pxr::UsdGeomPointInstancer(prim));
                }
            }
            if (prim.IsA<pxr::UsdGeomMesh>()) {
                ++statistics.mesh_count;
                statistics.primvar_count += pxr::UsdGeomPrimvarsAPI(prim).GetPrimvars().size();
            }
            if (prim.IsA<pxr::UsdShadeMaterial>()) {
                ++statistics.material_count;
            }
            if (prim.IsA<pxr::UsdShadeShader>()) {
                ++statistics.shader_count;
                pxr::TfToken shader_id;
                if (pxr::UsdShadeShader(prim).GetShaderId(&shader_id)) {
                    ++statistics.shader_id_counts[shader_id.GetString()];
                }
            }
            if (prim.IsA<pxr::UsdShadeNodeGraph>()) {
                ++statistics.node_graph_count;
            }
            if (prim.IsA<pxr::UsdGeomCamera>()) {
                ++statistics.camera_count;
            }
            if (prim.HasAPI<pxr::UsdLuxLightAPI>()) {
                ++statistics.light_count;
            }
            if (prim.IsA<pxr::UsdRenderSettings>()) {
                ++statistics.render_settings_count;
            }

            for (const auto& schema : prim.GetAppliedSchemas()) {
                ++statistics.applied_schema_counts[schema.GetString()];
                const auto [schema_name, instance_name] =
                    pxr::UsdSchemaRegistry::GetTypeNameAndInstance(schema);
                (void)instance_name;
                if (pxr::UsdSchemaRegistry::GetTypeFromSchemaTypeName(schema_name).IsUnknown()) {
                    ++statistics.unknown_schema_count;
                }
            }

            if (!retain_index) {
                return;
            }

            const auto property_begin = properties.size();
            for (const auto& property : prim.GetProperties()) {
                inspect_property_(property, used_path_keys, retain_index);
            }
            inspect_material_bindings_(prim);
            inspect_variants_(prim);
            prims.push_back({
                prim.GetPath().GetString(),
                type_name,
                native_prototype_type,
                token_names_(prim.GetAppliedSchemas()),
                metadata_keys_(prim),
                custom_data_keys_(prim),
                prim_stack_layers_(prim),
                prim_state_flags_(prim, runtime_prototype),
                property_begin,
                properties.size() - property_begin,
                });
        }

        void inspect_stage_() {
            auto& resolver = pxr::ArGetResolver();
            (void)resolver;
            pxr::ArResolverContextBinder resolver_context_binder(resolver_context);
            std::set<std::string> used_path_keys;

            const auto& root_layer = this->root_layer;
            stage_info.root_layer_identifier = root_layer->GetIdentifier();
            stage_info.root_layer_resolved_path = root_layer->GetRealPath();
            stage_info.default_prim_path = stage->GetDefaultPrim().GetPath().GetString();
            stage_info.up_axis = pxr::UsdGeomGetStageUpAxis(stage).GetString();
            stage_info.meters_per_unit = pxr::UsdGeomGetStageMetersPerUnit(stage);
            stage_info.start_time_code = stage->GetStartTimeCode();
            stage_info.end_time_code = stage->GetEndTimeCode();
            stage_info.frames_per_second = stage->GetFramesPerSecond();
            stage_info.time_codes_per_second = stage->GetTimeCodesPerSecond();
            stage_info.resolver_context = resolver_context.GetDebugString();

            for (const auto& [path, rule] : load_rules.GetRules()) {
                stage_info.load_rules.push_back(path.GetString() + ":" + load_rule_name_(rule));
            }
            const auto pseudo_root = root_layer->GetPseudoRoot();
            if (pseudo_root) {
                for (const auto& key : pseudo_root->ListFields()) {
                    stage_info.root_layer_metadata.emplace(
                        key.GetString(),
                        pxr::TfStringify(pseudo_root->GetField(key)));
                }
            } else {
                diagnostics.push_back({
                    "warning",
                    "Root layer pseudo-root SdfSpec is unavailable; root authored metadata was not indexed.",
                    root_layer->GetIdentifier(),
                    0,
                    });
            }

            std::set<std::string> seen_layers;
            for (const auto& layer : used_layers) {
                const auto real_path = layer->GetRealPath();
                layers.push_back({
                    layer->GetIdentifier(),
                    real_path,
                    true,
                    layer->GetIdentifier() == root_layer->GetIdentifier(),
                    });
                if (!real_path.empty()) {
                    used_path_keys.insert(absolute_path_key_(real_path));
                }
                seen_layers.insert(layer->GetIdentifier());
            }

            std::vector<pxr::SdfLayerRefPtr> dependency_layers;
            std::vector<std::string> dependency_assets;
            std::vector<std::string> unresolved_paths;
            const auto dependency_success = pxr::UsdUtilsComputeAllDependencies(
                pxr::SdfAssetPath(stage_info.root_layer_resolved_path),
                &dependency_layers,
                &dependency_assets,
                &unresolved_paths);
            if (!dependency_success) {
                diagnostics.push_back({ "error", "UsdUtilsComputeAllDependencies failed", {}, 0 });
            }
            for (const auto& layer : dependency_layers) {
                if (!layer) {
                    diagnostics.push_back({ "warning", "Dependency traversal returned an invalid SdfLayer.", {}, 0 });
                    continue;
                }
                const auto real_path = layer->GetRealPath();
                if (seen_layers.insert(layer->GetIdentifier()).second) {
                    layers.push_back({
                        layer->GetIdentifier(),
                        real_path,
                        false,
                        layer->GetIdentifier() == root_layer->GetIdentifier(),
                        });
                }
                asset_references.push_back({
                    {},
                    layer->GetIdentifier(),
                    real_path,
                    "dependency_layer",
                    !real_path.empty(),
                    });
                if (!real_path.empty()) {
                    used_path_keys.insert(absolute_path_key_(real_path));
                }
            }
            for (const auto& asset : dependency_assets) {
                asset_references.push_back({ {}, asset, asset, "dependency_asset", true });
                used_path_keys.insert(absolute_path_key_(asset));
            }
            for (const auto& unresolved : unresolved_paths) {
                asset_references.push_back({ {}, unresolved, {}, "unresolved_dependency", false });
                diagnostics.push_back({ "unresolved_asset", unresolved, {}, 0 });
            }

            for (const auto& composition_error : stage->GetCompositionErrors()) {
                diagnostics.push_back({ "composition_error", composition_error->ToString(), {}, 0 });
            }

            for (const auto& prim : stage->TraverseAll()) {
                inspect_prim_(prim, used_path_keys, true, false);
            }
            used_layers = stage->GetUsedLayers(true);
            for (const auto& layer : used_layers) {
                if (!seen_layers.insert(layer->GetIdentifier()).second) {
                    continue;
                }
                const auto real_path = layer->GetRealPath();
                layers.push_back({
                    layer->GetIdentifier(),
                    real_path,
                    true,
                    layer->GetIdentifier() == root_layer->GetIdentifier(),
                    });
                if (!real_path.empty()) {
                    used_path_keys.insert(absolute_path_key_(real_path));
                }
            }
            const auto prototypes = stage->GetPrototypes();
            statistics.prototype_count = prototypes.size();
            for (const auto& prototype : prototypes) {
                for (const auto& prototype_prim : pxr::UsdPrimRange::AllPrims(prototype)) {
                    inspect_prim_(prototype_prim, used_path_keys, false, true);
                }
            }

            const auto source_directory = root_path.parent_path();
            for (const auto& entry : std::filesystem::recursive_directory_iterator(source_directory)) {
                if (!entry.is_regular_file()) {
                    continue;
                }
                std::error_code relative_error;
                const auto relative = std::filesystem::relative(entry.path(), source_directory, relative_error);
                const auto byte_count = entry.file_size();
                inventory.push_back({
                    path_string_(relative_error ? entry.path().filename() : relative),
                    inventory_category_(entry.path()),
                    byte_count,
                    used_path_keys.contains(absolute_path_key_(entry.path())),
                    });
            }
            std::sort(inventory.begin(), inventory.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.relative_path < rhs.relative_path;
                });
        }
    };

    SceneRawUSD::SceneRawUSD(std::unique_ptr<Impl> impl)
        : impl_(std::move(impl)) {}

    SceneRawUSD::~SceneRawUSD() = default;
    SceneRawUSD::SceneRawUSD(SceneRawUSD&&) noexcept = default;
    SceneRawUSD& SceneRawUSD::operator=(SceneRawUSD&&) noexcept = default;

    std::unique_ptr<SceneRawUSD> SceneRawUSD::open(const std::filesystem::path& root_path) {
        const auto absolute_root = std::filesystem::absolute(root_path).lexically_normal();
        if (!std::filesystem::is_regular_file(absolute_root)) {
            throw std::runtime_error("Jungle USD root layer is not a file: " + path_string_(absolute_root));
        }

        auto impl = std::make_unique<Impl>();
        impl->root_path = absolute_root;
        DiagnosticCollector diagnostics(impl->diagnostics);
        auto& diagnostic_manager = pxr::TfDiagnosticMgr::GetInstance();
        diagnostic_manager.AddDelegate(&diagnostics);
        register_openusd_plugins_();
        impl->stage = pxr::UsdStage::Open(path_string_(absolute_root), pxr::UsdStage::LoadAll);

        if (!impl->stage) {
            diagnostic_manager.RemoveDelegate(&diagnostics);
            throw std::runtime_error("OpenUSD could not compose Jungle root layer: " + path_string_(absolute_root));
        }
        impl->root_layer = impl->stage->GetRootLayer();
        impl->used_layers = impl->stage->GetUsedLayers(true);
        impl->resolver_context = impl->stage->GetPathResolverContext();
        impl->load_rules = impl->stage->GetLoadRules();

        auto scene = std::unique_ptr<SceneRawUSD>(new SceneRawUSD(std::move(impl)));
        try {
            scene->impl_->inspect_stage_();
        } catch (...) {
            diagnostic_manager.RemoveDelegate(&diagnostics);
            throw;
        }
        diagnostic_manager.RemoveDelegate(&diagnostics);
        return scene;
    }

    const std::filesystem::path& SceneRawUSD::root_path() const {
        return impl_->root_path;
    }

    const RawUSDStageInfo& SceneRawUSD::stage_info() const {
        return impl_->stage_info;
    }

    const RawUSDStatistics& SceneRawUSD::statistics() const {
        return impl_->statistics;
    }

    const std::vector<RawUSDLayerInfo>& SceneRawUSD::layers() const {
        return impl_->layers;
    }

    const std::vector<RawUSDInventoryEntry>& SceneRawUSD::inventory() const {
        return impl_->inventory;
    }

    const std::vector<RawUSDAssetInfo>& SceneRawUSD::asset_references() const {
        return impl_->asset_references;
    }

    const std::vector<RawUSDDiagnostic>& SceneRawUSD::diagnostics() const {
        return impl_->diagnostics;
    }

    const std::vector<RawUSDVariantSelection>& SceneRawUSD::variant_selections() const {
        return impl_->variant_selections;
    }

    const std::vector<RawUSDPrimInfo>& SceneRawUSD::prims() const {
        return impl_->prims;
    }

    const std::vector<RawUSDPropertyInfo>& SceneRawUSD::properties() const {
        return impl_->properties;
    }

    const std::vector<RawUSDMaterialBindingInfo>& SceneRawUSD::material_bindings() const {
        return impl_->material_bindings;
    }

    const std::vector<RawUSDPointInstancerInfo>& SceneRawUSD::point_instancers() const {
        return impl_->point_instancers;
    }

    const pxr::UsdStageRefPtr& SceneRawUSDUsdAccess::stage(const SceneRawUSD& scene) {
        return scene.impl_->stage;
    }

    const pxr::SdfLayerHandle& SceneRawUSDUsdAccess::root_layer(const SceneRawUSD& scene) {
        return scene.impl_->root_layer;
    }

    const pxr::SdfLayerHandleVector& SceneRawUSDUsdAccess::used_layers(const SceneRawUSD& scene) {
        return scene.impl_->used_layers;
    }

    const pxr::ArResolverContext& SceneRawUSDUsdAccess::resolver_context(const SceneRawUSD& scene) {
        return scene.impl_->resolver_context;
    }

    const pxr::UsdStageLoadRules& SceneRawUSDUsdAccess::load_rules(const SceneRawUSD& scene) {
        return scene.impl_->load_rules;
    }

    void SceneRawUSD::write_manifest(const std::filesystem::path& output_path) const {
        if (!output_path.parent_path().empty()) {
            std::filesystem::create_directories(output_path.parent_path());
        }
        std::ofstream output(output_path, std::ios::out | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("Could not write Jungle raw manifest: " + path_string_(output_path));
        }

        pxr::JsWriter writer(output, pxr::JsWriter::Style::Pretty);
        writer.BeginObject();
        writer.WriteKeyValue("format", "SceneRawUSDManifest/v1");
        writer.WriteKeyValue("root_path", path_string_(root_path()));

        writer.WriteKey("stage");
        writer.BeginObject();
        const auto& stage = stage_info();
        writer.WriteKeyValue("root_layer_identifier", stage.root_layer_identifier);
        writer.WriteKeyValue("root_layer_resolved_path", stage.root_layer_resolved_path);
        writer.WriteKeyValue("default_prim_path", stage.default_prim_path);
        writer.WriteKeyValue("up_axis", stage.up_axis);
        writer.WriteKeyValue("meters_per_unit", stage.meters_per_unit);
        writer.WriteKeyValue("start_time_code", stage.start_time_code);
        writer.WriteKeyValue("end_time_code", stage.end_time_code);
        writer.WriteKeyValue("frames_per_second", stage.frames_per_second);
        writer.WriteKeyValue("time_codes_per_second", stage.time_codes_per_second);
        writer.WriteKeyValue("resolver_context", stage.resolver_context);
        writer.WriteKey("load_rules");
        write_string_array_(writer, stage.load_rules);
        writer.WriteKey("root_layer_metadata");
        writer.BeginObject();
        for (const auto& [key, value] : stage.root_layer_metadata) {
            writer.WriteKeyValue(key, value);
        }
        writer.EndObject();
        writer.EndObject();

        writer.WriteKey("statistics");
        writer.BeginObject();
        const auto& stats = statistics();
        writer.WriteKeyValue("prim_count", stats.prim_count);
        writer.WriteKeyValue("property_count", stats.property_count);
        writer.WriteKeyValue("authored_property_count", stats.authored_property_count);
        writer.WriteKeyValue("attribute_count", stats.attribute_count);
        writer.WriteKeyValue("relationship_count", stats.relationship_count);
        writer.WriteKeyValue("property_time_sample_count", stats.property_time_sample_count);
        writer.WriteKeyValue("value_clip_property_count", stats.value_clip_property_count);
        writer.WriteKeyValue("native_instance_count", stats.native_instance_count);
        writer.WriteKeyValue("prototype_count", stats.prototype_count);
        writer.WriteKeyValue("prototype_prim_count", stats.prototype_prim_count);
        writer.WriteKeyValue("prototype_property_count", stats.prototype_property_count);
        writer.WriteKeyValue("point_instancer_count", stats.point_instancer_count);
        writer.WriteKeyValue("mesh_count", stats.mesh_count);
        writer.WriteKeyValue("primvar_count", stats.primvar_count);
        writer.WriteKeyValue("material_count", stats.material_count);
        writer.WriteKeyValue("shader_count", stats.shader_count);
        writer.WriteKeyValue("node_graph_count", stats.node_graph_count);
        writer.WriteKeyValue("camera_count", stats.camera_count);
        writer.WriteKeyValue("light_count", stats.light_count);
        writer.WriteKeyValue("render_settings_count", stats.render_settings_count);
        writer.WriteKeyValue("unknown_schema_count", stats.unknown_schema_count);
        writer.WriteKey("prim_type_counts");
        write_string_count_map_(writer, stats.prim_type_counts);
        writer.WriteKey("prototype_type_counts");
        write_string_count_map_(writer, stats.prototype_type_counts);
        writer.WriteKey("applied_schema_counts");
        write_string_count_map_(writer, stats.applied_schema_counts);
        writer.WriteKey("shader_id_counts");
        write_string_count_map_(writer, stats.shader_id_counts);
        writer.EndObject();

        writer.WriteKey("layers");
        writer.BeginArray();
        for (const auto& layer : layers()) {
            writer.BeginObject();
            writer.WriteKeyValue("identifier", layer.identifier);
            writer.WriteKeyValue("resolved_path", layer.resolved_path);
            writer.WriteKeyValue("used_by_stage", layer.used_by_stage);
            writer.WriteKeyValue("is_root_layer", layer.is_root_layer);
            writer.EndObject();
        }
        writer.EndArray();

        writer.WriteKey("source_inventory");
        writer.BeginArray();
        for (const auto& item : inventory()) {
            writer.BeginObject();
            writer.WriteKeyValue("relative_path", item.relative_path);
            writer.WriteKeyValue("category", item.category);
            writer.WriteKeyValue("byte_count", item.byte_count);
            writer.WriteKeyValue("used_by_stage", item.used_by_stage);
            writer.EndObject();
        }
        writer.EndArray();

        writer.WriteKey("asset_references");
        writer.BeginArray();
        for (const auto& asset : asset_references()) {
            writer.BeginObject();
            writer.WriteKeyValue("source_property_path", asset.source_property_path);
            writer.WriteKeyValue("authored_path", asset.authored_path);
            writer.WriteKeyValue("resolved_path", asset.resolved_path);
            writer.WriteKeyValue("category", asset.category);
            writer.WriteKeyValue("resolved", asset.resolved);
            writer.EndObject();
        }
        writer.EndArray();

        writer.WriteKey("diagnostics");
        writer.BeginArray();
        for (const auto& diagnostic : diagnostics()) {
            writer.BeginObject();
            writer.WriteKeyValue("severity", diagnostic.severity);
            writer.WriteKeyValue("message", diagnostic.message);
            writer.WriteKeyValue("source_file", diagnostic.source_file);
            writer.WriteKeyValue("source_line", diagnostic.source_line);
            writer.EndObject();
        }
        writer.EndArray();

        writer.WriteKey("variant_selections");
        writer.BeginArray();
        for (const auto& variant : variant_selections()) {
            writer.BeginObject();
            writer.WriteKeyValue("prim_path", variant.prim_path);
            writer.WriteKeyValue("set_name", variant.set_name);
            writer.WriteKeyValue("selection", variant.selection);
            writer.WriteKey("variants");
            write_string_array_(writer, variant.variants);
            writer.EndObject();
        }
        writer.EndArray();

        writer.WriteKey("prims");
        writer.BeginArray();
        for (const auto& prim : prims()) {
            writer.BeginObject();
            writer.WriteKeyValue("path", prim.path);
            writer.WriteKeyValue("type_name", prim.type_name);
            writer.WriteKeyValue("native_prototype_type", prim.native_prototype_type);
            writer.WriteKeyValue("state_flags", static_cast<uint64_t>(prim.state_flags));
            writer.WriteKeyValue("property_begin", prim.property_begin);
            writer.WriteKeyValue("property_count", prim.property_count);
            writer.WriteKey("applied_schemas");
            write_string_array_(writer, prim.applied_schemas);
            writer.WriteKey("authored_metadata_keys");
            write_string_array_(writer, prim.authored_metadata_keys);
            writer.WriteKey("custom_data_keys");
            write_string_array_(writer, prim.custom_data_keys);
            writer.WriteKey("prim_stack_layers");
            write_string_array_(writer, prim.prim_stack_layers);
            writer.EndObject();
        }
        writer.EndArray();

        writer.WriteKey("properties");
        writer.BeginArray();
        for (const auto& property : properties()) {
            writer.BeginObject();
            writer.WriteKeyValue("path", property.path);
            writer.WriteKeyValue("kind", property.kind);
            writer.WriteKeyValue("type_name", property.type_name);
            writer.WriteKeyValue("resolved_value_source", property.resolved_value_source);
            writer.WriteKeyValue("time_sample_count", property.time_sample_count);
            writer.WriteKeyValue("authored", property.authored);
            writer.WriteKeyValue("has_value_clip", property.has_value_clip);
            writer.WriteKey("authored_metadata_keys");
            write_string_array_(writer, property.authored_metadata_keys);
            writer.WriteKey("custom_data_keys");
            write_string_array_(writer, property.custom_data_keys);
            writer.WriteKey("property_stack_layers");
            write_string_array_(writer, property.property_stack_layers);
            writer.WriteKey("connections_or_targets");
            write_string_array_(writer, property.connections_or_targets);
            writer.EndObject();
        }
        writer.EndArray();

        writer.WriteKey("material_bindings");
        writer.BeginArray();
        for (const auto& binding : material_bindings()) {
            writer.BeginObject();
            writer.WriteKeyValue("prim_path", binding.prim_path);
            writer.WriteKeyValue("purpose", binding.purpose);
            writer.WriteKeyValue("binding_kind", binding.binding_kind);
            writer.WriteKeyValue("strength", binding.strength);
            writer.WriteKeyValue("relationship_path", binding.relationship_path);
            writer.WriteKey("targets");
            write_string_array_(writer, binding.targets);
            writer.EndObject();
        }
        writer.EndArray();

        writer.WriteKey("point_instancers");
        writer.BeginArray();
        for (const auto& point_instancer : point_instancers()) {
            writer.BeginObject();
            writer.WriteKeyValue("prim_path", point_instancer.prim_path);
            writer.WriteKey("prototype_targets");
            write_string_array_(writer, point_instancer.prototype_targets);
            writer.WriteKeyValue(
                "proto_indices_property_path",
                point_instancer.proto_indices_property_path);
            writer.WriteKeyValue(
                "proto_indices_time_sample_count",
                point_instancer.proto_indices_time_sample_count);
            writer.WriteKey("instance_attribute_paths");
            write_string_array_(writer, point_instancer.instance_attribute_paths);
            writer.EndObject();
        }
        writer.EndArray();
        writer.EndObject();
    }

} // namespace scene::raw


#endif