#include "FastJungle/cooker/JungleSceneFile.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace fjr::scene {

    namespace {

        using Scene = cooker::JungleScene;

        constexpr std::array<char, 8> FILE_MAGIC{
            'F', 'J', 'S', 'C', 'E', 'N', 'E', '\0'
        };
        constexpr std::uint32_t FILE_HEADER_SIZE = 40;
        constexpr std::uint32_t ENDIAN_MARKER = 0x01020304u;
        constexpr std::uint64_t FNV_OFFSET_BASIS = 14695981039346656037ull;
        constexpr std::uint64_t FNV_PRIME = 1099511628211ull;
        constexpr std::uint64_t MAX_FILE_SIZE = 16ull * 1024ull * 1024ull * 1024ull;
        constexpr std::uint64_t MAX_STRING_SIZE = 16ull * 1024ull * 1024ull;
        constexpr std::uint64_t MAX_ELEMENT_COUNT = 100'000'000ull;
        constexpr std::size_t STREAM_CHUNK_SIZE = 64ull * 1024ull * 1024ull;

        static_assert(std::endian::native == std::endian::little);
        static_assert(sizeof(float) == 4);
        static_assert(sizeof(double) == 8);
        static_assert(sizeof(Scene::Float2) == 8);
        static_assert(sizeof(Scene::Float3) == 12);
        static_assert(sizeof(Scene::Float4) == 16);
        static_assert(sizeof(Scene::Quaternion) == 16);
        static_assert(std::is_trivially_copyable_v<Scene::Float2>);
        static_assert(std::is_trivially_copyable_v<Scene::Float3>);
        static_assert(std::is_trivially_copyable_v<Scene::Float4>);
        static_assert(std::is_trivially_copyable_v<Scene::Quaternion>);

        [[noreturn]] void fail(const std::string& message) {
            throw std::runtime_error("Invalid .fjscene file: " + message);
        }

        void update_checksum(
            std::uint64_t& checksum,
            const std::byte* data,
            std::size_t size) noexcept {

            for (std::size_t i = 0; i < size; ++i) {
                checksum ^= std::to_integer<std::uint8_t>(data[i]);
                checksum *= FNV_PRIME;
            }
        }

        template<typename T>
        void write_file_scalar(std::ostream& stream, const T& value) {
            static_assert(std::is_trivially_copyable_v<T>);
            stream.write(
                reinterpret_cast<const char*>(&value),
                static_cast<std::streamsize>(sizeof(T)));
        }

        template<typename T>
        T read_file_scalar(std::istream& stream) {
            static_assert(std::is_trivially_copyable_v<T>);
            T value{};
            stream.read(
                reinterpret_cast<char*>(&value),
                static_cast<std::streamsize>(sizeof(T)));
            return value;
        }

        class BinaryOutput {
        public:
            explicit BinaryOutput(std::ostream& stream)
                : stream_(stream) {
            }

            void write_bytes(const void* data, std::uint64_t size) {
                if (size > MAX_FILE_SIZE - size_) {
                    throw std::runtime_error(
                        "The .fjscene payload exceeds the supported size.");
                }

                const auto* bytes = static_cast<const std::byte*>(data);
                std::uint64_t remaining = size;
                while (remaining != 0) {
                    const auto chunk = static_cast<std::size_t>(
                        std::min<std::uint64_t>(remaining, STREAM_CHUNK_SIZE));
                    stream_.write(
                        reinterpret_cast<const char*>(bytes),
                        static_cast<std::streamsize>(chunk));
                    update_checksum(checksum_, bytes, chunk);
                    bytes += chunk;
                    remaining -= chunk;
                }
                size_ += size;
            }

            template<typename T>
            void write_scalar(const T& value) {
                static_assert(std::is_trivially_copyable_v<T>);
                write_bytes(&value, sizeof(T));
            }

            [[nodiscard]]
            std::uint64_t size() const noexcept {
                return size_;
            }

            [[nodiscard]]
            std::uint64_t checksum() const noexcept {
                return checksum_;
            }

        private:
            std::ostream& stream_;
            std::uint64_t size_ = 0;
            std::uint64_t checksum_ = FNV_OFFSET_BASIS;
        };

        class BinaryInput {
        public:
            BinaryInput(
                std::istream& stream,
                std::uint64_t payload_size)
                : stream_(stream),
                  remaining_(payload_size) {
            }

            void read_bytes(void* destination, std::uint64_t size) {
                if (size > remaining_) {
                    fail("a value extends beyond the payload boundary");
                }

                auto* bytes = static_cast<std::byte*>(destination);
                std::uint64_t unread = size;
                while (unread != 0) {
                    const auto chunk = static_cast<std::size_t>(
                        std::min<std::uint64_t>(unread, STREAM_CHUNK_SIZE));
                    stream_.read(
                        reinterpret_cast<char*>(bytes),
                        static_cast<std::streamsize>(chunk));
                    update_checksum(checksum_, bytes, chunk);
                    bytes += chunk;
                    unread -= chunk;
                }
                remaining_ -= size;
            }

            template<typename T>
            [[nodiscard]] T read_scalar() {
                static_assert(std::is_trivially_copyable_v<T>);
                T value{};
                read_bytes(&value, sizeof(T));
                return value;
            }

            [[nodiscard]]
            std::uint64_t remaining() const noexcept {
                return remaining_;
            }

            [[nodiscard]]
            std::uint64_t checksum() const noexcept {
                return checksum_;
            }

        private:
            std::istream& stream_;
            std::uint64_t remaining_ = 0;
            std::uint64_t checksum_ = FNV_OFFSET_BASIS;
        };

        void write_count(BinaryOutput& output, std::size_t count) {
            output.write_scalar(static_cast<std::uint64_t>(count));
        }

        std::size_t read_count(BinaryInput& input) {
            const auto count = input.read_scalar<std::uint64_t>();
            if (count > MAX_ELEMENT_COUNT ||
                count > std::numeric_limits<std::size_t>::max()) {
                fail("a container element count is unreasonable");
            }
            return static_cast<std::size_t>(count);
        }

        void write_string(BinaryOutput& output, const std::string& value) {
            write_count(output, value.size());
            output.write_bytes(value.data(), value.size());
        }

        std::string read_string(BinaryInput& input) {
            const auto size = input.read_scalar<std::uint64_t>();
            if (size > MAX_STRING_SIZE || size > input.remaining()) {
                fail("a string size is invalid");
            }
            std::string result(static_cast<std::size_t>(size), '\0');
            input.read_bytes(result.data(), size);
            return result;
        }

        void write_bool(BinaryOutput& output, bool value) {
            output.write_scalar(static_cast<std::uint8_t>(value ? 1u : 0u));
        }

        bool read_bool(BinaryInput& input) {
            const auto value = input.read_scalar<std::uint8_t>();
            if (value > 1u) {
                fail("a Boolean value is neither zero nor one");
            }
            return value != 0;
        }

        template<typename Enum>
        void write_enum(BinaryOutput& output, Enum value) {
            using Storage = std::underlying_type_t<Enum>;
            output.write_scalar(static_cast<Storage>(value));
        }

        template<typename Enum>
        Enum read_enum(BinaryInput& input, Enum maximum) {
            using Storage = std::underlying_type_t<Enum>;
            const auto value = input.read_scalar<Storage>();
            if (value > static_cast<Storage>(maximum)) {
                fail("an enumeration value is out of range");
            }
            return static_cast<Enum>(value);
        }

        template<typename T>
        void write_trivial_vector(
            BinaryOutput& output,
            const std::vector<T>& values) {

            static_assert(std::is_trivially_copyable_v<T>);
            write_count(output, values.size());
            if (values.size() >
                std::numeric_limits<std::uint64_t>::max() / sizeof(T)) {
                throw std::runtime_error(
                    "A .fjscene array is too large to serialize.");
            }
            output.write_bytes(
                values.data(),
                static_cast<std::uint64_t>(values.size()) * sizeof(T));
        }

        template<typename T>
        std::vector<T> read_trivial_vector(BinaryInput& input) {
            static_assert(std::is_trivially_copyable_v<T>);
            const auto count = read_count(input);
            if (count > std::numeric_limits<std::uint64_t>::max() / sizeof(T)) {
                fail("an array byte size overflows");
            }
            const auto byte_size = static_cast<std::uint64_t>(count) * sizeof(T);
            if (byte_size > input.remaining()) {
                fail("an array extends beyond the payload boundary");
            }
            std::vector<T> result(count);
            input.read_bytes(result.data(), byte_size);
            return result;
        }

        template<typename T, typename Writer>
        void write_objects(
            BinaryOutput& output,
            const std::vector<T>& values,
            Writer writer) {

            write_count(output, values.size());
            for (const auto& value : values) {
                writer(output, value);
            }
        }

        template<typename T, typename Reader>
        std::vector<T> read_objects(
            BinaryInput& input,
            Reader reader) {

            const auto count = read_count(input);
            if (count > input.remaining()) {
                fail("an object count cannot fit in the remaining payload");
            }
            std::vector<T> result;
            result.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                result.push_back(reader(input));
            }
            return result;
        }

        void write_strings(
            BinaryOutput& output,
            const std::vector<std::string>& values) {

            write_objects(
                output,
                values,
                [](BinaryOutput& destination, const std::string& value) {
                    write_string(destination, value);
                });
        }

        std::vector<std::string> read_strings(BinaryInput& input) {
            return read_objects<std::string>(
                input,
                [](BinaryInput& source) {
                    return read_string(source);
                });
        }

        void write_matrix(
            BinaryOutput& output,
            const Scene::Matrix4x4& matrix) {

            output.write_bytes(
                matrix.values.data(),
                matrix.values.size() * sizeof(double));
        }

        Scene::Matrix4x4 read_matrix(BinaryInput& input) {
            Scene::Matrix4x4 result;
            input.read_bytes(
                result.values.data(),
                result.values.size() * sizeof(double));
            return result;
        }

        void write_asset_reference(
            BinaryOutput& output,
            const Scene::AssetReference& asset) {

            write_string(output, asset.authored_path);
            write_string(output, asset.resolved_path);
            write_bool(output, asset.resolved_file_exists);
        }

        Scene::AssetReference read_asset_reference(BinaryInput& input) {
            Scene::AssetReference result;
            result.authored_path = read_string(input);
            result.resolved_path = read_string(input);
            result.resolved_file_exists = read_bool(input);
            return result;
        }

        void write_primvar(
            BinaryOutput& output,
            const Scene::Primvar& primvar) {

            write_string(output, primvar.name);
            write_string(output, primvar.type_name);
            write_string(output, primvar.interpolation);
            write_enum(output, primvar.storage);
            switch (primvar.storage) {
            case Scene::PrimvarStorage::Boolean:
                write_trivial_vector(
                    output,
                    std::get<std::vector<std::uint8_t>>(primvar.data));
                break;
            case Scene::PrimvarStorage::Float:
                write_trivial_vector(
                    output,
                    std::get<std::vector<float>>(primvar.data));
                break;
            case Scene::PrimvarStorage::Float2:
                write_trivial_vector(
                    output,
                    std::get<std::vector<Scene::Float2>>(primvar.data));
                break;
            case Scene::PrimvarStorage::Float3:
                write_trivial_vector(
                    output,
                    std::get<std::vector<Scene::Float3>>(primvar.data));
                break;
            }
            write_trivial_vector(output, primvar.indices);
        }

        Scene::Primvar read_primvar(BinaryInput& input) {
            Scene::Primvar result;
            result.name = read_string(input);
            result.type_name = read_string(input);
            result.interpolation = read_string(input);
            result.storage = read_enum(
                input,
                Scene::PrimvarStorage::Float3);
            switch (result.storage) {
            case Scene::PrimvarStorage::Boolean:
                result.data = read_trivial_vector<std::uint8_t>(input);
                break;
            case Scene::PrimvarStorage::Float:
                result.data = read_trivial_vector<float>(input);
                break;
            case Scene::PrimvarStorage::Float2:
                result.data = read_trivial_vector<Scene::Float2>(input);
                break;
            case Scene::PrimvarStorage::Float3:
                result.data = read_trivial_vector<Scene::Float3>(input);
                break;
            }
            result.indices = read_trivial_vector<std::int32_t>(input);
            return result;
        }

        void write_source_layer(
            BinaryOutput& output,
            const Scene::SourceLayer& layer) {

            write_string(output, layer.identifier);
            write_string(output, layer.resolved_path);
            write_bool(output, layer.is_root);
        }

        Scene::SourceLayer read_source_layer(BinaryInput& input) {
            Scene::SourceLayer result;
            result.identifier = read_string(input);
            result.resolved_path = read_string(input);
            result.is_root = read_bool(input);
            return result;
        }

        void write_node(BinaryOutput& output, const Scene::Node& node) {
            write_string(output, node.path);
            write_string(output, node.name);
            write_string(output, node.usd_type_name);
            write_string(output, node.purpose);
            write_string(output, node.native_prototype_path);
            output.write_scalar(node.parent);
            output.write_scalar(node.payload);
            write_enum(output, node.prim_kind);
            write_enum(output, node.object_kind);
            output.write_scalar(node.flags);
            write_matrix(output, node.local_transform);
        }

        Scene::Node read_node(BinaryInput& input) {
            Scene::Node result;
            result.path = read_string(input);
            result.name = read_string(input);
            result.usd_type_name = read_string(input);
            result.purpose = read_string(input);
            result.native_prototype_path = read_string(input);
            result.parent = input.read_scalar<std::uint32_t>();
            result.payload = input.read_scalar<std::uint32_t>();
            result.prim_kind = read_enum(input, Scene::PrimKind::Light);
            result.object_kind = read_enum(input, Scene::ObjectKind::Camera);
            result.flags = input.read_scalar<std::uint32_t>();
            result.local_transform = read_matrix(input);
            return result;
        }

        void write_mesh(BinaryOutput& output, const Scene::Mesh& mesh) {
            write_string(output, mesh.prim_path);
            write_string(output, mesh.orientation);
            write_string(output, mesh.subdivision_scheme);
            write_string(output, mesh.normals_interpolation);
            write_string(output, mesh.material_path);
            write_bool(output, mesh.double_sided);
            write_trivial_vector(output, mesh.points);
            write_trivial_vector(output, mesh.face_vertex_counts);
            write_trivial_vector(output, mesh.face_vertex_indices);
            write_trivial_vector(output, mesh.normals);
            write_trivial_vector(output, mesh.hole_indices);
            write_objects(output, mesh.primvars, write_primvar);
        }

        Scene::Mesh read_mesh(BinaryInput& input) {
            Scene::Mesh result;
            result.prim_path = read_string(input);
            result.orientation = read_string(input);
            result.subdivision_scheme = read_string(input);
            result.normals_interpolation = read_string(input);
            result.material_path = read_string(input);
            result.double_sided = read_bool(input);
            result.points = read_trivial_vector<Scene::Float3>(input);
            result.face_vertex_counts = read_trivial_vector<std::int32_t>(input);
            result.face_vertex_indices = read_trivial_vector<std::int32_t>(input);
            result.normals = read_trivial_vector<Scene::Float3>(input);
            result.hole_indices = read_trivial_vector<std::int32_t>(input);
            result.primvars = read_objects<Scene::Primvar>(input, read_primvar);
            return result;
        }

        void write_mesh_subset(
            BinaryOutput& output,
            const Scene::MeshSubset& subset) {

            write_string(output, subset.prim_path);
            write_string(output, subset.mesh_path);
            write_string(output, subset.element_type);
            write_string(output, subset.family_name);
            write_string(output, subset.family_type);
            write_string(output, subset.material_path);
            write_trivial_vector(output, subset.indices);
        }

        Scene::MeshSubset read_mesh_subset(BinaryInput& input) {
            Scene::MeshSubset result;
            result.prim_path = read_string(input);
            result.mesh_path = read_string(input);
            result.element_type = read_string(input);
            result.family_name = read_string(input);
            result.family_type = read_string(input);
            result.material_path = read_string(input);
            result.indices = read_trivial_vector<std::int32_t>(input);
            return result;
        }

        void write_point_instancer(
            BinaryOutput& output,
            const Scene::PointInstancer& instancer) {

            write_string(output, instancer.prim_path);
            write_enum(output, instancer.object_kind);
            write_strings(output, instancer.prototype_paths);
            write_trivial_vector(output, instancer.prototype_indices);
            write_trivial_vector(output, instancer.positions);
            write_trivial_vector(output, instancer.orientations);
            write_trivial_vector(output, instancer.scales);
            write_trivial_vector(output, instancer.velocities);
            write_trivial_vector(output, instancer.accelerations);
            write_trivial_vector(output, instancer.angular_velocities);
            write_trivial_vector(output, instancer.ids);
            write_trivial_vector(output, instancer.inactive_ids);
            write_trivial_vector(output, instancer.invisible_ids);
            write_objects(output, instancer.primvars, write_primvar);
        }

        Scene::PointInstancer read_point_instancer(BinaryInput& input) {
            Scene::PointInstancer result;
            result.prim_path = read_string(input);
            result.object_kind = read_enum(input, Scene::ObjectKind::Camera);
            result.prototype_paths = read_strings(input);
            result.prototype_indices = read_trivial_vector<std::int32_t>(input);
            result.positions = read_trivial_vector<Scene::Float3>(input);
            result.orientations = read_trivial_vector<Scene::Quaternion>(input);
            result.scales = read_trivial_vector<Scene::Float3>(input);
            result.velocities = read_trivial_vector<Scene::Float3>(input);
            result.accelerations = read_trivial_vector<Scene::Float3>(input);
            result.angular_velocities = read_trivial_vector<Scene::Float3>(input);
            result.ids = read_trivial_vector<std::int64_t>(input);
            result.inactive_ids = read_trivial_vector<std::int64_t>(input);
            result.invisible_ids = read_trivial_vector<std::int64_t>(input);
            result.primvars = read_objects<Scene::Primvar>(input, read_primvar);
            return result;
        }

        void write_native_instance(
            BinaryOutput& output,
            const Scene::NativeInstance& instance) {

            write_string(output, instance.prim_path);
            write_string(output, instance.prototype_path);
            write_enum(output, instance.object_kind);
        }

        Scene::NativeInstance read_native_instance(BinaryInput& input) {
            Scene::NativeInstance result;
            result.prim_path = read_string(input);
            result.prototype_path = read_string(input);
            result.object_kind = read_enum(input, Scene::ObjectKind::Camera);
            return result;
        }

        void write_shader_value(
            BinaryOutput& output,
            const Scene::ShaderValue& value) {

            write_string(output, value.type_name);
            write_string(output, value.unsupported_value);
            write_enum(output, value.kind);
            switch (value.kind) {
            case Scene::ShaderValueKind::Empty:
            case Scene::ShaderValueKind::Unsupported:
                if (!std::holds_alternative<std::monostate>(value.data)) {
                    throw std::runtime_error(
                        "A Jungle shader value has inconsistent storage.");
                }
                break;
            case Scene::ShaderValueKind::Float:
                output.write_scalar(std::get<float>(value.data));
                break;
            case Scene::ShaderValueKind::Float2:
                output.write_scalar(std::get<Scene::Float2>(value.data));
                break;
            case Scene::ShaderValueKind::Float3:
                output.write_scalar(std::get<Scene::Float3>(value.data));
                break;
            case Scene::ShaderValueKind::Float4:
                output.write_scalar(std::get<Scene::Float4>(value.data));
                break;
            case Scene::ShaderValueKind::Token:
            case Scene::ShaderValueKind::String:
                write_string(output, std::get<std::string>(value.data));
                break;
            case Scene::ShaderValueKind::Asset:
                write_asset_reference(
                    output,
                    std::get<Scene::AssetReference>(value.data));
                break;
            }
        }

        Scene::ShaderValue read_shader_value(BinaryInput& input) {
            Scene::ShaderValue result;
            result.type_name = read_string(input);
            result.unsupported_value = read_string(input);
            result.kind = read_enum(input, Scene::ShaderValueKind::Unsupported);
            switch (result.kind) {
            case Scene::ShaderValueKind::Empty:
            case Scene::ShaderValueKind::Unsupported:
                result.data = std::monostate{};
                break;
            case Scene::ShaderValueKind::Float:
                result.data = input.read_scalar<float>();
                break;
            case Scene::ShaderValueKind::Float2:
                result.data = input.read_scalar<Scene::Float2>();
                break;
            case Scene::ShaderValueKind::Float3:
                result.data = input.read_scalar<Scene::Float3>();
                break;
            case Scene::ShaderValueKind::Float4:
                result.data = input.read_scalar<Scene::Float4>();
                break;
            case Scene::ShaderValueKind::Token:
            case Scene::ShaderValueKind::String:
                result.data = read_string(input);
                break;
            case Scene::ShaderValueKind::Asset:
                result.data = read_asset_reference(input);
                break;
            }
            return result;
        }

        void write_shader_connection(
            BinaryOutput& output,
            const Scene::ShaderConnection& connection) {

            write_string(output, connection.source_prim_path);
            write_string(output, connection.source_name);
            write_bool(output, connection.source_is_output);
        }

        Scene::ShaderConnection read_shader_connection(BinaryInput& input) {
            Scene::ShaderConnection result;
            result.source_prim_path = read_string(input);
            result.source_name = read_string(input);
            result.source_is_output = read_bool(input);
            return result;
        }

        void write_shader_input(
            BinaryOutput& output,
            const Scene::ShaderInput& shader_input) {

            write_string(output, shader_input.name);
            write_shader_value(output, shader_input.value);
            write_objects(
                output,
                shader_input.connections,
                write_shader_connection);
            write_strings(output, shader_input.invalid_source_paths);
        }

        Scene::ShaderInput read_shader_input(BinaryInput& input) {
            Scene::ShaderInput result;
            result.name = read_string(input);
            result.value = read_shader_value(input);
            result.connections = read_objects<Scene::ShaderConnection>(
                input,
                read_shader_connection);
            result.invalid_source_paths = read_strings(input);
            return result;
        }

        void write_shader_output(
            BinaryOutput& output,
            const Scene::ShaderOutput& shader_output) {

            write_string(output, shader_output.name);
            write_string(output, shader_output.type_name);
            write_objects(
                output,
                shader_output.connections,
                write_shader_connection);
            write_strings(output, shader_output.invalid_source_paths);
        }

        Scene::ShaderOutput read_shader_output(BinaryInput& input) {
            Scene::ShaderOutput result;
            result.name = read_string(input);
            result.type_name = read_string(input);
            result.connections = read_objects<Scene::ShaderConnection>(
                input,
                read_shader_connection);
            result.invalid_source_paths = read_strings(input);
            return result;
        }

        void write_shader_node(
            BinaryOutput& output,
            const Scene::ShaderNode& shader) {

            write_string(output, shader.prim_path);
            write_string(output, shader.shader_id);
            write_objects(output, shader.inputs, write_shader_input);
            write_objects(output, shader.outputs, write_shader_output);
        }

        Scene::ShaderNode read_shader_node(BinaryInput& input) {
            Scene::ShaderNode result;
            result.prim_path = read_string(input);
            result.shader_id = read_string(input);
            result.inputs = read_objects<Scene::ShaderInput>(
                input,
                read_shader_input);
            result.outputs = read_objects<Scene::ShaderOutput>(
                input,
                read_shader_output);
            return result;
        }

        void write_material(
            BinaryOutput& output,
            const Scene::Material& material) {

            write_string(output, material.prim_path);
            write_trivial_vector(output, material.shader_nodes);
            write_objects(output, material.outputs, write_shader_output);
        }

        Scene::Material read_material(BinaryInput& input) {
            Scene::Material result;
            result.prim_path = read_string(input);
            result.shader_nodes = read_trivial_vector<std::uint32_t>(input);
            result.outputs = read_objects<Scene::ShaderOutput>(
                input,
                read_shader_output);
            return result;
        }

        void write_camera(BinaryOutput& output, const Scene::Camera& camera) {
            write_string(output, camera.prim_path);
            write_string(output, camera.projection);
            output.write_scalar(camera.focal_length);
            output.write_scalar(camera.horizontal_aperture);
            output.write_scalar(camera.vertical_aperture);
            output.write_scalar(camera.horizontal_aperture_offset);
            output.write_scalar(camera.vertical_aperture_offset);
            output.write_scalar(camera.focus_distance);
            output.write_scalar(camera.f_stop);
            output.write_scalar(camera.clipping_range);
        }

        Scene::Camera read_camera(BinaryInput& input) {
            Scene::Camera result;
            result.prim_path = read_string(input);
            result.projection = read_string(input);
            result.focal_length = input.read_scalar<float>();
            result.horizontal_aperture = input.read_scalar<float>();
            result.vertical_aperture = input.read_scalar<float>();
            result.horizontal_aperture_offset = input.read_scalar<float>();
            result.vertical_aperture_offset = input.read_scalar<float>();
            result.focus_distance = input.read_scalar<float>();
            result.f_stop = input.read_scalar<float>();
            result.clipping_range = input.read_scalar<Scene::Float2>();
            return result;
        }

        void write_environment_light(
            BinaryOutput& output,
            const Scene::EnvironmentLight& light) {

            write_string(output, light.prim_path);
            output.write_scalar(light.color);
            output.write_scalar(light.intensity);
            output.write_scalar(light.exposure);
            write_asset_reference(output, light.texture);
        }

        Scene::EnvironmentLight read_environment_light(BinaryInput& input) {
            Scene::EnvironmentLight result;
            result.prim_path = read_string(input);
            result.color = input.read_scalar<Scene::Float3>();
            result.intensity = input.read_scalar<float>();
            result.exposure = input.read_scalar<float>();
            result.texture = read_asset_reference(input);
            return result;
        }

        void write_diagnostic(
            BinaryOutput& output,
            const Scene::Diagnostic& diagnostic) {

            write_enum(output, diagnostic.severity);
            write_string(output, diagnostic.subject);
            write_string(output, diagnostic.message);
        }

        Scene::Diagnostic read_diagnostic(BinaryInput& input) {
            Scene::Diagnostic result;
            result.severity = read_enum(
                input,
                Scene::DiagnosticSeverity::Error);
            result.subject = read_string(input);
            result.message = read_string(input);
            return result;
        }

        void write_statistics(
            BinaryOutput& output,
            const Scene::Statistics& statistics) {

            output.write_scalar(statistics.composed_prim_count);
            output.write_scalar(statistics.composed_mesh_count);
            output.write_scalar(statistics.composed_mesh_subset_count);
            output.write_scalar(statistics.composed_material_count);
            output.write_scalar(statistics.composed_shader_count);
            output.write_scalar(statistics.composed_camera_count);
            output.write_scalar(statistics.native_prototype_count);
            output.write_scalar(statistics.exact_origin_instance_count);
            output.write_scalar(statistics.time_sampled_attribute_count);
            output.write_scalar(statistics.time_sample_count);
        }

        Scene::Statistics read_statistics(BinaryInput& input) {
            Scene::Statistics result;
            result.composed_prim_count = input.read_scalar<std::uint64_t>();
            result.composed_mesh_count = input.read_scalar<std::uint64_t>();
            result.composed_mesh_subset_count =
                input.read_scalar<std::uint64_t>();
            result.composed_material_count = input.read_scalar<std::uint64_t>();
            result.composed_shader_count = input.read_scalar<std::uint64_t>();
            result.composed_camera_count = input.read_scalar<std::uint64_t>();
            result.native_prototype_count = input.read_scalar<std::uint64_t>();
            result.exact_origin_instance_count =
                input.read_scalar<std::uint64_t>();
            result.time_sampled_attribute_count =
                input.read_scalar<std::uint64_t>();
            result.time_sample_count = input.read_scalar<std::uint64_t>();
            return result;
        }

        void write_scene(BinaryOutput& output, const Scene& scene) {
            write_string(output, scene.source_root);
            write_string(output, scene.up_axis);
            output.write_scalar(scene.meters_per_unit);
            output.write_scalar(scene.start_time_code);
            output.write_scalar(scene.end_time_code);
            write_statistics(output, scene.statistics);
            write_objects(output, scene.source_layers, write_source_layer);
            write_objects(output, scene.nodes, write_node);
            write_objects(output, scene.meshes, write_mesh);
            write_objects(output, scene.mesh_subsets, write_mesh_subset);
            write_objects(
                output,
                scene.point_instancers,
                write_point_instancer);
            write_objects(
                output,
                scene.native_instances,
                write_native_instance);
            write_objects(output, scene.shader_nodes, write_shader_node);
            write_objects(output, scene.materials, write_material);
            write_objects(output, scene.cameras, write_camera);
            write_objects(
                output,
                scene.environment_lights,
                write_environment_light);
            write_objects(
                output,
                scene.import_diagnostics,
                write_diagnostic);
        }

        Scene read_scene(BinaryInput& input) {
            Scene result;
            result.source_root = read_string(input);
            result.up_axis = read_string(input);
            result.meters_per_unit = input.read_scalar<double>();
            result.start_time_code = input.read_scalar<double>();
            result.end_time_code = input.read_scalar<double>();
            result.statistics = read_statistics(input);
            result.source_layers = read_objects<Scene::SourceLayer>(
                input,
                read_source_layer);
            result.nodes = read_objects<Scene::Node>(input, read_node);
            result.meshes = read_objects<Scene::Mesh>(input, read_mesh);
            result.mesh_subsets = read_objects<Scene::MeshSubset>(
                input,
                read_mesh_subset);
            result.point_instancers = read_objects<Scene::PointInstancer>(
                input,
                read_point_instancer);
            result.native_instances = read_objects<Scene::NativeInstance>(
                input,
                read_native_instance);
            result.shader_nodes = read_objects<Scene::ShaderNode>(
                input,
                read_shader_node);
            result.materials = read_objects<Scene::Material>(
                input,
                read_material);
            result.cameras = read_objects<Scene::Camera>(input, read_camera);
            result.environment_lights = read_objects<Scene::EnvironmentLight>(
                input,
                read_environment_light);
            result.import_diagnostics = read_objects<Scene::Diagnostic>(
                input,
                read_diagnostic);
            return result;
        }

        std::string display_path(const std::filesystem::path& path) {
            return path.generic_string();
        }

    } // namespace

    JungleSceneFile::WriteResult JungleSceneFile::write(
        const std::filesystem::path& path,
        const cooker::JungleScene& scene) {

        if (path.empty()) {
            throw std::runtime_error("The .fjscene output path is empty.");
        }

        try {
            const auto parent = path.parent_path();
            if (!parent.empty()) {
                std::filesystem::create_directories(parent);
            }

            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            stream.exceptions(std::ios::badbit | std::ios::failbit);

            stream.write(FILE_MAGIC.data(), FILE_MAGIC.size());
            write_file_scalar(stream, FORMAT_VERSION);
            write_file_scalar(stream, FILE_HEADER_SIZE);
            write_file_scalar(stream, ENDIAN_MARKER);
            write_file_scalar(stream, std::uint32_t{0});
            write_file_scalar(stream, std::uint64_t{0});
            write_file_scalar(stream, std::uint64_t{0});

            BinaryOutput output{stream};
            write_scene(output, scene);

            const WriteResult result{
                output.size(),
                output.checksum()
            };
            stream.seekp(24, std::ios::beg);
            write_file_scalar(stream, result.payload_size);
            write_file_scalar(stream, result.payload_checksum);
            stream.close();
            return result;
        }
        catch (const std::exception& exception) {
            throw std::runtime_error(
                "Could not write " + display_path(path) + ": " +
                exception.what());
        }
    }

    cooker::JungleScene JungleSceneFile::read(
        const std::filesystem::path& path) {

        if (path.empty()) {
            throw std::runtime_error("The .fjscene input path is empty.");
        }

        try {
            const auto file_size = std::filesystem::file_size(path);
            if (file_size < FILE_HEADER_SIZE || file_size > MAX_FILE_SIZE) {
                fail("the file size is outside the supported range");
            }

            std::ifstream stream(path, std::ios::binary);
            stream.exceptions(std::ios::badbit | std::ios::failbit);

            std::array<char, FILE_MAGIC.size()> magic{};
            stream.read(magic.data(), magic.size());
            if (magic != FILE_MAGIC) {
                fail("the magic bytes do not match");
            }

            const auto version = read_file_scalar<std::uint32_t>(stream);
            const auto header_size = read_file_scalar<std::uint32_t>(stream);
            const auto endian_marker = read_file_scalar<std::uint32_t>(stream);
            const auto reserved = read_file_scalar<std::uint32_t>(stream);
            const auto payload_size = read_file_scalar<std::uint64_t>(stream);
            const auto payload_checksum = read_file_scalar<std::uint64_t>(stream);

            if (version != FORMAT_VERSION) {
                fail("the format version is unsupported");
            }
            if (header_size != FILE_HEADER_SIZE) {
                fail("the header size does not match version 0");
            }
            if (endian_marker != ENDIAN_MARKER) {
                fail("the byte order is unsupported");
            }
            if (reserved != 0) {
                fail("a reserved header value is nonzero");
            }
            if (payload_size != file_size - FILE_HEADER_SIZE) {
                fail("the payload size does not match the file size");
            }

            BinaryInput input{stream, payload_size};
            auto result = read_scene(input);
            if (input.remaining() != 0) {
                fail("the payload contains trailing bytes");
            }
            if (input.checksum() != payload_checksum) {
                fail("the payload checksum does not match");
            }
            return result;
        }
        catch (const std::exception& exception) {
            throw std::runtime_error(
                "Could not read " + display_path(path) + ": " +
                exception.what());
        }
    }

} // namespace fjr::scene


/*
const auto write_result =
    fjr::scene::JungleSceneFile::write(cooked_scene, scene);
const auto reloaded =
    fjr::scene::JungleSceneFile::read(cooked_scene);
if (reloaded != scene) {
    throw std::runtime_error(
        "The .fjscene round trip changed Jungle scene data.");
}

std::cout
    << "\nCooked scene\n"
    << "  output             : "
    << cooked_scene.generic_string() << '\n'
    << "  format version     : "
    << fjr::scene::JungleSceneFile::FORMAT_VERSION << '\n'
    << "  payload bytes      : "
    << write_result.payload_size << '\n'
    << "  payload checksum   : 0x"
    << std::hex << write_result.payload_checksum << std::dec << '\n'
    << "  round trip         : exact\n";

*/
