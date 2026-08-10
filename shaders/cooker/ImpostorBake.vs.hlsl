cbuffer BakeCamera : register(b0)
{
    row_major float4x4 object_to_view;
    row_major float4x4 object_to_clip;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct PixelInput
{
    float4 position : SV_POSITION;
    float3 view_normal : NORMAL;
    float2 uv : TEXCOORD0;
    float3 view_position : TEXCOORD1;
};

PixelInput main(VertexInput input)
{
    const float4 view_position = mul(
        float4(input.position, 1.0f),
        object_to_view);

    PixelInput output;
    output.position = mul(
        float4(input.position, 1.0f),
        object_to_clip);
    output.view_normal = normalize(mul(
        float4(input.normal, 0.0f),
        object_to_view).xyz);
    output.uv = input.uv;
    output.view_position = view_position.xyz;
    return output;
}
