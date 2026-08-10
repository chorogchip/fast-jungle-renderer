
#ifndef FJR_FOLIAGE_WIND
#define FJR_FOLIAGE_WIND 0
#endif

#include "common/ForwardConstants.hlsli"

StructuredBuffer<uint> visible_instances : register(t0);
StructuredBuffer<InstanceTransform> instances : register(t1);
StructuredBuffer<Material> materials : register(t2);
StructuredBuffer<VertexDecodeParams> vertex_decode_params : register(t5);

static const uint MATERIAL_FLAG_IMPOSTOR = 1u;

struct VertexInput
{
    float4 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

float3 RotateForwardVector(float3 value, float4 quaternion)
{
    const float3 twice_cross = 2.0f * cross(quaternion.xyz, value);
    return value +
        quaternion.w * twice_cross +
        cross(quaternion.xyz, twice_cross);
}

float3 RotateInverseVector(float3 value, float4 quaternion)
{
    return RotateForwardVector(
        value,
        float4(-quaternion.xyz, quaternion.w));
}

ForwardPixelInput main(
    VertexInput input,
    uint local_instance_id : SV_InstanceID)
{
    const uint instance_id = visible_instances[
        visible_instance_offset + local_instance_id];
    const InstanceTransform instance = instances[instance_id];
    const Material material = materials[material_id];
    const bool is_impostor =
        (material.flags & MATERIAL_FLAG_IMPOSTOR) != 0;
    
    
    VertexDecodeParams decode = vertex_decode_params[submesh_id];
    
    float3 object_position = decode.position_min.xyz + input.position.xyz * decode.position_extent.xyz;
    float2 uv = decode.uv_min_extent.xy + input.uv * decode.uv_min_extent.zw;
    float3 object_normal = input.normal * 2.0f - 1.0f;
    float3 wind_sample_object_position = object_position;

    if (is_impostor)
    {
        const float3 local_center_scaled =
            material.impostor_center * instance.scale;
        const float3 world_center = instance.position + RotateForwardVector(
            local_center_scaled,
            instance.rotation);
        float3 local_to_camera = RotateInverseVector(
            cam_world_position - world_center,
            instance.rotation);
        local_to_camera.y = 0.0f;
        local_to_camera = normalize(local_to_camera);
        const float3 local_forward = -local_to_camera;
        const float3 local_right = normalize(cross(
            float3(0.0f, 1.0f, 0.0f),
            local_forward));
        const float2 plane = float2(
            uv.x * 2.0f - 1.0f,
            1.0f - uv.y * 2.0f);
        object_position = material.impostor_center +
            local_right * (plane.x * material.impostor_half_width) +
            float3(0.0f, plane.y * material.impostor_half_height, 0.0f);
        object_normal = local_to_camera;
        wind_sample_object_position = material.impostor_center;
    }

    float3 world_position = instance.position + RotateForwardVector(
        object_position * instance.scale,
        instance.rotation);

#if FJR_FOLIAGE_WIND
    const float3 wind_sample_position =
        instance.position + RotateForwardVector(
            wind_sample_object_position * instance.scale,
            instance.rotation);
    const uint phase_hash =
        instance_id * 1664525u + 1013904223u;
    const float instance_phase =
        float(phase_hash & 0xffffu) *
        (6.2831853f / 65535.0f);
    const float spatial_phase = dot(
        wind_sample_position.xz,
        float2(0.0031f, 0.0043f));
    float gust = sin(
        animation_time * 0.9f +
        spatial_phase +
        instance_phase);
    gust += 0.35f * sin(
        animation_time * 2.1f +
        spatial_phase * 1.7f +
        instance_phase * 1.37f);
    world_position.xz +=
        float2(0.8f, 0.6f) * gust * 0.5f;
#endif

    const float3 inverse_scale =
        sign(instance.scale) /
        max(abs(instance.scale), 1.0e-8f);

    ForwardPixelInput output;
    output.position = mul(
        float4(world_position, 1.0f),
        cam_view_projection);
    output.world_position = world_position;
    output.world_normal = normalize(
        RotateForwardVector(
            object_normal * inverse_scale,
            instance.rotation));
    output.uv = uv;
    return output;
}
