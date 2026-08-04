# Jungle scene file version 2

## Purpose

The cooked runtime scene is split into two files:

- `assets/cooked/JungleRuins.fjscene`: geometry, materials, instances, camera,
  light, and flat source provenance;
- `assets/cooked/JungleRuins.fjtex`: texture pixel payload.

The renderer does not link OpenUSD. A scene and texture file are one validated
pair and must be deployed together with the same basename.

`FastJungleCooker` imports the USD scene, releases OpenUSD, cooks textures, and
streams the file without allocating another file-sized buffer.
`FastJungle.exe` uses only the runtime reader and does not link or deploy
OpenUSD.

All values use the native little-endian x64 representation. The fixed 40-byte
`.fjscene` header is:

| Offset | Size | Value |
| ---: | ---: | --- |
| 0 | 8 | `FJSCENE\0` magic |
| 8 | 4 | format version (`2`) |
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

## Preserved flat source structure

The cooker records the root USDA and all authored root sublayers as
`SourceLayer` records. Each layer belongs to a flat `SourceGroup` derived from
its authored path, such as `Pyramid`, `Terrain`, `River`, `Grass_A`, or
`Grass_B`. Every `PointBatch` and `MatrixBatch` stores its source layer,
authored prim path, prototype, instance range, and authored transform data.

Referenced asset children inherit the closest authored root-sublayer owner.
The format does not copy the composed USD node hierarchy, and the cooker does
not merge independently authored matrix objects merely because they share a
prototype. All 778 PointInstancer batches and all 8,674,676 point instances are
retained.

## Renderer-owned derived data

AABBs are deliberately absent from the cooked contract. At load time the
renderer derives bounds in this order:

1. submesh bounds from vertex positions;
2. mesh bounds from submeshes;
3. prototype bounds from mesh parts and local transforms;
4. point and matrix batch bounds from instances;
5. the complete scene bound from all batches.

VFC consumes these renderer-owned batch bounds. Future spatial structures can
therefore be rebuilt from preserved static data without changing or reducing
the source-oriented cooked representation.

## Bounded-memory cooker path

OpenUSD first builds static scene data and a deduplicated texture path list.
After the stage is released, the cooker decodes and compresses one texture at a
time into a temporary payload. The writer creates and atomically replaces
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
- Instance compression, GPU-oriented packing, meshlets, and LOD generation
  remain future work.
- The format is tied to the current Windows x64 ABI. Serialized type or member
  layout changes require another scene format version.
