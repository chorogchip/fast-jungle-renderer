cbuffer ViewConstants : register(b0) {
    row_major float4x4 view_projection;
};

StructuredBuffer<float4> instance_transform_rows : register(t0);

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct VertexOutput {
    float4 position : SV_POSITION;
    float3 world_position : TEXCOORD1;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

VertexOutput main(VertexInput input, uint instance_id : SV_InstanceID) {
    VertexOutput output;
    const uint first_row = instance_id * 7;
    const float4 local_position = float4(input.position, 1.0);
    const float4 world_position =
        local_position.x * instance_transform_rows[first_row] +
        local_position.y * instance_transform_rows[first_row + 1] +
        local_position.z * instance_transform_rows[first_row + 2] +
        local_position.w * instance_transform_rows[first_row + 3];
    output.position = mul(world_position, view_projection);
    output.world_position = world_position.xyz;

    const uint normal_row = first_row + 4;
    output.normal = normalize(
        input.normal.x * instance_transform_rows[normal_row].xyz +
        input.normal.y * instance_transform_rows[normal_row + 1].xyz +
        input.normal.z * instance_transform_rows[normal_row + 2].xyz);
    output.uv = input.uv;
    return output;
}
