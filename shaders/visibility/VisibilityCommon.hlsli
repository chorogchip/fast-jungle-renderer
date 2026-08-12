#pragma once

static const uint VISIBILITY_INVALID = 0xffffffffu;
static const uint VISIBILITY_SUBMESH_LOW_MASK = 0x3ffu;
static const uint VISIBILITY_TRIANGLE_MASK = 0x3fffffu;
static const uint VISIBILITY_INSTANCE_MASK = 0x7ffffffeu;
static const uint VISIBILITY_BACK_FACE_MASK = 0x80000000u;

struct OpaqueVisibilityVertexOutput
{
    float4 position : SV_Position;
    nointerpolation uint instance_id : TEXCOORD0;
};

struct AlphaVisibilityVertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    nointerpolation uint instance_id : TEXCOORD1;
};

struct OpaqueShadingVertex
{
    uint normal;
    uint uv;
};

struct AlphaVisibilityVertex
{
    uint position_xy;
    uint position_zw;
    uint uv;
};

struct VisibilityRecord
{
    uint triangle_id;
    uint submesh_id;
    uint instance_id;
    bool back_face;
};

uint2 PackVisibility(
    uint triangle_id,
    uint submesh_id,
    uint instance_id,
    bool front_face)
{
    const uint lower = triangle_id |
        ((submesh_id & VISIBILITY_SUBMESH_LOW_MASK) << 22u);
    const uint back_face = front_face ? 0u : VISIBILITY_BACK_FACE_MASK;
    const uint upper = (submesh_id >> 10u) |
        (instance_id << 1u) |
        back_face;
    return uint2(lower, upper);
}

bool IsVisibilityEmpty(uint2 visibility)
{
    return visibility.y == VISIBILITY_INVALID;
}

VisibilityRecord UnpackVisibility(uint2 visibility)
{
    VisibilityRecord record;
    record.triangle_id = visibility.x & VISIBILITY_TRIANGLE_MASK;
    record.submesh_id =
        ((visibility.x >> 22u) & VISIBILITY_SUBMESH_LOW_MASK) |
        ((visibility.y & 1u) << 10u);
    record.instance_id =
        (visibility.y & VISIBILITY_INSTANCE_MASK) >> 1u;
    record.back_face =
        (visibility.y & VISIBILITY_BACK_FACE_MASK) != 0u;
    return record;
}
