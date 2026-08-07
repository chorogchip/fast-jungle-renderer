#include "ForwardCommon.hlsli"

ByteAddressBuffer instances : register(t0);

struct PointMeshBatch {
    row_major float4x4 local_transform;
    uint mesh_index;
    uint first_bin;
    uint first_cluster;
    uint cluster_count;
};

StructuredBuffer<PointMeshBatch> point_mesh_batches : register(t1, space1);

static const uint POINT_INSTANCE_SIZE = 40;

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct PixelInput {
    float4 position : SV_POSITION;
    float3 world_position : TEXCOORD1;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

float3 rotate_by_quaternion(float3 value, float4 quaternion) {
    const float3 twice_cross = 2.0 * cross(quaternion.xyz, value);
    return value +
        quaternion.w * twice_cross +
        cross(quaternion.xyz, twice_cross);
}

PixelInput main(VertexInput input, uint local_instance_id : SV_InstanceID)
{
    
    const uint instance_id = instance_offset + local_instance_id;
    
    PixelInput output;
    const DrawMetadata metadata = draw_metadata[draw_id];
    const PointMeshBatch batch =
        point_mesh_batches[metadata.transform_index];

    float4 world_position = mul(
        float4(input.position, 1.0),
        batch.local_transform);
    float3 world_normal = mul(
        input.normal,
        (float3x3)batch.local_transform);

    const uint byte_offset = instance_id * POINT_INSTANCE_SIZE;
    const float3 position = asfloat(instances.Load3(byte_offset));
    const float4 orientation = asfloat(
        instances.Load4(byte_offset + 12));
    const float3 scale = asfloat(instances.Load3(byte_offset + 28));

    world_position.xyz = rotate_by_quaternion(
        world_position.xyz * scale,
        orientation) + position;

    const float3 inverse_scale =
        sign(scale) / max(abs(scale), 1.0e-8);
    world_normal = rotate_by_quaternion(
        world_normal * inverse_scale,
        orientation);

    output.position = mul(world_position, view_projection);
    output.world_position = world_position.xyz;
    output.normal = world_normal;
    output.uv = input.uv;
    return output;
}
