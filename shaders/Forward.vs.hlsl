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

cbuffer DrawTransformConstants : register(b1) {
    row_major float4x4 part_local_transform;
    row_major float4x4 batch_local_to_world;
};

cbuffer DrawConstants : register(b2) {
    uint instance_offset;
    uint material_id;
    uint instance_kind;
};

ByteAddressBuffer instances : register(t0);

static const uint INSTANCE_KIND_POINT = 0;
static const uint POINT_INSTANCE_SIZE = 40;
static const uint MATRIX_INSTANCE_SIZE = 64;

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct PixelInput {
    float4 position : SV_POSITION;
    float3 world_position : TEXCOORD1;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

float3 rotate_by_quaternion(float3 value, float4 quaternion) {
    const float3 twice_cross = 2.0 * cross(quaternion.xyz, value);
    return value +
        quaternion.w * twice_cross +
        cross(quaternion.xyz, twice_cross);
}

float4 transform_position_rows(float4 position, uint byte_offset) {
    const float4 row0 = asfloat(instances.Load4(byte_offset));
    const float4 row1 = asfloat(instances.Load4(byte_offset + 16));
    const float4 row2 = asfloat(instances.Load4(byte_offset + 32));
    const float4 row3 = asfloat(instances.Load4(byte_offset + 48));
    return
        position.x * row0 +
        position.y * row1 +
        position.z * row2 +
        position.w * row3;
}

float3 transform_normal_rows(float3 normal, uint byte_offset) {
    const float3 row0 = asfloat(instances.Load3(byte_offset));
    const float3 row1 = asfloat(instances.Load3(byte_offset + 16));
    const float3 row2 = asfloat(instances.Load3(byte_offset + 32));
    return
        normal.x * row0 +
        normal.y * row1 +
        normal.z * row2;
}

PixelInput main(VertexInput input, uint local_instance_id : SV_InstanceID) {
    PixelInput output;

    float4 world_position = mul(
        float4(input.position, 1.0),
        part_local_transform);
    float3 world_normal = mul(
        input.normal,
        (float3x3)part_local_transform);

    const uint instance_id = instance_offset + local_instance_id;
    if (instance_kind == INSTANCE_KIND_POINT) {
        const uint byte_offset = instance_id * POINT_INSTANCE_SIZE;
        const float3 position = asfloat(instances.Load3(byte_offset));
        const float4 orientation = asfloat(
            instances.Load4(byte_offset + 12));
        const float3 scale = asfloat(instances.Load3(byte_offset + 28));

        world_position.xyz = rotate_by_quaternion(
            world_position.xyz * scale,
            orientation) + position;

        const float3 inverse_scale =
            sign(scale) / max(abs(scale), 1.0e-8);
        world_normal = rotate_by_quaternion(
            world_normal * inverse_scale,
            orientation);

        world_position = mul(
            world_position,
            batch_local_to_world);
        world_normal = mul(
            world_normal,
            (float3x3)batch_local_to_world);
    } else {
        const uint byte_offset = instance_id * MATRIX_INSTANCE_SIZE;
        world_position = transform_position_rows(
            world_position,
            byte_offset);
        world_normal = transform_normal_rows(
            world_normal,
            byte_offset);
    }

    output.position = mul(world_position, view_projection);
    output.world_position = world_position.xyz;
    output.normal = world_normal;
    output.uv = input.uv;
    return output;
}
