#include "UsdMeshBuilder.hpp"

#include "SceneSpace.hpp"
#include "StaticSceneDataBuilder.hpp"
#include "UsdMaterialBuilder.hpp"

#include "FastJungle/core/util/CheckedCast.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/core/util/Path.hpp"
#include "FastJungle/scene/StaticScene.hpp"

#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/subset.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fjr::cooker::internal {

    using log::fail;
    using util::checked_u32;
    using util::path_leaf;

    namespace {

        using StaticScene = scene::StaticScene;

        constexpr float VECTOR_EPSILON = 1.0e-10f;

        [[nodiscard]] std::array<uint32_t, 8> vertex_words(
            const StaticScene::Vertex& vertex) noexcept {

            static_assert(sizeof(StaticScene::Vertex) ==
                sizeof(std::array<uint32_t, 8>));
            return std::bit_cast<std::array<uint32_t, 8>>(vertex);
        }

        [[nodiscard]] uint64_t hash_vertex(
            const StaticScene::Vertex& vertex) noexcept {

            uint64_t hash = 14695981039346656037ull;
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
                uint32_t vertex_offset)
                : vertices_(vertices), vertex_offset_(vertex_offset) {}

            [[nodiscard]] uint32_t get_or_add(
                const StaticScene::Vertex& vertex) {

                if (slots_.empty() ||
                    (unique_count_ + 1) * 10 > slots_.size() * 7) {
                    rehash(slots_.empty() ? 16 : slots_.size() * 2);
                }

                const std::size_t slot = find_slot(vertex, slots_);
                const uint32_t existing = slots_[slot];
                if (existing != StaticScene::INVALID_INDEX) {
                    return existing;
                }

                const uint32_t local_index = checked_u32(
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
                const std::vector<uint32_t>& slots) const noexcept {

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
                std::vector<uint32_t> replacement(
                    capacity,
                    StaticScene::INVALID_INDEX);
                for (uint32_t local_index = 0;
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
            uint32_t vertex_offset_ = 0;
            uint32_t unique_count_ = 0;
            std::vector<uint32_t> slots_;
        };
        struct FaceGroup {
            std::string material_path;
            pxr::UsdShadeMaterial material;
            std::vector<uint32_t> faces;
        };

        struct UvData {
            pxr::TfToken interpolation;
            pxr::VtVec2fArray values;
        };

		struct PendingTriangleBoolStream {
			std::string name;
			pxr::TfToken interpolation;
			pxr::VtBoolArray source_values;
			std::vector<std::uint8_t> cooked_values;
		};

		struct PendingCornerFloatStream {
			std::string name;
			pxr::TfToken interpolation;
			pxr::VtFloatArray source_values;
			std::vector<float> cooked_values;
		};

		struct PendingCornerColor3Stream {
			std::string name;
			pxr::TfToken interpolation;
			pxr::VtVec3fArray source_values;
			std::vector<DirectX::XMFLOAT3> cooked_values;
		};

		struct PendingCornerTexcoord2Stream {
			std::string name;
			pxr::TfToken interpolation;
			pxr::VtVec2fArray source_values;
			std::vector<DirectX::XMFLOAT2> cooked_values;
		};

		struct PendingMeshAuxData {
			std::optional<PendingTriangleBoolStream> triangle_bool;
			std::vector<PendingCornerFloatStream> corner_floats;
			std::vector<PendingCornerColor3Stream> corner_colors;
			std::vector<PendingCornerTexcoord2Stream> corner_texcoords;
		};

    } // namespace

    class UsdMeshBuilder::Impl final {
    public:
        Impl(
            SceneSpace& converter,
            StaticSceneDataBuilder& scene_builder,
            UsdMaterialBuilder& material_builder)
            : converter_(converter),
              scene_builder_(scene_builder),
              material_builder_(material_builder),
              result_(&scene_builder.storage()) {}

        [[nodiscard]] MeshId build(const pxr::UsdPrim& prim) {
            return MeshId{cook_mesh(prim)};
        }

    private:
        [[nodiscard]] uint32_t add_string(std::string_view value) {
            return scene_builder_.intern_string(value).value();
        }

			[[nodiscard]] static StaticScene::EnumAttributeInterpolation
			attribute_interpolation(const pxr::TfToken& interpolation) {

				if (interpolation.IsEmpty() ||
					interpolation == pxr::UsdGeomTokens->constant) {
					return StaticScene::EnumAttributeInterpolation::CONSTANT;
				}
				if (interpolation == pxr::UsdGeomTokens->uniform) {
					return StaticScene::EnumAttributeInterpolation::UNIFORM;
				}
				if (interpolation == pxr::UsdGeomTokens->vertex) {
					return StaticScene::EnumAttributeInterpolation::VERTEX;
				}
				if (interpolation == pxr::UsdGeomTokens->varying) {
					return StaticScene::EnumAttributeInterpolation::VARYING;
				}
				if (interpolation == pxr::UsdGeomTokens->faceVarying) {
					return StaticScene::EnumAttributeInterpolation::FACE_VARYING;
				}
				fail(
					"Unsupported auxiliary primvar interpolation: ",
					interpolation.GetString());
			}

			[[nodiscard]] PendingMeshAuxData read_mesh_aux_data(
				const pxr::UsdPrim& prim) const {

				PendingMeshAuxData result;
				for (const auto& primvar :
					pxr::UsdGeomPrimvarsAPI{prim}.GetPrimvars()) {
					if (!primvar.GetAttr().HasAuthoredValueOpinion()) {
						continue;
					}
					const auto name = primvar.GetPrimvarName().GetString();
					if (name == "st") {
						continue;
					}

					const auto interpolation = primvar.GetInterpolation();
					if (name == "sharp_face") {
						PendingTriangleBoolStream stream;
						stream.name = name;
						stream.interpolation = interpolation;
						if (interpolation != pxr::UsdGeomTokens->uniform ||
							!primvar.ComputeFlattened(&stream.source_values)) {
							fail("sharp_face must be a uniform bool primvar.");
						}
						result.triangle_bool = std::move(stream);
						continue;
					}

					if (name == "Moss" || name == "Group" ||
						name == "Group_001") {
						PendingCornerFloatStream stream;
						stream.name = name;
						stream.interpolation = interpolation;
						if (!primvar.ComputeFlattened(&stream.source_values)) {
							fail("Unable to flatten float auxiliary primvar: ", name);
						}
						result.corner_floats.push_back(std::move(stream));
						continue;
					}

					if (name == "Color" || name == "Color_001" ||
						name == "Col") {
						PendingCornerColor3Stream stream;
						stream.name = name;
						stream.interpolation = interpolation;
						if (!primvar.ComputeFlattened(&stream.source_values)) {
							fail("Unable to flatten color auxiliary primvar: ", name);
						}
						result.corner_colors.push_back(std::move(stream));
						continue;
					}

					if (name == "UV1") {
						PendingCornerTexcoord2Stream stream;
						stream.name = name;
						stream.interpolation = interpolation;
						if (!primvar.ComputeFlattened(&stream.source_values)) {
							fail("Unable to flatten UV1 auxiliary primvar.");
						}
						result.corner_texcoords.push_back(std::move(stream));
						continue;
					}

					fail(
						"Unsupported authored mesh primvar: ",
						prim.GetPath().GetString(), ".", name);
				}
				return result;
			}

			void append_mesh_aux_data(
				uint32_t mesh,
				PendingMeshAuxData& data) {

				if (data.triangle_bool) {
					auto& stream = *data.triangle_bool;
					StaticScene::TriangleBoolStream destination;
					destination.mesh = mesh;
					destination.name = add_string(stream.name);
					destination.value_offset = checked_u32(
						result_->triangle_bool_values.size(),
						"Triangle bool stream offset");
					destination.value_count = checked_u32(
						stream.cooked_values.size(),
						"Triangle bool stream count");
					result_->triangle_bool_values.insert(
						result_->triangle_bool_values.end(),
						stream.cooked_values.begin(),
						stream.cooked_values.end());
					result_->triangle_bool_streams.push_back(destination);
				}

				for (auto& stream : data.corner_floats) {
					StaticScene::CornerFloatStream destination;
					destination.mesh = mesh;
					destination.name = add_string(stream.name);
					destination.source_interpolation =
						attribute_interpolation(stream.interpolation);
					destination.value_offset = checked_u32(
						result_->corner_float_values.size(),
						"Corner float stream offset");
					destination.value_count = checked_u32(
						stream.cooked_values.size(),
						"Corner float stream count");
					result_->corner_float_values.insert(
						result_->corner_float_values.end(),
						stream.cooked_values.begin(),
						stream.cooked_values.end());
					result_->corner_float_streams.push_back(destination);
				}

				for (auto& stream : data.corner_colors) {
					StaticScene::CornerColor3Stream destination;
					destination.mesh = mesh;
					destination.name = add_string(stream.name);
					destination.source_interpolation =
						attribute_interpolation(stream.interpolation);
					destination.value_offset = checked_u32(
						result_->corner_color3_values.size(),
						"Corner color stream offset");
					destination.value_count = checked_u32(
						stream.cooked_values.size(),
						"Corner color stream count");
					result_->corner_color3_values.insert(
						result_->corner_color3_values.end(),
						stream.cooked_values.begin(),
						stream.cooked_values.end());
					result_->corner_color3_streams.push_back(destination);
				}

				for (auto& stream : data.corner_texcoords) {
					StaticScene::CornerTexcoord2Stream destination;
					destination.mesh = mesh;
					destination.name = add_string(stream.name);
					destination.source_interpolation =
						attribute_interpolation(stream.interpolation);
					destination.value_offset = checked_u32(
						result_->corner_texcoord2_values.size(),
						"Corner texcoord stream offset");
					destination.value_count = checked_u32(
						stream.cooked_values.size(),
						"Corner texcoord stream count");
					result_->corner_texcoord2_values.insert(
						result_->corner_texcoord2_values.end(),
						stream.cooked_values.begin(),
						stream.cooked_values.end());
					result_->corner_texcoord2_streams.push_back(destination);
				}
			}

            [[nodiscard]] uint32_t cook_mesh(
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
                // SceneSpace swaps Y/Z, so the stage's default right-handed
                // winding must be reversed for StaticScene's CW front face.
                const bool reverse_winding =
                    orientation != pxr::UsdGeomTokens->leftHanded;
                const auto normal_interpolation =
                    mesh.GetNormalsInterpolation();
				auto auxiliary = read_mesh_aux_data(prim);

                auto groups = build_face_groups(prim, face_counts.size());

                StaticScene::Mesh destination;
                destination.name = add_string(prim.GetName().GetString());
                StaticScene::MeshLod lod0;
                lod0.submesh_offset = checked_u32(
                    result_->submeshes.size(),
                    "Mesh submesh offset");

                for (const auto& group : groups) {
                    const auto material = material_builder_.build(
                        group.material_path,
                        group.material);
					// st is part of the fixed StaticMesh contract, not something that
					// appears only when the current material happens to sample it.
					const auto uv = read_uv_data(prim, material.uv_primvar);

                    StaticScene::Submesh submesh;
                    submesh.name = add_string(path_leaf(group.material_path));
                    submesh.vertex_offset = checked_u32(
                        result_->vertices.size(),
                        "Submesh vertex offset");
                    submesh.index_offset = checked_u32(
                        result_->indices.size(),
                        "Submesh index offset");
                    submesh.material = material.id.value();
                    submesh.flags = material.submesh_flags(double_sided);
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

							if (auxiliary.triangle_bool) {
								auto& stream = *auxiliary.triangle_bool;
								const auto value_index = interpolation_index(
									stream.interpolation,
									face,
									corners[0],
									0);
								if (value_index >= stream.source_values.size()) {
									fail("Triangle bool primvar index is invalid: ", key);
								}
								stream.cooked_values.push_back(
									stream.source_values[value_index] ? 1u : 0u);
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

                                triangle[vertex].position =
                                    converter_.position_to_meters(
                                    points[static_cast<std::size_t>(point_index)]);
                                if (!normals.empty()) {
                                    const auto normal_index = interpolation_index(
                                        normal_interpolation,
                                        face,
                                        corner,
                                        static_cast<uint32_t>(point_index));
                                    if (normal_index >= normals.size()) {
                                        fail("Mesh normal index is invalid: ", key);
                                    }
                                    triangle[vertex].normal =
                                        converter_.direction_to_target(
                                        normals[normal_index]);
                                }
                                if (!uv.values.empty()) {
                                    const auto uv_index = interpolation_index(
                                        uv.interpolation,
                                        face,
                                        corner,
                                        static_cast<uint32_t>(point_index));
                                    if (uv_index >= uv.values.size()) {
                                        fail("Mesh UV index is invalid: ", key);
                                    }
                                    const auto value = uv.values[uv_index];
                                    triangle[vertex].uv = {
                                        value[0],
                                        1.0f - value[1]
                                    };
                                }

								for (auto& stream : auxiliary.corner_floats) {
									const auto value_index = interpolation_index(
										stream.interpolation,
										face,
										corner,
										static_cast<uint32_t>(point_index));
									if (value_index >= stream.source_values.size()) {
										fail("Float primvar index is invalid: ", key);
									}
									stream.cooked_values.push_back(
										stream.source_values[value_index]);
								}
								for (auto& stream : auxiliary.corner_colors) {
									const auto value_index = interpolation_index(
										stream.interpolation,
										face,
										corner,
										static_cast<uint32_t>(point_index));
									if (value_index >= stream.source_values.size()) {
										fail("Color primvar index is invalid: ", key);
									}
									const auto value = stream.source_values[value_index];
									stream.cooked_values.push_back({
										value[0], value[1], value[2]});
								}
								for (auto& stream : auxiliary.corner_texcoords) {
									const auto value_index = interpolation_index(
										stream.interpolation,
										face,
										corner,
										static_cast<uint32_t>(point_index));
									if (value_index >= stream.source_values.size()) {
										fail("Texcoord primvar index is invalid: ", key);
									}
									const auto value = stream.source_values[value_index];
									stream.cooked_values.push_back({
										value[0], 1.0f - value[1]});
								}
                            }

                            normalize_triangle_normals(triangle);
                            if (result_->info.vertex_count_before_indexing >
                                std::numeric_limits<uint64_t>::max() -
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
                    ++lod0.submesh_count;
                }

                if (lod0.submesh_count == 0) {
                    fail("Mesh produced no triangles: ", key);
                }

                destination.lod_offset = checked_u32(
                    result_->mesh_lods.size(),
                    "Mesh LOD offset");
                destination.lod_count = 1;
                result_->mesh_lods.push_back(lod0);

                const auto index = checked_u32(
                    result_->meshes.size(),
                    "Mesh index");
                result_->meshes.push_back(destination);
				append_mesh_aux_data(index, auxiliary);
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
                for (uint32_t face = 0; face < face_count; ++face) {
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
                uint32_t point) {

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


        SceneSpace& converter_;
        StaticSceneDataBuilder& scene_builder_;
        UsdMaterialBuilder& material_builder_;
        StaticScene* result_ = nullptr;
        std::unordered_map<std::string, uint32_t> mesh_cache_;
    };

    UsdMeshBuilder::UsdMeshBuilder(
        SceneSpace& converter,
        StaticSceneDataBuilder& scene_builder,
        UsdMaterialBuilder& material_builder)
        : impl_(std::make_unique<Impl>(
            converter, scene_builder, material_builder)) {}

    UsdMeshBuilder::~UsdMeshBuilder() = default;

    MeshId UsdMeshBuilder::build(const pxr::UsdPrim& prim) {
        return impl_->build(prim);
    }

} // namespace fjr::cooker::internal
