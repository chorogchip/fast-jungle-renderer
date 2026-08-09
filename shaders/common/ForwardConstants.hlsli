#pragma once

#include "../RenderData.hlsli"
#include "CameraConstants.hlsli"

cbuffer DrawConstants : register(b1)
{
    uint visible_instance_offset;
    uint material_id;
};

static const uint INVALID_INDEX = 0xffffffffu;

struct ForwardPixelInput
{
    float4 position : SV_POSITION;
    float3 world_position : WORLD_POSITION;
    float3 world_normal : NORMAL;
    float2 uv : TEXCOORD0;
};
