#include "JungleSceneCompiler.hpp"

#include "FastJungle/core/math/CheckedCast.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/scene/StaticScene.hpp"

#include "JungleSceneProfile.hpp"
#include "PathKey.hpp"
#include "SceneSpace.hpp"
#include "StaticSceneAssembler.hpp"
#include "UsdCameraCompiler.hpp"
#include "UsdEnvironmentCompiler.hpp"
#include "UsdMaterialCompiler.hpp"
#include "UsdMeshCompiler.hpp"

#include <pxr/base/gf/frustum.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/quath.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
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

#include <algorithm>
#include <bit>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fjr::cooker {

    namespace {

        using StaticScene = scene::StaticScene;
        using SourceComponent = internal::JungleComponent;
        using internal::JungleSceneProfile;
        using internal::MaterialProduct;
        using internal::path_leaf;
        using internal::SceneSpace;
        using internal::StaticSceneAssembler;
        using internal::UsdCameraCompiler;
        using internal::UsdEnvironmentCompiler;
        using internal::UsdMaterialCompiler;
        using internal::UsdMeshCompiler;

        constexpr float VECTOR_EPSILON = 1.0e-10f;
        constexpr std::size_t MAX_PROTOTYPE_DEPTH = 64;

        [[noreturn]] void fail(std::string message) {
            log::Logger::g_logger << log::abrt(message);
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

            return math::checked_cast<std::uint32_t>(value, subject);
        }

        [[nodiscard]] bool is_visible(const pxr::UsdPrim& prim) {
            if (!prim || !prim.IsActive()) {
                return false;
            }
            const pxr::UsdGeomImageable imageable{prim};
            return !imageable ||
                imageable.ComputeVisibility() != pxr::UsdGeomTokens->invisible;
        }

		struct PointInstancerSource {
			StaticScene::EnumPointCategory category =
				StaticScene::EnumPointCategory::ANTHURIUM;
			std::uint32_t mesh = StaticScene::INVALID_INDEX;
			DirectX::XMFLOAT4X4 local_transform =
				StaticScene::IDENTITY_TRANSFORM;
			std::uint32_t source_ordinal = 0;
			pxr::UsdPrim prim;
		};

		struct ResolvedMesh {
			std::uint32_t mesh = StaticScene::INVALID_INDEX;
			DirectX::XMFLOAT4X4 local_transform =
				StaticScene::IDENTITY_TRANSFORM;
		};

		[[nodiscard]] bool matrix_bits_equal(
			const DirectX::XMFLOAT4X4& left,
			const DirectX::XMFLOAT4X4& right) noexcept {

			return std::bit_cast<std::array<std::uint32_t, 16>>(left) ==
				std::bit_cast<std::array<std::uint32_t, 16>>(right);
		}

        class Builder final {
        public:
            explicit Builder(pxr::UsdStageRefPtr stage)
                : stage_(std::move(stage)),
                  profile_(stage_),
                  converter_(pxr::UsdGeomGetStageMetersPerUnit(stage_)),
                  // StaticScene intentionally ignores animation. Every USD
                  // attribute and transform is sampled at default time.
                  xform_cache_(pxr::UsdTimeCode::Default()),
                  material_compiler_(assembler_),
                  mesh_compiler_(converter_, assembler_, material_compiler_),
                  result_(&assembler_.storage()) {}

            [[nodiscard]] StaticSceneBuild run() {
                if (pxr::UsdGeomGetStageUpAxis(stage_) != pxr::UsdGeomTokens->z) {
                    fail("Intel Jungle root stage must be Z-up.");
                }

                collect_point_instancers();
                build_point_batches();
                build_direct_scene();
				flatten_static_components();
				JungleSceneProfile::validate_contract(*result_);

                if (result_->vertices.empty() || result_->meshes.empty()) {
                    fail("Intel Jungle produced no renderable meshes.");
                }
                if (result_->point_batches.empty()) {
                    fail("Intel Jungle produced no point batches.");
                }

                return assembler_.finish();
            }

        private:
            [[nodiscard]] std::uint32_t add_string(std::string_view value) {
                return assembler_.intern_string(value).value();
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
                return converter_.object_transform_to_target(local);
            }

            [[nodiscard]] DirectX::XMMATRIX world_transform(
                const pxr::UsdPrim& prim) {

                return converter_.object_transform_to_target(
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

					const auto component = profile_.component_for_prim(prim);
					if (!JungleSceneProfile::is_point_component(component)) {
						fail(
							"PointInstancer belongs to a non-point root component: ",
							prim.GetPath().GetString());
					}

					const auto prototype_prim = stage_->GetPrimAtPath(
						targets.front());
					if (!prototype_prim) {
						fail(
							"Missing PointInstancer prototype: ",
							targets.front().GetString());
					}

					const auto resolved = resolve_single_mesh(prototype_prim);
					const auto instancer_world = SceneSpace::store(
						world_transform(prim));
					if (!matrix_bits_equal(
						instancer_world,
						StaticScene::IDENTITY_TRANSFORM)) {

						fail(
							"PointInstancer world transform is not bit-exact identity: ",
							prim.GetPath().GetString());
					}

					point_instancers_.push_back({
						.category = JungleSceneProfile::point_category(component),
						.mesh = resolved.mesh,
						.local_transform = resolved.local_transform,
						.source_ordinal = checked_u32(
							point_instancers_.size(),
							"PointInstancer source ordinal"),
						.prim = prim,
					});
                }

				std::ranges::stable_sort(
					point_instancers_,
					[](const PointInstancerSource& left,
					   const PointInstancerSource& right) {
						return std::tuple{
							left.mesh,
							static_cast<std::uint32_t>(left.category),
							left.source_ordinal,
						} < std::tuple{
							right.mesh,
							static_cast<std::uint32_t>(right.category),
							right.source_ordinal,
						};
					});
            }

			void build_point_batches() {
				std::size_t source_index = 0;
				while (source_index < point_instancers_.size()) {
					const auto mesh = point_instancers_[source_index].mesh;
					const auto local_transform =
						point_instancers_[source_index].local_transform;

					while (source_index < point_instancers_.size() &&
						point_instancers_[source_index].mesh == mesh) {

						const auto& first_source =
							point_instancers_[source_index];
						if (!matrix_bits_equal(
							first_source.local_transform,
							local_transform)) {

							fail(
								"One point mesh resolves to multiple local transforms: ",
								first_source.prim.GetPath().GetString());
						}

						StaticScene::PointBatch batch;
						batch.mesh = mesh;
						batch.local_transform = local_transform;
						batch.category = first_source.category;
						batch.instances.offset = checked_u32(
							result_->point_instances.size(),
							"Point batch instance offset");

						while (source_index < point_instancers_.size() &&
							point_instancers_[source_index].mesh == mesh &&
							point_instancers_[source_index].category ==
								batch.category) {

							const auto& source = point_instancers_[source_index];
							if (!matrix_bits_equal(
								source.local_transform,
								local_transform)) {

								fail(
									"One point mesh resolves to multiple local transforms: ",
									source.prim.GetPath().GetString());
							}

							append_point_instances(source.prim);
							++source_index;
						}

						batch.instances.count = checked_u32(
							result_->point_instances.size() -
								batch.instances.offset,
							"Point batch instance count");
						if (batch.instances.count == 0) {
							fail("Point batch has no visible instances.");
						}
						result_->point_batches.push_back(batch);
					}
				}
			}

			void append_point_instances(const pxr::UsdPrim& prim) {
                    const pxr::UsdGeomPointInstancer source{prim};
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

                    for (std::size_t index = 0; index < positions.size(); ++index) {
                        const std::int64_t id = ids.size() == positions.size()
                            ? ids[index]
                            : static_cast<std::int64_t>(index);
                        if (hidden_ids.contains(id)) {
                            continue;
                        }

                        result_->point_instances.push_back({
                            converter_.position_to_meters(positions[index]),
                            converter_.orientation_to_target(
                                orientations[index]),
                            converter_.scale_to_target(scales[index])
                        });
                    }

            }

            void build_direct_scene() {
                for (const auto& prim : stage_->Traverse()) {
                    if (prim.IsA<pxr::UsdGeomCamera>()) {
                        if (is_visible(prim)) {
							if (profile_.component_for_prim(prim) !=
								SourceComponent::CAMERA) {
								fail("Camera is not owned by the camera root layer.");
							}
                            UsdCameraCompiler::compile(
                                prim,
                                converter_,
                                assembler_);
                        }
                        continue;
                    }
                    if (prim.IsA<pxr::UsdLuxDomeLight>()) {
                        if (is_visible(prim)) {
							if (profile_.component_for_prim(prim) !=
								SourceComponent::ROOT) {
								fail("Environment light is not owned by the root USDA.");
							}
                            UsdEnvironmentCompiler::compile(
                                prim,
                                world_transform(prim),
                                assembler_);
                        }
                        continue;
                    }
                    if (prim.IsA<pxr::UsdGeomPointInstancer>()) {
                        continue;
                    }

					const auto component = profile_.component_for_prim(prim);
					// A point component may contain authored prototype definitions as
					// children. They are inputs to PointInstancer, never direct scene
					// objects, even when an unused definition is not a relationship target.
					if (JungleSceneProfile::is_point_component(component)) {
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
						if (!JungleSceneProfile::is_static_component(component)) {
                            fail(
								"Direct instance belongs to a non-static component: ",
                                prim.GetPath().GetString());
                        }
						const auto definition = resolve_single_mesh(
							native_prototype);
						const auto final_transform = DirectX::XMMatrixMultiply(
							DirectX::XMLoadFloat4x4(&definition.local_transform),
							world_transform(prim));
						append_static_instance(
							component,
                            prim.GetName().GetString(),
							definition.mesh,
							final_transform);
                        continue;
                    }

                    if (prim.IsA<pxr::UsdGeomMesh>() && is_visible(prim)) {
						if (!JungleSceneProfile::is_static_component(component)) {
                            fail(
								"Direct mesh belongs to a non-static component: ",
                                prim.GetPath().GetString());
                        }
						const auto mesh =
							mesh_compiler_.compile(prim).value();
						if (component == SourceComponent::RIVER) {
							flip_mesh_facing(mesh);
						}
						append_static_instance(
							component,
                            prim.GetName().GetString(),
							mesh,
                            world_transform(prim));
                    }
                }
            }

			void flip_mesh_facing(std::uint32_t mesh_index) {

				const auto& mesh = result_->meshes[mesh_index];
				for (std::uint32_t lod_index = 0;
					lod_index < mesh.lod_count;
					++lod_index) {

					const auto& lod = result_->mesh_lods[
						mesh.lod_offset + lod_index];
					for (std::uint32_t local_submesh = 0;
						local_submesh < lod.submesh_count;
						++local_submesh) {

						const auto& submesh = result_->submeshes[
							lod.submesh_offset + local_submesh];
						for (std::uint32_t index = 0;
							index < submesh.index_count;
							index += 3) {

							std::swap(
								result_->indices[
									submesh.index_offset + index + 1],
								result_->indices[
									submesh.index_offset + index + 2]);
						}

						for (std::uint32_t vertex = 0;
							vertex < submesh.vertex_count;
							++vertex) {

							auto& normal = result_->vertices[
								submesh.vertex_offset + vertex].normal;
							normal.x = -normal.x;
							normal.y = -normal.y;
							normal.z = -normal.z;
						}
					}
				}
			}

			[[nodiscard]] ResolvedMesh resolve_single_mesh(
				const pxr::UsdPrim& root) {

				ResolvedMesh result;
				bool found = false;
				collect_single_mesh(
					root,
					DirectX::XMMatrixIdentity(),
					result,
					found,
					0);
				if (!found) {
					fail(
						"Instanced definition has no mesh: ",
						root.GetPath().GetString());
				}
				return result;
			}

			void collect_single_mesh(
                const pxr::UsdPrim& prim,
                DirectX::FXMMATRIX parent_transform,
				ResolvedMesh& result,
				bool& found,
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
					collect_single_mesh(
						native_prototype,
						current,
						result,
						found,
						depth + 1);
                    return;
                }

                if (prim.IsA<pxr::UsdGeomMesh>()) {
					// All used Jungle PointInstancer and native-instance definitions
					// resolve to one mesh. Enforcing that fact here prevents a generic
					// part graph from leaking into the Cooked ABI.
					if (found) {
						fail(
							"Instanced definition contains multiple meshes: ",
							prim.GetPath().GetString());
					}
					result.mesh = mesh_compiler_.compile(prim).value();
					result.local_transform = SceneSpace::store(current);
					found = true;
                }

                for (const auto& child : prim.GetChildren()) {
					collect_single_mesh(
                        child,
                        current,
						result,
						found,
                        depth + 1);
                }
            }

			void append_static_instance(
				SourceComponent component,
                std::string_view name,
				std::uint32_t mesh,
                DirectX::FXMMATRIX world) {

				pending_static_instances_[static_cast<std::size_t>(component)]
					.push_back({
						.name = add_string(name),
						.mesh = mesh,
						.world_transform = SceneSpace::store(world),
					});
            }

			void flatten_static_components() {
				auto append_single = [this](
					SourceComponent component,
					std::uint32_t& destination) {

					auto& pending = pending_static_instances_[
						static_cast<std::size_t>(component)];
					if (pending.size() != 1) {
						fail("Single static Jungle component did not produce one mesh.");
					}
					destination = checked_u32(
						result_->static_mesh_instances.size(),
						"Static component instance index");
					result_->static_mesh_instances.push_back(pending.front());
				};

				append_single(SourceComponent::PYRAMID, result_->components.pyramid.instance);
				append_single(SourceComponent::RIVER, result_->components.river.instance);
				append_single(SourceComponent::CREEK, result_->components.creek.instance);
				append_single(SourceComponent::BANYAN, result_->components.banyan.instance);

				auto append_range = [this](SourceComponent component) {
					auto& pending = pending_static_instances_[
						static_cast<std::size_t>(component)];
					StaticScene::IndexRange range{
						.offset = checked_u32(
							result_->static_mesh_instances.size(),
							"Static component range offset"),
						.count = checked_u32(
							pending.size(),
							"Static component range count"),
					};
					result_->static_mesh_instances.insert(
						result_->static_mesh_instances.end(),
						pending.begin(),
						pending.end());
					return range;
				};

				result_->components.terrain.extended = append_range(
					SourceComponent::TERRAIN_EXTENDED);
				result_->components.terrain.cinematic = append_range(
					SourceComponent::TERRAIN_CINEMATIC);
            }
        private:
            pxr::UsdStageRefPtr stage_;
            JungleSceneProfile profile_;
            SceneSpace converter_;
            pxr::UsdGeomXformCache xform_cache_;
            StaticSceneAssembler assembler_;
            UsdMaterialCompiler material_compiler_;
            UsdMeshCompiler mesh_compiler_;
            StaticScene* result_ = nullptr;

			std::vector<PointInstancerSource> point_instancers_;

			std::array<
				std::vector<StaticScene::StaticMeshInstance>,
				static_cast<std::size_t>(SourceComponent::COUNT)>
				pending_static_instances_;
        };

    } // namespace

    StaticSceneBuild internal::JungleSceneCompiler::compile(
        pxr::UsdStageRefPtr stage) {

        return Builder{std::move(stage)}.run();
    }

} // namespace fjr::cooker
