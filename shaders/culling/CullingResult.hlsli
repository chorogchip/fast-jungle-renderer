#pragma once

#include "../common/RenderData.hlsli"

static const uint CULL_RESULT_CULLED = 0xffffu;
static const uint CULL_MAX_CONVENTIONAL_LODS = 7;
static const uint CULL_MAX_IMPOSTOR_DIRECTIONS = 8;
static const uint CULL_BUCKET_COUNT =
    CULL_MAX_CONVENTIONAL_LODS + CULL_MAX_IMPOSTOR_DIRECTIONS;
static const uint CULL_RESERVATION_STRIDE = 16;
static const uint CULL_RESULT_BUCKET_MASK = 0xfu;

bool TryGetCullBucket(
    Mesh mesh,
    uint mesh_lod_id,
    out uint bucket)
{
    if (mesh_lod_id >= mesh.lod_offset &&
        mesh_lod_id - mesh.lod_offset < mesh.lod_count &&
        mesh_lod_id - mesh.lod_offset < CULL_MAX_CONVENTIONAL_LODS)
    {
        bucket = mesh_lod_id - mesh.lod_offset;
        return true;
    }

    if (mesh.impostor_direction_count <=
            CULL_MAX_IMPOSTOR_DIRECTIONS &&
        mesh_lod_id >= mesh.impostor_card_lod_offset &&
        mesh_lod_id - mesh.impostor_card_lod_offset <
            mesh.impostor_direction_count)
    {
        bucket = CULL_MAX_CONVENTIONAL_LODS +
            mesh_lod_id - mesh.impostor_card_lod_offset;
        return true;
    }

    bucket = 0;
    return false;
}

uint CullBucketToMeshLod(Mesh mesh, uint bucket)
{
    return bucket < CULL_MAX_CONVENTIONAL_LODS
        ? mesh.lod_offset + bucket
        : mesh.impostor_card_lod_offset +
            bucket - CULL_MAX_CONVENTIONAL_LODS;
}

uint PackCullResult(uint bucket, uint local_rank)
{
    return bucket | (local_rank << 4);
}

uint CullResultBucket(uint packed_result)
{
    return packed_result & CULL_RESULT_BUCKET_MASK;
}

uint CullResultLocalRank(uint packed_result)
{
    return packed_result >> 4;
}
