#include "../common/ConstBufCamera.hlsli"
#include "../common/ConstantDraw.hlsli"
#include "../common/RenderData.hlsli"
#include "../common/Quaternion.hlsli"

struct VS_input
{
    float4 position : POSITION;
};

struct VS_output
{
    float4 position : SV_Position;
    nointerpolation uint instance_id : TEXCOORD0;
};

StructuredBuffer<uint> visible_instances : register(t0);
StructuredBuffer<InstanceTransform> instances : register(t1);
StructuredBuffer<VertexDecodeParams> vertex_decode_params : register(t2);

VS_output main(VS_input input, uint local_instance_id : SV_InstanceID)
{
    const uint instance_id = visible_instances[visible_instance_offset + local_instance_id];
    const InstanceTransform instance = instances[instance_id];
    VertexDecodeParams decode = vertex_decode_params[submesh_id];
    
    const float3 position = decode.position_min.xyz + input.position.xyz * decode.position_extent.xyz;

    const float3 world_position = instance.position +
        RotateForwardVector(position * instance.scale, instance.rotation);
    
    VS_output output;
    output.position = mul(float4(world_position, 1.0f), cam_view_projection);
    output.instance_id = instance_id;
    return output;
}