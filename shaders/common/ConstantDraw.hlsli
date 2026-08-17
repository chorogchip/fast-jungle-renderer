#pragma once

cbuffer DrawConstants : register(b1)
{
    uint visible_instance_offset;
    uint material_id;
    uint submesh_id;
    uint visibility_batch_id;
    uint triangle_count;
};
