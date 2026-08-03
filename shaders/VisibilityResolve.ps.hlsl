struct DrawData {
    uint first_index;
    int base_vertex;
    uint instance_offset;
    uint material_id;
    uint instance_kind;
    uint transform_constant_index;
    uint2 padding;
};

struct Material {
    float4 base_color;
    float4 emissive_roughness;
    float4 surface;
    uint4 texture_bindings_0;
    uint4 texture_bindings_1;
};

Texture2D<uint2> visibility_buffer : register(t0);
StructuredBuffer<DrawData> draws : register(t1);
StructuredBuffer<Material> materials : register(t2);

float4 main(float4 position : SV_POSITION) : SV_TARGET {
    const uint2 visibility = visibility_buffer.Load(
        int3(uint2(position.xy), 0));

    if (visibility.x == 0) {
        return float4(0.015, 0.025, 0.04, 1.0);
    }

    const DrawData draw = draws[visibility.x - 1];
    const Material material = materials[draw.material_id];

    // Initial resolve: prove the visibility path and material indirection
    // before adding attribute reconstruction and texture evaluation.
    const float primitive_variation =
        0.92 + 0.08 * frac(visibility.y * 0.61803398875);
    const float3 color =
        material.base_color.rgb * primitive_variation +
        material.emissive_roughness.rgb;
    return float4(color, 1.0);
}
