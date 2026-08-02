#include "FastJungle/cooker/JungleUsdImporter.hpp"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/quath.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/tf/stringUtils.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/listOp.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/subset.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/lightAPI.h>
#include <pxr/usd/usdShade/connectableAPI.h>
#include <pxr/usd/usdShade/input.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/output.h>
#include <pxr/usd/usdShade/shader.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace fjr::cooker {

    namespace {

        using Scene = scene::JungleScene;

        Scene::Float2 to_float2(const pxr::GfVec2f& value) {
            return {value[0], value[1]};
        }

        Scene::Float3 to_float3(const pxr::GfVec3f& value) {
            return {value[0], value[1], value[2]};
        }

        Scene::Float4 to_float4(const pxr::GfVec4f& value) {
            return {value[0], value[1], value[2], value[3]};
        }

        template<typename Source, typename Convert>
        auto copy_array(const Source& source, Convert convert) {
            using Destination = decltype(convert(source[0]));
            std::vector<Destination> result;
            result.reserve(source.size());
            for (const auto& value : source) {
                result.push_back(convert(value));
            }
            return result;
        }

        template<typename Source, typename Destination>
        std::vector<Destination> copy_numeric_array(const Source& source) {
            std::vector<Destination> result;
            result.reserve(source.size());
            for (const auto& value : source) {
                result.push_back(static_cast<Destination>(value));
            }
            return result;
        }

        Scene::AssetReference make_asset_reference(
            const pxr::SdfAssetPath& asset_path) {

            Scene::AssetReference result;
            result.authored_path = asset_path.GetAssetPath();
            result.resolved_path = asset_path.GetResolvedPath();
            if (!result.resolved_path.empty()) {
                result.resolved_file_exists = std::filesystem::exists(
                    std::filesystem::path{result.resolved_path});
            }
            return result;
        }

        Scene::Matrix4x4 to_matrix(const pxr::GfMatrix4d& source) {
            Scene::Matrix4x4 result;
            for (std::size_t row = 0; row < 4; ++row) {
                for (std::size_t column = 0; column < 4; ++column) {
                    result.values[row * 4 + column] =
                        source[static_cast<int>(row)][static_cast<int>(column)];
                }
            }
            return result;
        }

        Scene::ShaderConnection to_connection(
            const pxr::UsdShadeConnectionSourceInfo& source) {

            Scene::ShaderConnection result;
            result.source_prim_path =
                source.source.GetPrim().GetPath().GetString();
            result.source_name = source.sourceName.GetString();
            result.source_is_output =
                source.sourceType == pxr::UsdShadeAttributeType::Output;
            return result;
        }

        template<typename ShadingProperty>
        std::vector<Scene::ShaderConnection> read_connections(
            const ShadingProperty& property,
            std::vector<std::string>& invalid_source_paths) {

            pxr::SdfPathVector invalid_paths;
            const auto sources = property.GetConnectedSources(&invalid_paths);
            std::vector<Scene::ShaderConnection> result;
            result.reserve(sources.size());
            for (const auto& source : sources) {
                result.push_back(to_connection(source));
            }
            invalid_source_paths.reserve(invalid_paths.size());
            for (const auto& path : invalid_paths) {
                invalid_source_paths.push_back(path.GetString());
            }
            return result;
        }

        Scene::ShaderValue read_shader_value(
            const pxr::UsdAttribute& attribute,
            std::vector<Scene::Diagnostic>& diagnostics) {

            Scene::ShaderValue result;
            result.type_name = attribute.GetTypeName().GetAsToken().GetString();

            pxr::VtValue value;
            if (!attribute.Get(&value) || value.IsEmpty()) {
                return result;
            }

            if (value.IsHolding<float>()) {
                result.kind = Scene::ShaderValueKind::Float;
                result.data = value.UncheckedGet<float>();
            }
            else if (value.IsHolding<pxr::GfVec2f>()) {
                result.kind = Scene::ShaderValueKind::Float2;
                result.data = to_float2(value.UncheckedGet<pxr::GfVec2f>());
            }
            else if (value.IsHolding<pxr::GfVec3f>()) {
                result.kind = Scene::ShaderValueKind::Float3;
                result.data = to_float3(value.UncheckedGet<pxr::GfVec3f>());
            }
            else if (value.IsHolding<pxr::GfVec4f>()) {
                result.kind = Scene::ShaderValueKind::Float4;
                result.data = to_float4(value.UncheckedGet<pxr::GfVec4f>());
            }
            else if (value.IsHolding<pxr::TfToken>()) {
                result.kind = Scene::ShaderValueKind::Token;
                result.data = value.UncheckedGet<pxr::TfToken>().GetString();
            }
            else if (value.IsHolding<std::string>()) {
                result.kind = Scene::ShaderValueKind::String;
                result.data = value.UncheckedGet<std::string>();
            }
            else if (value.IsHolding<pxr::SdfAssetPath>()) {
                result.kind = Scene::ShaderValueKind::Asset;
                result.data = make_asset_reference(
                    value.UncheckedGet<pxr::SdfAssetPath>());
            }
            else {
                result.kind = Scene::ShaderValueKind::Unsupported;
                result.unsupported_value = pxr::TfStringify(value);
                diagnostics.push_back({
                    Scene::DiagnosticSeverity::Warning,
                    attribute.GetPath().GetString(),
                    "Shader value type was retained as text: " +
                        result.type_name
                });
            }
            return result;
        }

        Scene::ShaderOutput read_shader_output(
            const pxr::UsdShadeOutput& output) {

            Scene::ShaderOutput result;
            result.name = output.GetBaseName().GetString();
            result.type_name = output.GetTypeName().GetAsToken().GetString();
            result.connections = read_connections(
                output,
                result.invalid_source_paths);
            return result;
        }

        Scene::Primvar read_primvar(
            const pxr::UsdGeomPrimvar& source,
            std::vector<Scene::Diagnostic>& diagnostics) {

            Scene::Primvar result;
            result.name = source.GetPrimvarName().GetString();
            result.type_name = source.GetTypeName().GetAsToken().GetString();
            result.interpolation = source.GetInterpolation().GetString();

            pxr::VtIntArray indices;
            if (source.GetIndices(&indices)) {
                result.indices = copy_numeric_array<
                    pxr::VtIntArray,
                    std::int32_t>(indices);
            }

            pxr::VtValue value;
            if (!source.Get(&value)) {
                diagnostics.push_back({
                    Scene::DiagnosticSeverity::Error,
                    source.GetAttr().GetPath().GetString(),
                    "Unable to read primvar value."
                });
                result.storage = Scene::PrimvarStorage::Float;
                result.data = std::vector<float>{};
                return result;
            }

            if (value.IsHolding<pxr::VtBoolArray>()) {
                result.storage = Scene::PrimvarStorage::Boolean;
                const auto& values = value.UncheckedGet<pxr::VtBoolArray>();
                std::vector<std::uint8_t> copied;
                copied.reserve(values.size());
                for (const bool entry : values) {
                    copied.push_back(entry ? 1u : 0u);
                }
                result.data = std::move(copied);
            }
            else if (value.IsHolding<pxr::VtFloatArray>()) {
                result.storage = Scene::PrimvarStorage::Float;
                result.data = copy_numeric_array<
                    pxr::VtFloatArray,
                    float>(value.UncheckedGet<pxr::VtFloatArray>());
            }
            else if (value.IsHolding<pxr::VtVec2fArray>()) {
                result.storage = Scene::PrimvarStorage::Float2;
                result.data = copy_array(
                    value.UncheckedGet<pxr::VtVec2fArray>(),
                    to_float2);
            }
            else if (value.IsHolding<pxr::VtVec3fArray>()) {
                result.storage = Scene::PrimvarStorage::Float3;
                result.data = copy_array(
                    value.UncheckedGet<pxr::VtVec3fArray>(),
                    to_float3);
            }
            else {
                diagnostics.push_back({
                    Scene::DiagnosticSeverity::Error,
                    source.GetAttr().GetPath().GetString(),
                    "Unsupported Intel Jungle mesh primvar type: " +
                        result.type_name
                });
                result.storage = Scene::PrimvarStorage::Float;
                result.data = std::vector<float>{};
            }
            return result;
        }

        Scene::PrimKind classify_prim(const pxr::UsdPrim& prim) {
            if (prim.IsA<pxr::UsdGeomMesh>()) {
                return Scene::PrimKind::Mesh;
            }
            if (prim.IsA<pxr::UsdGeomSubset>()) {
                return Scene::PrimKind::GeomSubset;
            }
            if (prim.IsA<pxr::UsdGeomPointInstancer>()) {
                return Scene::PrimKind::PointInstancer;
            }
            if (prim.IsA<pxr::UsdShadeMaterial>()) {
                return Scene::PrimKind::Material;
            }
            if (prim.IsA<pxr::UsdShadeShader>()) {
                return Scene::PrimKind::Shader;
            }
            if (prim.IsA<pxr::UsdGeomCamera>()) {
                return Scene::PrimKind::Camera;
            }
            if (prim.IsA<pxr::UsdLuxDomeLight>()) {
                return Scene::PrimKind::Light;
            }
            if (prim.IsA<pxr::UsdGeomScope>()) {
                return Scene::PrimKind::Scope;
            }
            if (prim.IsA<pxr::UsdGeomXform>()) {
                return Scene::PrimKind::Transform;
            }
            return Scene::PrimKind::Other;
        }

        class ImportContext {
        public:
            explicit ImportContext(pxr::UsdStageRefPtr stage)
                : stage_(std::move(stage)) {

                scene_.nodes.reserve(4000);
                scene_.meshes.reserve(160);
                scene_.mesh_subsets.reserve(40);
                scene_.point_instancers.reserve(778);
                scene_.native_instances.reserve(741);
                scene_.shader_nodes.reserve(700);
                scene_.materials.reserve(140);
            }

            Scene run(const std::filesystem::path& source_root) {
                scene_.source_root = source_root.generic_string();
                scene_.up_axis =
                    pxr::UsdGeomGetStageUpAxis(stage_).GetString();
                scene_.meters_per_unit =
                    pxr::UsdGeomGetStageMetersPerUnit(stage_);
                scene_.start_time_code = stage_->GetStartTimeCode();
                scene_.end_time_code = stage_->GetEndTimeCode();
                read_layers();

                for (const auto& prim : stage_->Traverse()) {
                    ++scene_.statistics.composed_prim_count;
                    import_prim(prim, false, true);
                }

                const auto prototypes = stage_->GetPrototypes();
                scene_.statistics.native_prototype_count = prototypes.size();
                for (const auto& prototype : prototypes) {
                    for (const auto& prim : pxr::UsdPrimRange(prototype)) {
                        import_prim(prim, true, false);
                    }
                }

                return std::move(scene_);
            }

        private:
            void read_layers() {
                const auto root_layer = stage_->GetRootLayer();
                for (const auto& layer : stage_->GetUsedLayers()) {
                    Scene::SourceLayer result;
                    result.identifier = layer->GetIdentifier();
                    result.resolved_path = layer->GetRealPath();
                    result.is_root = layer == root_layer;
                    scene_.source_layers.push_back(std::move(result));
                }
            }

            std::uint32_t add_node(
                const pxr::UsdPrim& prim,
                bool inside_native_prototype) {

                Scene::Node node;
                node.path = prim.GetPath().GetString();
                node.name = prim.GetName().GetString();
                node.usd_type_name = prim.GetTypeName().GetString();
                node.prim_kind = classify_prim(prim);
                node.object_kind = Scene::classify_object(node.path);
                if (prim.IsActive()) {
                    node.flags |= Scene::NodeActive;
                }
                if (inside_native_prototype) {
                    node.flags |= Scene::NodeInsideNativePrototype;
                }
                if (prim.IsPrototype()) {
                    node.flags |= Scene::NodeNativePrototype;
                }
                if (prim.IsInstance()) {
                    node.flags |= Scene::NodeNativeInstance;
                    const auto prototype = prim.GetPrototype();
                    if (prototype) {
                        node.native_prototype_path =
                            prototype.GetPath().GetString();
                    }
                }

                const pxr::UsdGeomImageable imageable{prim};
                if (imageable) {
                    const auto visibility = imageable.ComputeVisibility();
                    if (visibility != pxr::UsdGeomTokens->invisible) {
                        node.flags |= Scene::NodeVisible;
                    }
                    pxr::TfToken purpose;
                    if (imageable.GetPurposeAttr().Get(&purpose)) {
                        node.purpose = purpose.GetString();
                    }
                }

                const pxr::UsdGeomXformable xformable{prim};
                if (xformable) {
                    pxr::GfMatrix4d transform{1.0};
                    bool resets_transform = false;
                    if (xformable.GetLocalTransformation(
                        &transform,
                        &resets_transform)) {
                        node.local_transform = to_matrix(transform);
                    }
                    if (resets_transform) {
                        node.flags |= Scene::NodeResetsTransform;
                    }
                }

                const auto parent_path = prim.GetPath().GetParentPath().GetString();
                const auto parent = node_indices_.find(parent_path);
                if (parent != node_indices_.end()) {
                    node.parent = parent->second;
                }

                const auto index = static_cast<std::uint32_t>(
                    scene_.nodes.size());
                node_indices_.emplace(node.path, index);
                scene_.nodes.push_back(std::move(node));
                return index;
            }

            void import_prim(
                const pxr::UsdPrim& prim,
                bool inside_native_prototype,
                bool composed_stage) {

                const auto node_index = add_node(
                    prim,
                    inside_native_prototype);
                inspect_time_samples(prim);

                if (prim.IsInstance()) {
                    Scene::NativeInstance instance;
                    instance.prim_path = prim.GetPath().GetString();
                    const auto prototype = prim.GetPrototype();
                    if (prototype) {
                        instance.prototype_path =
                            prototype.GetPath().GetString();
                    }
                    instance.object_kind =
                        Scene::classify_object(instance.prim_path);
                    scene_.native_instances.push_back(std::move(instance));
                }

                if (prim.IsA<pxr::UsdGeomPointInstancer>()) {
                    import_point_instancer(prim, node_index);
                }
                else if (prim.IsA<pxr::UsdGeomMesh>()) {
                    if (composed_stage) {
                        ++scene_.statistics.composed_mesh_count;
                    }
                    import_mesh(prim, node_index);
                }
                else if (prim.IsA<pxr::UsdGeomSubset>()) {
                    if (composed_stage) {
                        ++scene_.statistics.composed_mesh_subset_count;
                    }
                    import_mesh_subset(prim, node_index);
                }
                else if (prim.IsA<pxr::UsdShadeMaterial>()) {
                    if (composed_stage) {
                        ++scene_.statistics.composed_material_count;
                    }
                    import_material(prim, node_index);
                }
                else if (prim.IsA<pxr::UsdShadeShader>()) {
                    if (composed_stage) {
                        ++scene_.statistics.composed_shader_count;
                    }
                    import_shader(prim, node_index);
                }
                else if (prim.IsA<pxr::UsdGeomCamera>()) {
                    if (composed_stage) {
                        ++scene_.statistics.composed_camera_count;
                    }
                    import_camera(prim, node_index);
                }
                else if (prim.IsA<pxr::UsdLuxDomeLight>()) {
                    import_environment_light(prim, node_index);
                }
            }

            void inspect_time_samples(const pxr::UsdPrim& prim) {
                for (const auto& attribute : prim.GetAttributes()) {
                    std::vector<double> samples;
                    if (attribute.GetTimeSamples(&samples) &&
                        !samples.empty()) {
                        ++scene_.statistics.time_sampled_attribute_count;
                        scene_.statistics.time_sample_count += samples.size();
                        scene_.import_diagnostics.push_back({
                            Scene::DiagnosticSeverity::Error,
                            attribute.GetPath().GetString(),
                            "Time-sampled attribute is not materialized yet."
                        });
                    }
                }
            }

            void import_point_instancer(
                const pxr::UsdPrim& prim,
                std::uint32_t node_index) {

                const pxr::UsdGeomPointInstancer source{prim};
                Scene::PointInstancer result;
                result.prim_path = prim.GetPath().GetString();
                result.object_kind =
                    Scene::classify_object(result.prim_path);

                pxr::SdfPathVector prototypes;
                source.GetPrototypesRel().GetTargets(&prototypes);
                result.prototype_paths.reserve(prototypes.size());
                for (const auto& path : prototypes) {
                    result.prototype_paths.push_back(path.GetString());
                }

                pxr::VtIntArray prototype_indices;
                source.GetProtoIndicesAttr().Get(&prototype_indices);
                result.prototype_indices = copy_numeric_array<
                    pxr::VtIntArray,
                    std::int32_t>(prototype_indices);

                pxr::VtVec3fArray positions;
                source.GetPositionsAttr().Get(&positions);
                result.positions = copy_array(positions, to_float3);
                for (const auto& position : result.positions) {
                    if (position.x == 0.0f &&
                        position.y == 0.0f &&
                        position.z == 0.0f) {
                        ++scene_.statistics.exact_origin_instance_count;
                    }
                }

                pxr::VtQuathArray orientations;
                source.GetOrientationsAttr().Get(&orientations);
                result.orientations.reserve(orientations.size());
                for (const auto& orientation : orientations) {
                    const auto imaginary = orientation.GetImaginary();
                    result.orientations.push_back({
                        static_cast<float>(orientation.GetReal()),
                        {
                            static_cast<float>(imaginary[0]),
                            static_cast<float>(imaginary[1]),
                            static_cast<float>(imaginary[2])
                        }
                    });
                }

                pxr::VtVec3fArray scales;
                source.GetScalesAttr().Get(&scales);
                result.scales = copy_array(scales, to_float3);

                pxr::VtVec3fArray velocities;
                if (source.GetVelocitiesAttr().Get(&velocities)) {
                    result.velocities = copy_array(velocities, to_float3);
                }
                pxr::VtVec3fArray accelerations;
                if (source.GetAccelerationsAttr().Get(&accelerations)) {
                    result.accelerations = copy_array(
                        accelerations,
                        to_float3);
                }
                pxr::VtVec3fArray angular_velocities;
                if (source.GetAngularVelocitiesAttr().Get(&angular_velocities)) {
                    result.angular_velocities = copy_array(
                        angular_velocities,
                        to_float3);
                }
                pxr::VtInt64Array ids;
                if (source.GetIdsAttr().Get(&ids)) {
                    result.ids = copy_numeric_array<
                        pxr::VtInt64Array,
                        std::int64_t>(ids);
                }
                pxr::SdfInt64ListOp inactive_ids;
                if (prim.GetMetadata(
                    pxr::UsdGeomTokens->inactiveIds,
                    &inactive_ids)) {
                    result.inactive_ids = inactive_ids.GetExplicitItems();
                }
                pxr::VtInt64Array invisible_ids;
                if (source.GetInvisibleIdsAttr().Get(&invisible_ids)) {
                    result.invisible_ids = copy_numeric_array<
                        pxr::VtInt64Array,
                        std::int64_t>(invisible_ids);
                }

                const pxr::UsdGeomPrimvarsAPI primvars{prim};
                for (const auto& primvar : primvars.GetPrimvarsWithValues()) {
                    result.primvars.push_back(read_primvar(
                        primvar,
                        scene_.import_diagnostics));
                }

                scene_.nodes[node_index].payload =
                    static_cast<std::uint32_t>(scene_.point_instancers.size());
                scene_.point_instancers.push_back(std::move(result));
            }

            void import_mesh(
                const pxr::UsdPrim& prim,
                std::uint32_t node_index) {

                const pxr::UsdGeomMesh source{prim};
                Scene::Mesh result;
                result.prim_path = prim.GetPath().GetString();

                pxr::TfToken token;
                source.GetOrientationAttr().Get(&token);
                result.orientation = token.GetString();
                source.GetSubdivisionSchemeAttr().Get(&token);
                result.subdivision_scheme = token.GetString();
                result.normals_interpolation =
                    source.GetNormalsInterpolation().GetString();
                source.GetDoubleSidedAttr().Get(&result.double_sided);

                const pxr::UsdShadeMaterialBindingAPI material_binding{prim};
                const auto material = material_binding.ComputeBoundMaterial();
                if (material) {
                    result.material_path = material.GetPath().GetString();
                }

                pxr::VtVec3fArray points;
                source.GetPointsAttr().Get(&points);
                result.points = copy_array(points, to_float3);

                pxr::VtIntArray values;
                source.GetFaceVertexCountsAttr().Get(&values);
                result.face_vertex_counts = copy_numeric_array<
                    pxr::VtIntArray,
                    std::int32_t>(values);
                source.GetFaceVertexIndicesAttr().Get(&values);
                result.face_vertex_indices = copy_numeric_array<
                    pxr::VtIntArray,
                    std::int32_t>(values);
                source.GetHoleIndicesAttr().Get(&values);
                result.hole_indices = copy_numeric_array<
                    pxr::VtIntArray,
                    std::int32_t>(values);

                pxr::VtVec3fArray normals;
                source.GetNormalsAttr().Get(&normals);
                result.normals = copy_array(normals, to_float3);

                const pxr::UsdGeomPrimvarsAPI primvars{prim};
                for (const auto& primvar : primvars.GetPrimvarsWithValues()) {
                    result.primvars.push_back(read_primvar(
                        primvar,
                        scene_.import_diagnostics));
                }

                scene_.nodes[node_index].payload =
                    static_cast<std::uint32_t>(scene_.meshes.size());
                scene_.meshes.push_back(std::move(result));
            }

            void import_mesh_subset(
                const pxr::UsdPrim& prim,
                std::uint32_t node_index) {

                const pxr::UsdGeomSubset source{prim};
                Scene::MeshSubset result;
                result.prim_path = prim.GetPath().GetString();
                result.mesh_path = prim.GetPath().GetParentPath().GetString();

                pxr::TfToken token;
                source.GetElementTypeAttr().Get(&token);
                result.element_type = token.GetString();
                source.GetFamilyNameAttr().Get(&token);
                result.family_name = token.GetString();
                result.family_type = pxr::UsdGeomSubset::GetFamilyType(
                    pxr::UsdGeomImageable{prim.GetParent()},
                    pxr::TfToken{result.family_name}).GetString();

                pxr::VtIntArray indices;
                source.GetIndicesAttr().Get(&indices);
                result.indices = copy_numeric_array<
                    pxr::VtIntArray,
                    std::int32_t>(indices);

                const pxr::UsdShadeMaterialBindingAPI material_binding{prim};
                const auto material = material_binding.ComputeBoundMaterial();
                if (material) {
                    result.material_path = material.GetPath().GetString();
                }

                scene_.nodes[node_index].payload =
                    static_cast<std::uint32_t>(scene_.mesh_subsets.size());
                scene_.mesh_subsets.push_back(std::move(result));
            }

            void import_material(
                const pxr::UsdPrim& prim,
                std::uint32_t node_index) {

                const pxr::UsdShadeMaterial source{prim};
                Scene::Material result;
                result.prim_path = prim.GetPath().GetString();
                for (const auto& output : source.GetOutputs()) {
                    result.outputs.push_back(read_shader_output(output));
                }

                const auto index = static_cast<std::uint32_t>(
                    scene_.materials.size());
                material_indices_.emplace(result.prim_path, index);
                scene_.nodes[node_index].payload = index;
                scene_.materials.push_back(std::move(result));
            }

            void import_shader(
                const pxr::UsdPrim& prim,
                std::uint32_t node_index) {

                const pxr::UsdShadeShader source{prim};
                Scene::ShaderNode result;
                result.prim_path = prim.GetPath().GetString();
                pxr::TfToken shader_id;
                source.GetShaderId(&shader_id);
                result.shader_id = shader_id.GetString();

                for (const auto& input : source.GetInputs()) {
                    Scene::ShaderInput imported;
                    imported.name = input.GetBaseName().GetString();
                    imported.value = read_shader_value(
                        input.GetAttr(),
                        scene_.import_diagnostics);
                    imported.connections = read_connections(
                        input,
                        imported.invalid_source_paths);
                    result.inputs.push_back(std::move(imported));
                }
                for (const auto& output : source.GetOutputs()) {
                    result.outputs.push_back(read_shader_output(output));
                }

                const auto index = static_cast<std::uint32_t>(
                    scene_.shader_nodes.size());
                scene_.nodes[node_index].payload = index;
                scene_.shader_nodes.push_back(std::move(result));

                auto ancestor = prim.GetParent();
                while (ancestor) {
                    const auto material = material_indices_.find(
                        ancestor.GetPath().GetString());
                    if (material != material_indices_.end()) {
                        scene_.materials[material->second].shader_nodes.push_back(
                            index);
                        break;
                    }
                    ancestor = ancestor.GetParent();
                }
            }

            void import_camera(
                const pxr::UsdPrim& prim,
                std::uint32_t node_index) {

                const pxr::UsdGeomCamera source{prim};
                Scene::Camera result;
                result.prim_path = prim.GetPath().GetString();
                pxr::TfToken projection;
                source.GetProjectionAttr().Get(&projection);
                result.projection = projection.GetString();
                source.GetFocalLengthAttr().Get(&result.focal_length);
                source.GetHorizontalApertureAttr().Get(
                    &result.horizontal_aperture);
                source.GetVerticalApertureAttr().Get(
                    &result.vertical_aperture);
                source.GetHorizontalApertureOffsetAttr().Get(
                    &result.horizontal_aperture_offset);
                source.GetVerticalApertureOffsetAttr().Get(
                    &result.vertical_aperture_offset);
                source.GetFocusDistanceAttr().Get(&result.focus_distance);
                source.GetFStopAttr().Get(&result.f_stop);
                pxr::GfVec2f clipping_range;
                source.GetClippingRangeAttr().Get(&clipping_range);
                result.clipping_range = to_float2(clipping_range);

                scene_.nodes[node_index].payload =
                    static_cast<std::uint32_t>(scene_.cameras.size());
                scene_.cameras.push_back(std::move(result));
            }

            void import_environment_light(
                const pxr::UsdPrim& prim,
                std::uint32_t node_index) {

                const pxr::UsdLuxDomeLight source{prim};
                const pxr::UsdLuxLightAPI light{prim};
                Scene::EnvironmentLight result;
                result.prim_path = prim.GetPath().GetString();
                pxr::GfVec3f color{1.0f};
                light.GetColorAttr().Get(&color);
                result.color = to_float3(color);
                light.GetIntensityAttr().Get(&result.intensity);
                light.GetExposureAttr().Get(&result.exposure);
                pxr::SdfAssetPath texture;
                if (source.GetTextureFileAttr().Get(&texture)) {
                    result.texture = make_asset_reference(texture);
                }

                scene_.nodes[node_index].payload = static_cast<std::uint32_t>(
                    scene_.environment_lights.size());
                scene_.environment_lights.push_back(std::move(result));
            }

            pxr::UsdStageRefPtr stage_;
            Scene scene_;
            std::unordered_map<std::string, std::uint32_t> node_indices_;
            std::unordered_map<std::string, std::uint32_t> material_indices_;
        };

    } // namespace

    scene::JungleScene JungleUsdImporter::import_scene(
        const std::filesystem::path& root_layer) {

        const auto absolute_path = std::filesystem::absolute(root_layer);
        if (!std::filesystem::is_regular_file(absolute_path)) {
            throw std::runtime_error(
                "Jungle root layer does not exist: " +
                absolute_path.generic_string());
        }

        auto stage = pxr::UsdStage::Open(
            absolute_path.generic_string(),
            pxr::UsdStage::LoadAll);
        if (!stage) {
            throw std::runtime_error(
                "OpenUSD could not open: " + absolute_path.generic_string());
        }

        ImportContext context{std::move(stage)};
        return context.run(absolute_path);
    }

} // namespace fjr::cooker
