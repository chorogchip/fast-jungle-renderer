#include "FastJungle/scene/StaticSceneValidation.hpp"

#include "FastJungle/core/util/Logger.hpp"

#include <array>
#include <cmath>

namespace fjr::scene {

    void StaticSceneValidator::validate(const StaticScene& scene) {
        validate(scene, scene.texture_data.size());
    }

    void StaticSceneValidator::validate(
        const StaticScene& scene,
        std::uint64_t texture_payload_size) {

        if (scene.strings.empty() || scene.strings.front() != '\0') {
            log::Logger::g_logger
                << "Invalid StaticScene string table.\n";
            log::Logger::g_logger.abort();
        }

        for (const auto& texture : scene.textures) {
            require_range(
                texture.mip_offset,
                texture.mip_count,
                scene.texture_mips.size(),
                "texture mip");
            require_range(
                texture.data_byte_offset,
                texture.data_size,
                texture_payload_size,
                "texture data",
                StaticScene::INVALID_INDEX_64);

            for (std::uint32_t index = 0;
                 index < texture.mip_count;
                 ++index) {
                const auto& mip = scene.texture_mips[
                    texture.mip_offset + index];
                require_range(
                    mip.data_byte_offset_local,
                    mip.slice_pitch,
                    texture.data_size,
                    "texture mip data",
                    StaticScene::INVALID_INDEX_64);
            }
        }

        for (const auto& binding : scene.texture_bindings) {
            require_index(
                binding.texture,
                scene.textures.size(),
                "texture binding texture");
            require_index(
                binding.sampler,
                scene.samplers.size(),
                "texture binding sampler");
        }

        const auto validate_optional_binding = [&scene](
            std::uint32_t binding,
            std::string_view subject) {
            if (binding != StaticScene::INVALID_INDEX) {
                require_index(
                    binding,
                    scene.texture_bindings.size(),
                    subject);
            }
        };
        for (const auto& material : scene.materials) {
            validate_optional_binding(
                material.texture_binding_base_color,
                "material base-color binding");
            validate_optional_binding(
                material.texture_binding_normal,
                "material normal binding");
            validate_optional_binding(
                material.texture_binding_roughness,
                "material roughness binding");
			validate_optional_binding(
				material.texture_binding_metallic,
				"material metallic binding");
            validate_optional_binding(
                material.texture_binding_opacity,
                "material opacity binding");
            validate_optional_binding(
                material.texture_binding_emissive,
                "material emissive binding");
        }

        for (const auto& submesh : scene.submeshes) {
            require_range(
                submesh.vertex_offset,
                submesh.vertex_count,
                scene.vertices.size(),
                "submesh vertex");
            require_range(
                submesh.index_offset,
                submesh.index_count,
                scene.indices.size(),
                "submesh index");
            if (submesh.material != StaticScene::INVALID_INDEX) {
                require_index(
                    submesh.material,
                    scene.materials.size(),
                    "submesh material");
            }
            if (submesh.index_count == 0 || submesh.index_count % 3 != 0) {
                log::Logger::g_logger
                    << "Invalid StaticScene submesh triangle count.\n";
                log::Logger::g_logger.abort();
            }
            for (std::uint32_t local_index = 0;
                 local_index < submesh.index_count;
                 ++local_index) {
                if (scene.indices[submesh.index_offset + local_index] >=
                    submesh.vertex_count) {
                    log::Logger::g_logger
                        << "Invalid StaticScene submesh local index.\n";
                    log::Logger::g_logger.abort();
                }
            }
        }

        for (const auto& mesh : scene.meshes) {
            require_range(
                mesh.lod_offset,
                mesh.lod_count,
                scene.mesh_lods.size(),
                "mesh LOD");
            if (mesh.lod_count == 0) {
                log::Logger::g_logger
                    << "Invalid StaticScene empty mesh LOD range.\n";
                log::Logger::g_logger.abort();
            }

            const auto& lod0 = scene.mesh_lods[mesh.lod_offset];
            require_range(
                lod0.submesh_offset,
                lod0.submesh_count,
                scene.submeshes.size(),
                "mesh LOD0 submesh");
            if (lod0.submesh_count == 0 || lod0.max_deviation != 0.0f) {
                log::Logger::g_logger
                    << "Invalid StaticScene mesh LOD0.\n";
                log::Logger::g_logger.abort();
            }

            float previous_deviation = 0.0f;
            for (std::uint32_t local_lod = 0;
                 local_lod < mesh.lod_count;
                 ++local_lod) {
                const auto& lod = scene.mesh_lods[mesh.lod_offset + local_lod];
                require_range(
                    lod.submesh_offset,
                    lod.submesh_count,
                    scene.submeshes.size(),
                    "mesh LOD submesh");
                if (lod.submesh_count != lod0.submesh_count ||
                    !std::isfinite(lod.max_deviation) ||
                    lod.max_deviation < previous_deviation ||
					lod.reserved != 0) {
                    log::Logger::g_logger
                        << "Invalid StaticScene mesh LOD contract.\n";
                    log::Logger::g_logger.abort();
                }

                for (std::uint32_t local_submesh = 0;
                     local_submesh < lod.submesh_count;
                     ++local_submesh) {
                    const auto& base = scene.submeshes[
                        lod0.submesh_offset + local_submesh];
                    const auto& candidate = scene.submeshes[
                        lod.submesh_offset + local_submesh];
                    if (candidate.name != base.name ||
                        candidate.vertex_offset != base.vertex_offset ||
                        candidate.vertex_count != base.vertex_count ||
                        candidate.material != base.material ||
                        candidate.flags != base.flags) {
                        log::Logger::g_logger
                            << "Invalid StaticScene mesh LOD submesh contract.\n";
                        log::Logger::g_logger.abort();
                    }
					if (local_lod > 0) {
						const auto& previous_lod = scene.mesh_lods[
							mesh.lod_offset + local_lod - 1];
						const auto& previous_submesh = scene.submeshes[
							previous_lod.submesh_offset + local_submesh];
						if (candidate.index_count > previous_submesh.index_count) {
							log::Logger::g_logger
								<< "Invalid StaticScene increasing LOD index count.\n";
							log::Logger::g_logger.abort();
						}
					}
                }
                previous_deviation = lod.max_deviation;
            }
        }

		const auto lod0_corner_count = [&scene](std::uint32_t mesh_index) {
			const auto& mesh = scene.meshes[mesh_index];
			const auto& lod0 = scene.mesh_lods[mesh.lod_offset];
			std::uint64_t count = 0;
			for (std::uint32_t local = 0; local < lod0.submesh_count; ++local) {
				count += scene.submeshes[lod0.submesh_offset + local].index_count;
			}
			return count;
		};

		for (const auto& stream : scene.triangle_bool_streams) {
			require_index(stream.mesh, scene.meshes.size(), "triangle bool mesh");
			require_range(
				stream.value_offset,
				stream.value_count,
				scene.triangle_bool_values.size(),
				"triangle bool value");
			if (stream.value_count != lod0_corner_count(stream.mesh) / 3) {
				log::Logger::g_logger
					<< "Invalid StaticScene LOD0 triangle bool count.\n";
				log::Logger::g_logger.abort();
			}
		}

		auto validate_corner_stream = [&scene, &lod0_corner_count](
			const auto& stream,
			std::uint64_t value_size,
			std::string_view subject) {

			require_index(stream.mesh, scene.meshes.size(), subject);
			require_range(
				stream.value_offset,
				stream.value_count,
				value_size,
				subject);
			if (stream.value_count != lod0_corner_count(stream.mesh)) {
				log::Logger::g_logger
					<< "Invalid StaticScene LOD0 corner stream count.\n";
				log::Logger::g_logger.abort();
			}
		};
		for (const auto& stream : scene.corner_float_streams) {
			validate_corner_stream(
				stream, scene.corner_float_values.size(), "corner float stream");
		}
		for (const auto& stream : scene.corner_color3_streams) {
			validate_corner_stream(
				stream, scene.corner_color3_values.size(), "corner color stream");
		}
		for (const auto& stream : scene.corner_texcoord2_streams) {
			validate_corner_stream(
				stream, scene.corner_texcoord2_values.size(), "corner texcoord stream");
		}

		std::uint64_t expected_instance_offset = 0;
		for (const auto& batch : scene.point_batches) {
			require_index(
				batch.mesh,
				scene.meshes.size(),
				"point batch");
			const auto category =
				static_cast<std::size_t>(batch.category);
			if (category >= static_cast<std::size_t>(
				StaticScene::EnumPointCategory::COUNT)) {

				log::Logger::g_logger
					<< "Point batch category is invalid.\n";
				log::Logger::g_logger.abort();
			}
			require_range(
				batch.instances.offset,
				batch.instances.count,
				scene.point_instances.size(),
				"point batch instance");
			if (batch.instances.count == 0 ||
				batch.instances.offset != expected_instance_offset) {

				log::Logger::g_logger
					<< "Point batch instances are not contiguous.\n";
				log::Logger::g_logger.abort();
			}
			expected_instance_offset += batch.instances.count;
		}
		if (expected_instance_offset != scene.point_instances.size()) {

			log::Logger::g_logger
				<< "Point batches do not cover all point data.\n";
			log::Logger::g_logger.abort();
		}

		for (const auto& instance : scene.static_mesh_instances) {
			require_index(instance.mesh, scene.meshes.size(), "static mesh instance");
		}
        if (scene.environment_light.texture != StaticScene::INVALID_INDEX) {
            require_index(
                scene.environment_light.texture,
                scene.textures.size(),
                "environment light texture");
        }
    }

    void StaticSceneValidator::require_index(
        std::uint64_t index,
        std::uint64_t size,
        std::string_view subject) {

        if (index < size) {
            return;
        }
        log::Logger::g_logger
            << "Invalid StaticScene " << subject
            << " index.\n";
        log::Logger::g_logger.abort();
    }

    void StaticSceneValidator::require_range(
        std::uint64_t offset,
        std::uint64_t count,
        std::uint64_t size,
        std::string_view subject,
        std::uint64_t invalid_offset) {

        if (count == 0 && offset == invalid_offset) {
            return;
        }
        if (offset <= size && count <= size - offset) {
            return;
        }
        log::Logger::g_logger
            << "Invalid StaticScene " << subject
            << " range.\n";
        log::Logger::g_logger.abort();
    }

} // namespace fjr::scene
