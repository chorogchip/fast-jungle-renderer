#include "../common/ConstBufCamera.hlsli"
#include "../common/ConstantDraw.hlsli"
#include "../common/RenderData.hlsli"
#include "../common/Quaternion.hlsli"
#include "VisibilityCommon.hlsli"

struct VSInput
{
    float4 position : POSITION;
    float2 uv : TEXCOORD0;
};

StructuredBuffer<uint> visible_instances : register(t0);
StructuredBuffer<InstanceTransform> instances : register(t1);
StructuredBuffer<VertexDecodeParams> vertex_decode_params : register(t2);
StructuredBuffer<Material> materials : register(t3);

AlphaVisibilityVertexOutput main(
    VSInput input,
    uint local_instance_id : SV_InstanceID)
{
    const uint instance_id = visible_instances[
        visible_instance_offset + local_instance_id];
    const InstanceTransform instance = instances[instance_id];
    const VertexDecodeParams decode = vertex_decode_params[submesh_id];
    const Material material = materials[material_id];

    const float2 uv = decode.uv_min_extent.xy +
        input.uv * decode.uv_min_extent.zw;
    float3 object_position = decode.position_min.xyz +
        input.position.xyz * decode.position_extent.xyz;

    if ((material.flags & MATERIAL_FLAG_IMPOSTOR) != 0u)
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
            float3(
                0.0f,
                plane.y * material.impostor_half_height,
                0.0f);
    }

    const float3 world_position = instance.position + RotateForwardVector(
        object_position * instance.scale,
        instance.rotation);

    AlphaVisibilityVertexOutput output;
    output.position = mul(
        float4(world_position, 1.0f),
        cam_view_projection);
    output.uv = uv;
    output.instance_id = instance_id;
    return output;
}
