#include "../common/ConstantDraw.hlsli"
#include "../common/RenderData.hlsli"

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    nointerpolation uint instance_id : TEXCOORD1;
};

StructuredBuffer<Material> materials : register(t3);
Texture2D<float4> scene_textures[] : register(t0, space1);
SamplerState material_sampler : register(s0);

uint2 main(
    VSOutput input,
    uint triangle_id : SV_PrimitiveID,
    bool front_face : SV_IsFrontFace) : SV_TARGET
{
    const Material material = materials[material_id];

    const float opacity = material.opacity * scene_textures[
        NonUniformResourceIndex(material.texture_opacity)].Sample(
            material_sampler,
            input.uv).r;
    clip(opacity - material.opacity_threshold);

    const uint lower = triangle_id | ((submesh_id & 0x3ffu) << 22u);
    const uint back_face = front_face ? 0u : 0x80000000u;
    const uint upper = (submesh_id >> 10u) |
        (input.instance_id << 1u) |
        back_face;
    return uint2(lower, upper);
}
