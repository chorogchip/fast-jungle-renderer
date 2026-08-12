#pragma once

struct ImpostorBakePixelInput
{
    float4 position : SV_POSITION;
    float3 view_normal : NORMAL;
    float2 uv : TEXCOORD0;
    float3 view_position : TEXCOORD1;
};
