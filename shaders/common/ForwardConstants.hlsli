#pragma once

#include "../RenderData.hlsli"
#include "CameraConstants.hlsli"


cbuffer DrawConstants : register(b1)
{
    uint visible_instance_offset;
    uint material_id;
    uint submesh_id;
};

struct VertexDecodeParams
{
    float4 position_min;
    float4 position_extent;
    float4 uv_min_extent;
};

static const uint INVALID_INDEX = 0xffffffffu;

struct ForwardPixelInput
{
    float4 position : SV_POSITION;
    float3 world_position : WORLD_POSITION;
    float3 world_normal : NORMAL;
    float2 uv : TEXCOORD0;
};