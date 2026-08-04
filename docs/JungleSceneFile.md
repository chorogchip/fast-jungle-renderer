# Jungle scene file version 1

## Purpose

`JungleRuins.fjscene` is the boundary between the OpenUSD cooker and the
runtime renderer. Version 1 is specific to the fixed Intel Jungle Ruins scene
and serializes the complete project-owned `StaticScene`; it is not a general
asset container or the final GPU layout.

The default paths are:

- source: `assets/scene/JungleRuins_1_0_1b/USD/JungleRuins_Karma.usda`
- cooked output: `assets/cooked/JungleRuins.fjscene`

`FastJungleCooker` imports the USD scene, releases OpenUSD, cooks textures, and
streams the file without allocating another file-sized buffer.
`FastJungle.exe` uses only the runtime reader and does not link or deploy
OpenUSD.

## Header

All values use the native little-endian x64 representation. The fixed 32-byte
header is:

| Offset | Size | Value |
| ---: | ---: | --- |
| 0 | 8 | `FJSCENE\0` magic |
| 8 | 4 | format version (`1`) |
| 12 | 4 | header size (`32`) |
| 16 | 4 | `StaticScene::Vertex` size |
| 20 | 4 | `StaticScene::SceneInfo` size |
| 24 | 8 | payload byte count |

The reader rejects unknown versions, ABI-size mismatches, length mismatches,
truncation, and trailing bytes.

## Payload

The payload follows `SceneData_MACRO` in `StaticScene.hpp`. Every vector starts
with a native `size_t` element count and is followed by the contiguous bytes of
its trivially copyable elements. The fixed camera, environment-light, and scene
information records follow the vectors.

Texture pixels are stored in `texture_data`. `Texture::data_byte_offset` is
relative to the beginning of that vector, while each
`TextureMip::data_byte_offset_local` is relative to its owning texture. The
texture records retain width, height, DXGI format, row pitch, slice pitch, and
mip ranges required by the runtime.

The format is intentionally tied to the current Windows x64 ABI. A change to a
serialized type, member order, or ABI size requires a format version change.

## Bounded-memory cooker path

Cooking uses two phases:

1. OpenUSD builds meshes, instances, materials, bindings, and a deduplicated
   list of resolved texture path strings. No texture pixels are decoded.
2. After all OpenUSD objects are destroyed, textures are decoded sequentially.
   The cooker generates a complete mip chain, compresses color, normal, scalar,
   and HDR environment textures to BC7, BC5, BC4, and BC6H respectively, then
   appends each result to a temporary payload file. All per-texture
   `ScratchImage` storage is released before the next texture begins.

The final writer copies that payload into the `texture_data` position in 4 MiB
chunks. It writes the other vectors directly from `StaticScene`, so it never
allocates a second buffer as large as the final file. The streaming test checks
that external and in-memory texture payload writes produce identical files.

The runtime full loader reads each serialized vector directly into its final
`StaticScene` storage. The metadata loader records the texture payload offset
and size, seeks over that range, and reads the remaining scene records without
materializing texture bytes.

## Deliberate limits

- Texture dimensions are preserved; there is not yet a configurable resolution
  cap or platform-specific texture quality profile.
- The file has no scene chunk table or memory mapping.
- Runtime `StaticSceneReader::load` still materializes the complete
  `StaticScene`, including texture bytes. Incremental renderer upload through
  the exposed texture payload range remains separate future work.
- Instance compression, GPU-oriented packing, meshlets, and LOD generation
  remain future cooker stages.
