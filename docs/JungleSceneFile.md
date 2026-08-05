# Jungle scene file version 5

## Purpose

The cooked runtime scene is split into two files:

- `assets/cooked/JungleRuins.fjscene`: geometry, materials, instances, camera,
  light, and flat source provenance;
- `assets/cooked/JungleRuins.fjtex`: texture pixel payload.

The renderer does not link OpenUSD. A scene and texture file are one validated
pair and must be deployed together with the same basename.

`FastJungleCooker` imports the USD scene, releases OpenUSD, generates mesh
LODs, cooks textures, and streams the file without allocating another
file-sized buffer.
`FastJungle.exe` uses only the runtime reader and does not link or deploy
OpenUSD.

All values use the native little-endian x64 representation. The fixed 40-byte
`.fjscene` header is:

| Offset | Size | Value |
| ---: | ---: | --- |
| 0 | 8 | `FJSCENE\0` magic |
| 8 | 4 | format version (`5`) |
| 12 | 4 | header size (`40`) |
| 16 | 4 | `StaticScene::Vertex` size |
| 20 | 4 | `StaticScene::SceneInfo` size |
| 24 | 8 | static payload byte count |
| 32 | 8 | expected texture payload byte count |

The reader rejects unknown versions, ABI-size mismatches, length mismatches,
truncation, and trailing bytes.

## Texture header

The fixed 24-byte `.fjtex` header is:

| Offset | Size | Value |
| ---: | ---: | --- |
| 0 | 8 | `FJTEX\0\0\0` magic |
| 8 | 4 | format version (`1`) |
| 12 | 4 | header size (`24`) |
| 16 | 8 | texture payload byte count |

`Texture::data_byte_offset` is relative to the first byte after this header.
Each `TextureMip::data_byte_offset_local` is relative to its owning texture.
The reader requires the `.fjscene` expected texture size, `.fjtex` declared
size, and physical file size to agree.

## Mesh LOD contract

Every `Mesh` owns four `MeshLod` records targeting 100%, 40%, 15%, and 4% of
its original triangle count. A LOD owns the same ordered set of `Submesh`
records as LOD0. All levels share LOD0 vertex ranges, material IDs, names, and
flags; generated levels add only index ranges. Small submeshes below 128
triangles reuse the preceding index range. Triangle alignment, small meshes,
and border-locked terrain can make aggregate counts differ slightly from the
nominal ratios.

The cooker uses pinned meshoptimizer v1.2 and simplifies from the preceding
level. Its primary path permits seam collapses while measuring position,
normal, and UV error. If disconnected non-terrain geometry still misses its
target, a position-only sloppy pass bounded by the level's remaining error
budget (and capped at 0.1) supplies the lower level. Terrain never uses that
fallback and keeps borders locked.
`MeshLod::max_deviation` stores
the accumulated maximum object-space deviation in meters and is finite and
monotonic. Auxiliary triangle and corner primvars remain defined against LOD0
topology only.

At runtime a 1-pixel projected-error threshold selects one LOD. Static meshes
use their individual bounds and scale. The current point path derives
256-instance clusters and uses each cluster's bounds and maximum instance
scale, which remains conservative until GPU instance compaction can select LOD
per instance.

## Point mesh and category structure

Point-instancer provenance is intentionally not a runtime boundary. The cooker
resolves each prototype to a mesh and local transform, requires every source
PointInstancer transform to be bit-exact identity, and stably reorders its
instances by mesh and category. All sources using the same mesh must also have
the same bit-exact local transform; disagreement is a cook error instead of a
hidden second mesh identity.

Each unique point mesh becomes one `PointMeshBatch`. Its contiguous
`PointCategorySpan` records preserve semantic distinctions such as `Grass_B`
versus `Pyramid_Grass_B` even when both use the same mesh. The Jungle scene
therefore stores 53 point mesh batches, 58 category spans, and all 8,674,676
point instances. Static mesh instances remain independent and keep their full
world matrices.

## Renderer-owned derived data

AABBs are deliberately absent from the cooked contract. At load time the
renderer derives bounds in this order:

1. LOD0 submesh bounds from vertex positions;
2. mesh bounds from LOD0 submeshes;
3. point-mesh local and point-span bounds from instances;
4. static-instance bounds from their world transforms;
5. the complete scene bound.

VFC consumes these renderer-owned bounds. Future spatial structures can
therefore be rebuilt from the compact mesh/category representation without
changing the cooked scene contract.

## Bounded-memory cooker path

OpenUSD first builds static scene data and a deduplicated texture path list.
After the stage is released, the cooker generates index-only mesh LODs, then
decodes and compresses one texture at a time into a temporary payload. The
writer creates and atomically replaces
`.fjtex` first, then creates and replaces `.fjscene` last. A failed cook cannot
publish a new scene header that points at an incomplete new texture file.

`StaticSceneReader::load_metadata` reads and validates static data without
materializing texture pixels. `StaticSceneReader::load` additionally reads the
companion payload into `texture_data` for the current renderer upload path.

## Deliberate limits

- Texture dimensions are preserved; there is not yet a configurable
  resolution cap or platform quality profile.
- The files have no chunk table or memory mapping.
- The full runtime loader still materializes texture bytes before GPU upload.
- Instance compression, GPU-oriented packing, meshlets, and per-point-instance
  GPU LOD selection remain future work.
- The format is tied to the current Windows x64 ABI. Serialized type or member
  layout changes require another scene format version.
