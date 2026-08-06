#ifndef FAST_JUNGLE_FORWARD_COMMON
#define FAST_JUNGLE_FORWARD_COMMON

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

cbuffer DrawConstants : register(b1) {
    uint draw_id;
};

struct DrawMetadata {
    uint material_id;
    uint transform_index;
    uint index_count;
    uint first_index;
    int base_vertex;
    uint instance_kind;
    uint raster_class;
    uint padding;
};

StructuredBuffer<DrawMetadata> draw_metadata : register(t0, space1);

#endif
