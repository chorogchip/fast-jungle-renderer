#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <variant>
#include <vector>

namespace fjr::scene {

    constexpr std::uint32_t INVALID_SCENE_INDEX =
        std::numeric_limits<std::uint32_t>::max();

    struct Float2 {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct Float3 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct Float4 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;
    };

    // USD quaternions are stored as real + imaginary, not xyzw. Keeping the
    // names explicit prevents a silent component-order change at upload time.
    struct Quaternion {
        float real = 1.0f;
        Float3 imaginary{};
    };

    struct Matrix4x4 {
        std::array<double, 16> values{
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0
        };
    };

    enum class JungleObjectKind : std::uint8_t {
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

    enum SceneNodeFlag : std::uint32_t {
        SceneNodeActive = 1u << 0u,
        SceneNodeVisible = 1u << 1u,
        SceneNodeNativeInstance = 1u << 2u,
        SceneNodeNativePrototype = 1u << 3u,
        SceneNodeInsideNativePrototype = 1u << 4u,
        SceneNodeResetsTransform = 1u << 5u,
    };

    struct SceneNode {
        std::string path;
        std::string name;
        std::string usd_type_name;
        std::string purpose;
        std::string native_prototype_path;
        std::uint32_t parent = INVALID_SCENE_INDEX;
        std::uint32_t payload = INVALID_SCENE_INDEX;
        PrimKind prim_kind = PrimKind::Other;
        JungleObjectKind object_kind = JungleObjectKind::Unknown;
        std::uint32_t flags = 0;
        Matrix4x4 local_transform{};
    };

    struct SourceLayer {
        std::string identifier;
        std::string resolved_path;
        bool is_root = false;
    };

    struct AssetReference {
        std::string authored_path;
        std::string resolved_path;
        bool resolved_file_exists = false;
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
    };

    struct MeshSubset {
        std::string prim_path;
        std::string mesh_path;
        std::string element_type;
        std::string family_name;
        std::string family_type;
        std::string material_path;
        std::vector<std::int32_t> indices;
    };

    struct PointInstancer {
        std::string prim_path;
        JungleObjectKind object_kind = JungleObjectKind::Unknown;
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
    };

    struct NativeInstance {
        std::string prim_path;
        std::string prototype_path;
        JungleObjectKind object_kind = JungleObjectKind::Unknown;
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
    };

    struct ShaderConnection {
        std::string source_prim_path;
        std::string source_name;
        bool source_is_output = true;
    };

    struct ShaderInput {
        std::string name;
        ShaderValue value;
        std::vector<ShaderConnection> connections;
        std::vector<std::string> invalid_source_paths;
    };

    struct ShaderOutput {
        std::string name;
        std::string type_name;
        std::vector<ShaderConnection> connections;
        std::vector<std::string> invalid_source_paths;
    };

    struct ShaderNode {
        std::string prim_path;
        std::string shader_id;
        std::vector<ShaderInput> inputs;
        std::vector<ShaderOutput> outputs;
    };

    struct Material {
        std::string prim_path;
        std::vector<std::uint32_t> shader_nodes;
        std::vector<ShaderOutput> outputs;
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
    };

    struct EnvironmentLight {
        std::string prim_path;
        Float3 color{1.0f, 1.0f, 1.0f};
        float intensity = 1.0f;
        float exposure = 0.0f;
        AssetReference texture;
    };

    enum class DiagnosticSeverity : std::uint8_t {
        Information,
        Warning,
        Error
    };

    struct SceneDiagnostic {
        DiagnosticSeverity severity = DiagnosticSeverity::Information;
        std::string subject;
        std::string message;
    };

    struct SceneStatistics {
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
    };

    struct JungleScene {
        std::string source_root;
        std::string up_axis;
        double meters_per_unit = 1.0;
        double start_time_code = 0.0;
        double end_time_code = 0.0;
        SceneStatistics statistics{};
        std::vector<SourceLayer> source_layers;
        std::vector<SceneNode> nodes;
        std::vector<Mesh> meshes;
        std::vector<MeshSubset> mesh_subsets;
        std::vector<PointInstancer> point_instancers;
        std::vector<NativeInstance> native_instances;
        std::vector<ShaderNode> shader_nodes;
        std::vector<Material> materials;
        std::vector<Camera> cameras;
        std::vector<EnvironmentLight> environment_lights;
        std::vector<SceneDiagnostic> import_diagnostics;
    };

    JungleObjectKind classify_jungle_object(const std::string& prim_path);
    const char* jungle_object_kind_name(JungleObjectKind kind);
    std::vector<SceneDiagnostic> validate_jungle_scene(
        const JungleScene& scene);

} // namespace fjr::scene
