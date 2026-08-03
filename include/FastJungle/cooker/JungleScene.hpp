#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <variant>
#include <vector>

namespace fjr::cooker {

    class JungleScene {
    public:
        static constexpr std::uint32_t INVALID_INDEX =
            std::numeric_limits<std::uint32_t>::max();

        struct Float2 {
            float x = 0.0f;
            float y = 0.0f;

            bool operator==(const Float2&) const = default;
        };

        struct Float3 {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;

            bool operator==(const Float3&) const = default;
        };

        struct Float4 {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            float w = 0.0f;

            bool operator==(const Float4&) const = default;
        };

        // USD quaternions are stored as real + imaginary, not xyzw. Keeping
        // the names explicit prevents a silent component-order change at
        // upload time.
        struct Quaternion {
            float real = 1.0f;
            Float3 imaginary{};

            bool operator==(const Quaternion&) const = default;
        };

        struct Matrix4x4 {
            std::array<double, 16> values{
                1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 0.0, 0.0, 1.0
            };

            bool operator==(const Matrix4x4&) const = default;
        };

        enum class ObjectKind : std::uint8_t {
            Unknown,
            Anthurium,
            GrassA,
            GrassB,
            PyramidGrassB,
            PyramidMoss,
            QueenForest,
            RiverForest,
            RiverSapling,
            RiverSeedling,
            Shrub,
            ShrubSorrel,
            Nettle,
            Terrain,
            Pyramid,
            Banyan,
            River,
            Creek,
            Environment,
            Camera
        };

        enum class PrimKind : std::uint8_t {
            Other,
            Scope,
            Transform,
            Mesh,
            GeomSubset,
            PointInstancer,
            Material,
            Shader,
            Camera,
            Light
        };

        enum NodeFlag : std::uint32_t {
            NodeActive = 1u << 0u,
            NodeVisible = 1u << 1u,
            NodeNativeInstance = 1u << 2u,
            NodeNativePrototype = 1u << 3u,
            NodeInsideNativePrototype = 1u << 4u,
            NodeResetsTransform = 1u << 5u,
        };

        struct Node {
            std::string path;
            std::string name;
            std::string usd_type_name;
            std::string purpose;
            std::string native_prototype_path;
            std::uint32_t parent = INVALID_INDEX;
            std::uint32_t payload = INVALID_INDEX;
            PrimKind prim_kind = PrimKind::Other;
            ObjectKind object_kind = ObjectKind::Unknown;
            std::uint32_t flags = 0;
            Matrix4x4 local_transform{};

            bool operator==(const Node&) const = default;
        };

        struct SourceLayer {
            std::string identifier;
            std::string resolved_path;
            bool is_root = false;

            bool operator==(const SourceLayer&) const = default;
        };

        struct AssetReference {
            std::string authored_path;
            std::string resolved_path;
            bool resolved_file_exists = false;

            bool operator==(const AssetReference&) const = default;
        };

        enum class PrimvarStorage : std::uint8_t {
            Boolean,
            Float,
            Float2,
            Float3
        };

        using PrimvarData = std::variant<
            std::vector<std::uint8_t>,
            std::vector<float>,
            std::vector<Float2>,
            std::vector<Float3>>;

        struct Primvar {
            std::string name;
            std::string type_name;
            std::string interpolation;
            PrimvarStorage storage = PrimvarStorage::Float;
            PrimvarData data;
            std::vector<std::int32_t> indices;

            bool operator==(const Primvar&) const = default;
        };

        struct Mesh {
            std::string prim_path;
            std::string orientation;
            std::string subdivision_scheme;
            std::string normals_interpolation;
            std::string material_path;
            bool double_sided = false;
            std::vector<Float3> points;
            std::vector<std::int32_t> face_vertex_counts;
            std::vector<std::int32_t> face_vertex_indices;
            std::vector<Float3> normals;
            std::vector<std::int32_t> hole_indices;
            std::vector<Primvar> primvars;

            bool operator==(const Mesh&) const = default;
        };

        struct MeshSubset {
            std::string prim_path;
            std::string mesh_path;
            std::string element_type;
            std::string family_name;
            std::string family_type;
            std::string material_path;
            std::vector<std::int32_t> indices;

            bool operator==(const MeshSubset&) const = default;
        };

        struct PointInstancer {
            std::string prim_path;
            ObjectKind object_kind = ObjectKind::Unknown;
            std::vector<std::string> prototype_paths;
            std::vector<std::int32_t> prototype_indices;
            std::vector<Float3> positions;
            std::vector<Quaternion> orientations;
            std::vector<Float3> scales;
            std::vector<Float3> velocities;
            std::vector<Float3> accelerations;
            std::vector<Float3> angular_velocities;
            std::vector<std::int64_t> ids;
            std::vector<std::int64_t> inactive_ids;
            std::vector<std::int64_t> invisible_ids;
            std::vector<Primvar> primvars;

            bool operator==(const PointInstancer&) const = default;
        };

        struct NativeInstance {
            std::string prim_path;
            std::string prototype_path;
            ObjectKind object_kind = ObjectKind::Unknown;

            bool operator==(const NativeInstance&) const = default;
        };

        enum class ShaderValueKind : std::uint8_t {
            Empty,
            Float,
            Float2,
            Float3,
            Float4,
            Token,
            String,
            Asset,
            Unsupported
        };

        using ShaderValueData = std::variant<
            std::monostate,
            float,
            Float2,
            Float3,
            Float4,
            std::string,
            AssetReference>;

        struct ShaderValue {
            std::string type_name;
            std::string unsupported_value;
            ShaderValueKind kind = ShaderValueKind::Empty;
            ShaderValueData data;

            bool operator==(const ShaderValue&) const = default;
        };

        struct ShaderConnection {
            std::string source_prim_path;
            std::string source_name;
            bool source_is_output = true;

            bool operator==(const ShaderConnection&) const = default;
        };

        struct ShaderInput {
            std::string name;
            ShaderValue value;
            std::vector<ShaderConnection> connections;
            std::vector<std::string> invalid_source_paths;

            bool operator==(const ShaderInput&) const = default;
        };

        struct ShaderOutput {
            std::string name;
            std::string type_name;
            std::vector<ShaderConnection> connections;
            std::vector<std::string> invalid_source_paths;

            bool operator==(const ShaderOutput&) const = default;
        };

        struct ShaderNode {
            std::string prim_path;
            std::string shader_id;
            std::vector<ShaderInput> inputs;
            std::vector<ShaderOutput> outputs;

            bool operator==(const ShaderNode&) const = default;
        };

        struct Material {
            std::string prim_path;
            std::vector<std::uint32_t> shader_nodes;
            std::vector<ShaderOutput> outputs;

            bool operator==(const Material&) const = default;
        };

        struct Camera {
            std::string prim_path;
            std::string projection;
            float focal_length = 0.0f;
            float horizontal_aperture = 0.0f;
            float vertical_aperture = 0.0f;
            float horizontal_aperture_offset = 0.0f;
            float vertical_aperture_offset = 0.0f;
            float focus_distance = 0.0f;
            float f_stop = 0.0f;
            Float2 clipping_range{};

            bool operator==(const Camera&) const = default;
        };

        struct EnvironmentLight {
            std::string prim_path;
            Float3 color{1.0f, 1.0f, 1.0f};
            float intensity = 1.0f;
            float exposure = 0.0f;
            AssetReference texture;

            bool operator==(const EnvironmentLight&) const = default;
        };

        enum class DiagnosticSeverity : std::uint8_t {
            Information,
            Warning,
            Error
        };

        struct Diagnostic {
            DiagnosticSeverity severity = DiagnosticSeverity::Information;
            std::string subject;
            std::string message;

            bool operator==(const Diagnostic&) const = default;
        };

        struct Statistics {
            std::uint64_t composed_prim_count = 0;
            std::uint64_t composed_mesh_count = 0;
            std::uint64_t composed_mesh_subset_count = 0;
            std::uint64_t composed_material_count = 0;
            std::uint64_t composed_shader_count = 0;
            std::uint64_t composed_camera_count = 0;
            std::uint64_t native_prototype_count = 0;
            std::uint64_t exact_origin_instance_count = 0;
            std::uint64_t time_sampled_attribute_count = 0;
            std::uint64_t time_sample_count = 0;

            bool operator==(const Statistics&) const = default;
        };

        std::string source_root;
        std::string up_axis;
        double meters_per_unit = 1.0;
        double start_time_code = 0.0;
        double end_time_code = 0.0;
        Statistics statistics{};
        std::vector<SourceLayer> source_layers;
        std::vector<Node> nodes;
        std::vector<Mesh> meshes;
        std::vector<MeshSubset> mesh_subsets;
        std::vector<PointInstancer> point_instancers;
        std::vector<NativeInstance> native_instances;
        std::vector<ShaderNode> shader_nodes;
        std::vector<Material> materials;
        std::vector<Camera> cameras;
        std::vector<EnvironmentLight> environment_lights;
        std::vector<Diagnostic> import_diagnostics;

        bool operator==(const JungleScene&) const = default;

        [[nodiscard]]
        static ObjectKind classify_object(const std::string& prim_path);

        [[nodiscard]]
        static const char* object_kind_name(ObjectKind kind) noexcept;

    };

} // namespace fjr::scene
