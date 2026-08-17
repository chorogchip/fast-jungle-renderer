#pragma once

static const uint MESH_LOD_CULLED = 0xffffffffu;

static const uint MATERIAL_FLAG_IMPOSTOR = 1u << 0u;
static const uint MATERIAL_MODE_SHIFT = 1u;
static const uint MATERIAL_MODE_MASK = 3u << MATERIAL_MODE_SHIFT;
static const uint MATERIAL_MODE_TEXTURED_PBR = 1u << MATERIAL_MODE_SHIFT;
static const uint MATERIAL_MODE_CONSTANT_PBR = 2u << MATERIAL_MODE_SHIFT;
static const uint MATERIAL_MODE_WATER = 3u << MATERIAL_MODE_SHIFT;

static const uint INVALID_INDEX = 0xffffffffu;

static const uint SOFTWARE_LOCAL_WORK_CAPACITY = 1u << 17u;
static const uint SOFTWARE_BATCH_CAPACITY = 1u << 8u;
static const uint SOFTWARE_LOCAL_WORK_BITS = 17u;
static const uint SOFTWARE_TRIANGLE_BITS = 7u;
