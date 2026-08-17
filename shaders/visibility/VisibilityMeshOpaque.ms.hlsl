#include "../common/ConstBufCamera.hlsli"
#include "../common/ConstantMeshDispatch.hlsli"
#include "../common/Quaternion.hlsli"
#include "../common/RenderData.hlsli"
#include "../common/SphereCulling.hlsli"

static const uint MESH_THREAD_COUNT = 32;
static const uint MESH_VERTEX_COUNT = 128;
static const uint MESH_TRIANGLE_COUNT = 128;

StructuredBuffer<uint> visible_instances : register(t0);
StructuredBuffer<InstanceTransform> instances : register(t1);
StructuredBuffer<VertexDecodeParams> vertex_decode_params : register(t2);
StructuredBuffer<SubMesh> submeshes : register(t3);
StructuredBuffer<RasterCluster> raster_clusters : register(t4);
StructuredBuffer<uint> raster_cluster_vertices : register(t5);
StructuredBuffer<uint> raster_cluster_triangles : register(t6);
ByteAddressBuffer opaque_vertices : register(t7);

struct MeshVertexOutput
{
    float4 position : SV_Position;
};

struct MeshPrimitiveOutput
{
    nointerpolation uint instance_id : TEXCOORD0;
    nointerpolation uint triangle_id : TEXCOORD1;
};

groupshared uint group_instance_id;
groupshared InstanceTransform group_instance;
groupshared VertexDecodeParams group_decode;
groupshared SubMesh group_submesh;
groupshared RasterCluster group_cluster;
groupshared uint group_visible;

float3 LoadPosition(uint vertex_id)
{
    const uint2 packed = opaque_vertices.Load2(vertex_id * 8u);
    return float3(
        packed.x & 0xffffu,
        packed.x >> 16u,
        packed.y & 0xffffu) * (1.0f / 65535.0f);
}

[outputtopology("triangle")]
[numthreads(MESH_THREAD_COUNT, 1, 1)]
void main(
    uint thread_id : SV_GroupIndex,
    uint3 group_id : SV_GroupID,
    out vertices MeshVertexOutput vertices[MESH_VERTEX_COUNT],
    out indices uint3 triangles[MESH_TRIANGLE_COUNT],
    out primitives MeshPrimitiveOutput primitives[MESH_TRIANGLE_COUNT])
{
    if (thread_id == 0)
    {
        group_submesh = submeshes[submesh_id];
        group_decode = vertex_decode_params[submesh_id];
        group_cluster = raster_clusters[
            group_submesh.raster_cluster_offset + group_id.x];
        group_instance_id = visible_instances[
            visible_instance_offset + group_id.y];
        group_instance = instances[group_instance_id];

        float3 world_center;
        float world_radius;
        TransformSphere(
            group_cluster.bounds_sphere,
            group_instance,
            world_center,
            world_radius);
        float3 world_axis = normalize(RotateForwardVector(
            group_cluster.normal_cone.xyz / group_instance.scale,
            group_instance.rotation));
        world_axis *= group_instance.scale.x *
            group_instance.scale.y * group_instance.scale.z < 0.0f
            ? -1.0f
            : 1.0f;
        const float3 camera_to_center =
            world_center - cam_world_position;
        group_visible = dot(camera_to_center, world_axis) <
            group_cluster.normal_cone.w * length(camera_to_center) +
            world_radius;
    }
    GroupMemoryBarrierWithGroupSync();

    const RasterCluster cluster = group_cluster;
    SetMeshOutputCounts(
        group_visible != 0 ? cluster.vertex_count : 0,
        group_visible != 0 ? cluster.triangle_count : 0);

    if (group_visible == 0)
        return;

    for (uint local_vertex = thread_id;
        local_vertex < cluster.vertex_count;
        local_vertex += MESH_THREAD_COUNT)
    {
        const uint vertex_id = raster_cluster_vertices[
            cluster.vertex_offset + local_vertex];
        const float3 encoded_position = LoadPosition(
            group_submesh.base_vertex + vertex_id);
        const float3 object_position = group_decode.position_min.xyz +
            encoded_position * group_decode.position_extent.xyz;
        const float3 world_position = group_instance.position +
            RotateForwardVector(
                object_position * group_instance.scale,
                group_instance.rotation);
        vertices[local_vertex].position = mul(
            float4(world_position, 1.0f),
            cam_view_projection);
    }

    for (uint local_triangle = thread_id;
        local_triangle < cluster.triangle_count;
        local_triangle += MESH_THREAD_COUNT)
    {
        const uint packed_triangle = raster_cluster_triangles[
            cluster.triangle_offset + local_triangle];
        triangles[local_triangle] = uint3(
            packed_triangle & 0xffu,
            (packed_triangle >> 8u) & 0xffu,
            (packed_triangle >> 16u) & 0xffu);
        primitives[local_triangle].instance_id = group_instance_id;
        primitives[local_triangle].triangle_id =
            group_id.x * MESH_TRIANGLE_COUNT + local_triangle;
    }
}
