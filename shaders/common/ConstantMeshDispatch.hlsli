#pragma once

cbuffer MeshDispatchConstants : register(b1)
{
    uint visible_instance_offset;
    uint submesh_id;
};
