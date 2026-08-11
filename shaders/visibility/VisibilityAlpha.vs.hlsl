#include "../common/ConstBufCamera.hlsli"
#include "../common/ConstantDraw.hlsli"
#include "../common/RenderData.hlsli"
#include "../common/Quaternion.hlsli"

struct VSInput
{
    float4 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    nointerpolation uint instance_id : TEXCOORD1;
};

StructuredBuffer<uint> visible_instances : register(t0);
StructuredBuffer<InstanceTransform> instances : register(t1);
StructuredBuffer<VertexDecodeParams> vertex_decode_params : register(t2);

VSOutput main(VSInput input, uint local_instance_id : SV_InstanceID)
{
    const uint instance_id = visible_instances[
        visible_instance_offset + local_instance_id];
    const InstanceTransform instance = instances[instance_id];
    const VertexDecodeParams decode = vertex_decode_params[submesh_id];

    const float3 object_position = decode.position_min.xyz +
        input.position.xyz * decode.position_extent.xyz;
    const float3 world_position = instance.position + RotateForwardVector(
        object_position * instance.scale,
        instance.rotation);

    VSOutput output;
    output.position = mul(
        float4(world_position, 1.0f),
        cam_view_projection);
    output.uv = decode.uv_min_extent.xy +
        input.uv * decode.uv_min_extent.zw;
    output.instance_id = instance_id;
    return output;
}
