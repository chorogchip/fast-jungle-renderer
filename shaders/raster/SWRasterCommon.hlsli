#pragma once

#include "../common/ConstBufCamera.hlsli"
#include "../common/Quaternion.hlsli"
#include "../common/RenderData.hlsli"

static const uint SOFTWARE_THREADS_PER_GROUP = 64;
static const uint SOFTWARE_LOCAL_WORK_BITS = 17;
static const uint SOFTWARE_TRIANGLE_BITS = 7;
static const uint RASTER_CLASS_OPAQUE = 2;
static const float SOFTWARE_SUBPIXEL_SCALE = 256.0f;

cbuffer SoftwareBatchConstants : register(b1)
{
    uint batch_id;
    uint visible_instance_offset;
    uint instance_count;
    uint cluster_offset;
    uint cluster_count;
    uint submesh_id;
    uint dispatch_width;
};

StructuredBuffer<uint> visible_instances : register(t0);
StructuredBuffer<InstanceTransform> instances : register(t1);
StructuredBuffer<VertexDecodeParams> vertex_decode_params : register(t2);
StructuredBuffer<SubMesh> submeshes : register(t3);
StructuredBuffer<RasterCluster> raster_clusters : register(t4);
StructuredBuffer<uint> raster_cluster_vertices : register(t5);
StructuredBuffer<uint> raster_cluster_triangles : register(t6);
ByteAddressBuffer opaque_vertices : register(t7);
ByteAddressBuffer alpha_vertices : register(t8);

RWByteAddressBuffer software_key : register(u0);

groupshared InstanceTransform group_instance;
groupshared VertexDecodeParams group_decode;
groupshared SubMesh group_submesh;
groupshared RasterCluster group_cluster;
groupshared float4 group_raster_positions[SOFTWARE_THREADS_PER_GROUP];

float Edge(float2 a, float2 b, float2 sample_position)
{
    const float2 edge = b - a;
    const float2 relative = sample_position - a;
    return edge.x * relative.y - edge.y * relative.x;
}

float3 LoadPosition(uint vertex_id, uint raster_class)
{
    const uint byte_offset = vertex_id *
        (raster_class == RASTER_CLASS_OPAQUE ? 8u : 12u);
    const uint2 packed = raster_class == RASTER_CLASS_OPAQUE
        ? opaque_vertices.Load2(byte_offset)
        : alpha_vertices.Load2(byte_offset);
    return float3(
        packed.x & 0xffffu,
        packed.x >> 16u,
        packed.y & 0xffffu) * (1.0f / 65535.0f);
}

void RasterTriangle(
    float4 raster0,
    float4 raster1,
    float4 raster2,
    uint local_work,
    uint local_triangle,
    bool double_sided)
{
    if (min(raster0.w, min(raster1.w, raster2.w)) <= 0.0f)
        return;

    float2 position0 = raster0.xy;
    float2 position1 = raster1.xy;
    float2 position2 = raster2.xy;
    float3 depth = float3(raster0.z, raster1.z, raster2.z);

    float area = Edge(position0, position1, position2);
    if (area < 0.0f && double_sided)
    {
        const float2 old_position1 = position1;
        position1 = position2;
        position2 = old_position1;
        const float old_depth1 = depth.y;
        depth.y = depth.z;
        depth.z = old_depth1;
        area = -area;
    }
    if (area <= 0.00001f)
        return;

    const float2 bounds_min = min(position0, min(position1, position2));
    const float2 bounds_max = max(position0, max(position1, position2));
    int2 pixel_min = int2(floor(bounds_min));
    int2 pixel_max = int2(ceil(bounds_max)) - 1;
    const int2 viewport_limit =
        int2(cam_pixel_width - 1, cam_pixel_height - 1);
    pixel_min = clamp(pixel_min, int2(0, 0), viewport_limit);
    pixel_max = clamp(pixel_max, int2(0, 0), viewport_limit);
    if (any(pixel_min > pixel_max))
        return;

    const float2 direction0 = position2 - position1;
    const float2 direction1 = position0 - position2;
    const float2 direction2 = position1 - position0;
    const bool3 top_left = bool3(
        direction0.y < 0.0f ||
            (direction0.y == 0.0f && direction0.x > 0.0f),
        direction1.y < 0.0f ||
            (direction1.y == 0.0f && direction1.x > 0.0f),
        direction2.y < 0.0f ||
            (direction2.y == 0.0f && direction2.x > 0.0f));
    const float3 edge_dx = -float3(
        direction0.y,
        direction1.y,
        direction2.y);
    const float3 edge_dy = float3(
        direction0.x,
        direction1.x,
        direction2.x);
    const float2 first_sample = float2(pixel_min) + 0.5f;
    float3 row_edges = float3(
        Edge(position1, position2, first_sample),
        Edge(position2, position0, first_sample),
        Edge(position0, position1, first_sample));
    const float inverse_area = rcp(area);
    const float depth_dx = dot(edge_dx, depth) * inverse_area;
    const float depth_dy = dot(edge_dy, depth) * inverse_area;
    float row_depth = dot(row_edges, depth) * inverse_area;
    const uint primitive_id =
        (batch_id << (SOFTWARE_LOCAL_WORK_BITS + SOFTWARE_TRIANGLE_BITS)) |
        (local_work << SOFTWARE_TRIANGLE_BITS) |
        local_triangle;
    [loop]
    for (int y = pixel_min.y; y <= pixel_max.y; ++y)
    {
        float3 sample_edges = row_edges;
        float sample_depth = row_depth;
        [loop]
        for (int x = pixel_min.x; x <= pixel_max.x; ++x)
        {
            const bool3 outside = or(
                sample_edges < 0.0f,
                and(sample_edges == 0.0f, !top_left));
            if (!any(outside) &&
                sample_depth >= 0.0f && sample_depth <= 1.0f)
            {
                const uint64_t key =
                    (uint64_t(asuint(sample_depth)) << 32u) |
                    primitive_id;
                const uint byte_offset =
                    (uint(y) * cam_pixel_width + uint(x)) * 8u;
                software_key.InterlockedMin64(byte_offset, key);
            }
            sample_edges += edge_dx;
            sample_depth += depth_dx;
        }
        row_edges += edge_dy;
        row_depth += depth_dy;
    }
}

void SoftwareRasterMain(uint thread_id, uint3 group_id)
{
    const uint local_work = group_id.x + group_id.y * dispatch_width;
    if (local_work >= instance_count * cluster_count)
        return;

    const uint local_instance = local_work / cluster_count;
    const uint local_cluster = local_work % cluster_count;

    if (thread_id == 0)
    {
        group_submesh = submeshes[submesh_id];
        group_decode = vertex_decode_params[submesh_id];
        group_cluster = raster_clusters[cluster_offset + local_cluster];
        const uint instance_id = visible_instances[
            visible_instance_offset + local_instance];
        group_instance = instances[instance_id];
    }
    GroupMemoryBarrierWithGroupSync();

    const RasterCluster cluster = group_cluster;
    const SubMesh submesh = group_submesh;
    if (thread_id < cluster.vertex_count)
    {
        const uint local_vertex = raster_cluster_vertices[
            cluster.vertex_offset + thread_id];
        const float3 encoded_position = LoadPosition(
            submesh.base_vertex + local_vertex,
            submesh.raster_class);
        const float3 object_position = group_decode.position_min.xyz +
            encoded_position * group_decode.position_extent.xyz;
        const float3 world_position = group_instance.position +
            RotateForwardVector(
                object_position * group_instance.scale,
                group_instance.rotation);
        const float4 clip_position = mul(
            float4(world_position, 1.0f),
            cam_view_projection);
        const float3 ndc = clip_position.xyz / clip_position.w;
        float2 raster_position = float2(
            ndc.x * 0.5f + 0.5f,
            0.5f - ndc.y * 0.5f) *
            float2(cam_pixel_width, cam_pixel_height);
        raster_position = round(
            raster_position * SOFTWARE_SUBPIXEL_SCALE) /
            SOFTWARE_SUBPIXEL_SCALE;
        group_raster_positions[thread_id] = float4(
            raster_position,
            ndc.z,
            clip_position.w);
    }
    GroupMemoryBarrierWithGroupSync();

    const bool double_sided = submesh.raster_class != RASTER_CLASS_OPAQUE;
    for (uint local_triangle = thread_id;
        local_triangle < cluster.triangle_count;
        local_triangle += SOFTWARE_THREADS_PER_GROUP)
    {
        const uint packed_triangle = raster_cluster_triangles[
            cluster.triangle_offset + local_triangle];
        const uint vertex0 = packed_triangle & 0x3fu;
        const uint vertex1 = (packed_triangle >> 6u) & 0x3fu;
        const uint vertex2 = (packed_triangle >> 12u) & 0x3fu;
        RasterTriangle(
            group_raster_positions[vertex0],
            group_raster_positions[vertex1],
            group_raster_positions[vertex2],
            local_work,
            local_triangle,
            double_sided);
    }
}
