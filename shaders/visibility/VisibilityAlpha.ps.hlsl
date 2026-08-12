#include "../common/ConstantDraw.hlsli"
#include "../common/RenderData.hlsli"
#include "VisibilityCommon.hlsli"

StructuredBuffer<Material> materials : register(t3);
Texture2D<float4> scene_textures[] : register(t0, space1);
SamplerState material_sampler : register(s0);

uint2 main(
    AlphaVisibilityVertexOutput input,
    uint triangle_id : SV_PrimitiveID,
    bool front_face : SV_IsFrontFace) : SV_TARGET
{
    const Material material = materials[material_id];

    const float opacity = material.opacity * scene_textures[
        NonUniformResourceIndex(material.texture_opacity)].Sample(
            material_sampler,
            input.uv).r;
    clip(opacity - material.opacity_threshold);

    return PackVisibility(
        triangle_id,
        submesh_id,
        input.instance_id,
        front_face);
}
