cbuffer CameraConstants : register(b0) {
    row_major float4x4 view_projection;
    float3 camera_world_position;
    float camera_padding;

    row_major float4x4 environment_world_transform;
    float3 environment_color;
    float environment_intensity;
    uint environment_texture_id;
    uint3 environment_padding;
};

cbuffer DrawConstants : register(b2) {
    uint instance_offset;
    uint material_id;
    uint instance_kind;
};

struct Material {
    float4 base_color;
    float4 emissive_roughness;
    float4 surface;
    float4 optical;
    uint4 texture_bindings_0;
    uint4 texture_bindings_1;
};

struct TextureBinding {
    uint texture_id;
    uint sampler_id;
    uint channel;
    uint flags;
};

StructuredBuffer<Material> materials : register(t1);
StructuredBuffer<TextureBinding> texture_bindings : register(t2);
Texture2D<float4> scene_textures[] : register(t3);
SamplerState scene_samplers[] : register(s0);

static const uint INVALID_INDEX = 0xffffffff;
static const uint TEXTURE_BINDING_SRGB = 1;
static const uint TEXTURE_CHANNEL_RGB = 5;

struct PixelInput {
    float4 position : SV_POSITION;
    float3 world_position : TEXCOORD1;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    bool front_face : SV_IsFrontFace;
};

float4 sample_binding(uint binding_id, float2 uv) {
    const TextureBinding binding = texture_bindings[binding_id];
    const uint srgb_offset =
        (binding.flags & TEXTURE_BINDING_SRGB) != 0 ? 1 : 0;
    const uint texture_descriptor = binding.texture_id * 2 + srgb_offset;
    return scene_textures[
        NonUniformResourceIndex(texture_descriptor)].Sample(
            scene_samplers[NonUniformResourceIndex(binding.sampler_id)],
            uv);
}

float select_channel(float4 value, uint channel) {
    if (channel == 2) {
        return value.g;
    }
    if (channel == 3) {
        return value.b;
    }
    if (channel == 4) {
        return value.a;
    }
    return value.r;
}

void build_tangent_frame(
    float3 normal,
    float3 position_dx,
    float3 position_dy,
    float2 uv_dx,
    float2 uv_dy,
    out float3 tangent,
    out float3 bitangent) {

    const float3 position_dy_perp = cross(position_dy, normal);
    const float3 position_dx_perp = cross(normal, position_dx);
    tangent = position_dy_perp * uv_dx.x + position_dx_perp * uv_dy.x;
    bitangent = position_dy_perp * uv_dx.y + position_dx_perp * uv_dy.y;

    const float maximum_length_squared = max(
        dot(tangent, tangent),
        dot(bitangent, bitangent));
    if (maximum_length_squared > 1.0e-12) {
        const float inverse_length = rsqrt(maximum_length_squared);
        tangent *= inverse_length;
        bitangent *= inverse_length;
        return;
    }

    const float3 axis = abs(normal.y) < 0.9
        ? float3(0.0, 1.0, 0.0)
        : float3(0.0, 0.0, 1.0);
    tangent = normalize(cross(axis, normal));
    bitangent = normalize(cross(normal, tangent));
}

float2 environment_uv(float3 direction) {
    const float pi = 3.14159265358979323846;
    direction = normalize(direction);
    return float2(
        atan2(direction.x, direction.z) / (2.0 * pi) + 0.5,
        acos(clamp(direction.y, -1.0, 1.0)) / pi);
}

float4 main(PixelInput input) : SV_TARGET {
    const Material material = materials[material_id];
    const float2 uv = input.uv;

    const float3 position_dx = ddx(input.world_position);
    const float3 position_dy = ddy(input.world_position);
    const float2 uv_dx = ddx(uv);
    const float2 uv_dy = ddy(uv);

    float4 albedo = material.base_color;
    if (material.texture_bindings_0.x != INVALID_INDEX) {
        const uint binding_id = material.texture_bindings_0.x;
        const TextureBinding binding = texture_bindings[binding_id];
        const float4 sample = sample_binding(binding_id, uv);
        albedo.rgb *= sample.rgb;
        if (binding.channel != TEXTURE_CHANNEL_RGB) {
            albedo.a *= sample.a;
        }
    }

    float opacity = material.surface.y * albedo.a;
    if (material.texture_bindings_0.w != INVALID_INDEX) {
        const uint binding_id = material.texture_bindings_0.w;
        const TextureBinding binding = texture_bindings[binding_id];
        opacity *= select_channel(
            sample_binding(binding_id, uv),
            binding.channel);
    }
    clip(opacity - material.surface.z);

    // MatrixInstance transforms are restricted to rigid/uniform scale.
    // Perspective interpolation changes the length, so normalize in the PS.
    float3 normal = normalize(input.normal);
    normal *= input.front_face ? 1.0 : -1.0;

    if (material.texture_bindings_0.y != INVALID_INDEX) {
        float3 tangent;
        float3 bitangent;
        build_tangent_frame(
            normal,
            position_dx,
            position_dy,
            uv_dx,
            uv_dy,
            tangent,
            bitangent);
        const float3 tangent_normal =
            sample_binding(material.texture_bindings_0.y, uv).xyz *
            2.0 - 1.0;
        normal = normalize(
            tangent_normal.x * tangent +
            tangent_normal.y * bitangent +
            tangent_normal.z * normal);
    }

    float roughness = material.emissive_roughness.w;
    if (material.texture_bindings_0.z != INVALID_INDEX) {
        const uint binding_id = material.texture_bindings_0.z;
        const TextureBinding binding = texture_bindings[binding_id];
        roughness = select_channel(
            sample_binding(binding_id, uv),
            binding.channel);
    }
    roughness = saturate(roughness);

    float3 emissive = material.emissive_roughness.rgb;
    if (material.texture_bindings_1.x != INVALID_INDEX) {
        emissive *= sample_binding(
            material.texture_bindings_1.x,
            uv).rgb;
    }

    float3 environment = environment_color * environment_intensity;
    if (environment_texture_id != INVALID_INDEX) {
        environment *= scene_textures[
            NonUniformResourceIndex(environment_texture_id * 2)].Sample(
                scene_samplers[0],
                environment_uv(normal)).rgb;
    }

    float metallic = material.surface.x;
    if (material.texture_bindings_1.y != INVALID_INDEX) {
        const uint binding_id = material.texture_bindings_1.y;
        const TextureBinding binding = texture_bindings[binding_id];
        metallic = select_channel(
            sample_binding(binding_id, uv),
            binding.channel);
    }
    metallic = saturate(metallic);
    const float3 view_direction = normalize(
        camera_world_position - input.world_position);
    const float3 light_direction = normalize(float3(0.45, 0.75, -0.55));
    const float3 half_vector = normalize(
        light_direction + view_direction);
    const float n_dot_l = saturate(dot(normal, light_direction));
    const float specular_power = lerp(256.0, 4.0, roughness);
    const float specular_strength = pow(
        saturate(dot(normal, half_vector)),
        specular_power);
    const float3 f0 = lerp(
        float3(0.04, 0.04, 0.04),
        albedo.rgb,
        metallic);
    const float3 diffuse = albedo.rgb * (1.0 - metallic);

    const float3 lit =
        (diffuse + f0 * specular_strength) * n_dot_l +
        albedo.rgb * environment +
        emissive;

    return float4(lit, opacity);
}
