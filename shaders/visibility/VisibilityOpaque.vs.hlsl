#include "../common/ConstantDraw.hlsli"
#include "../common/RenderData.hlsli"

struct VS_input
{
    float4 position : POSITION;
};

struct VS_output
{
    float4 position : SV_Position;
    nointerpolation uint instance_id;
};

StructuredBuffer<uint> visible_instances : register(t0);
StructuredBuffer<InstanceTransform> instances : register(t1);
StructuredBuffer<VertexDecodeParams> vertex_decode_params : register(t5);

static const uint MATERIAL_FLAG_IMPOSTOR = 1u;


float3 RotateForwardVector(float3 value, float4 quaternion)
{
    const float3 twice_cross = 2.0f * cross(quaternion.xyz, value);
    return value +
        quaternion.w * twice_cross +
        cross(quaternion.xyz, twice_cross);
}

float4 main(VS_input input, uint local_instance_id : SV_InstanceID)
{
    const uint instance_id = visible_instances[visible_instance_offset + local_instance_id];
    const InstanceTransform instance = instances[instance_id];
    VertexDecodeParams decode = vertex_decode_params[submesh_id];
    
    float3 position = decode.position_min.xyz + input.position.xyz * decode.position_extent.xyz;

    const float3 world_position = instance.position +
        RotateForwardVector(position * instance.scale, instance.rotation);
    
    VS_output output;
    output.position = mul(float4(world_position, 1.0f), cam_view_projection);
    output.instance_id = instance_id;
    return output;
}
