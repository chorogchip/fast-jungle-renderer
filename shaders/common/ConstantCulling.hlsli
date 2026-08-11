#pragma once

cbuffer CullingDispatchConstants : register(b1)
{
    uint indirect_draw_capacity_per_class;
    uint raster_class_count;
};
