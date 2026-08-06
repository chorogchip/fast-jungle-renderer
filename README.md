# FastJungle

FastJungle is a DirectX 12 renderer specialized for the Intel Jungle Ruins scene.

The project targets **1920 x 1080 at 60 FPS on a GTX 1060 Mobile**. Instead of building a general-purpose renderer, it uses a fixed scene and hardware target to enable aggressive offline processing, compact data layouts, GPU-driven culling, and scene-specific optimization.

## Target

* GPU: NVIDIA GTX 1060 Mobile
* OS: Windows
* API: DirectX 12
* Shader compiler: DXC
* Shader model: SM 6.6
* Resolution: 1920 x 1080
* Target frame rate: 60 FPS
* Quality: basic PBR, alpha-tested foliage, lighting, and shadows
* Executable: `FastJungle.exe`

## Scene

Intel Jungle Ruins contains a large amount of instanced vegetation.

| Item                    |     Count |
| ----------------------- | --------: |
| Point instances         | 8,674,676 |
| PointInstancer sources  |       778 |
| Unique prototypes       |        53 |
| Native USD instances    |       741 |
| Native USD prototypes   |        21 |
| Owned mesh definitions  |       142 |
| Scatter systems         |       205 |
| Cinematic terrain cells |        16 |
| Extended terrain cells  |        64 |

The USD stage itself contains 121 mesh prims. Importing the 21 native USD
prototype trees brings the project-owned total to 142 mesh definitions. Most
point instances belong to a small number of vegetation groups, including River
Forest, River Seedling, Pyramid Moss, and Queen Forest.

The current uncompressed instance representation uses approximately 430 MiB. Instance compression and spatial organization are therefore major parts of the project.

## Pipeline

```text
USD scene
  ↓ OpenUSD
Scene metadata + texture path manifest
  ↓ release OpenUSD, then cook one texture at a time
JungleRuins.fjscene + JungleRuins.fjtex
  ↓ JungleSceneFile runtime reader
GPU renderer
```

The cooker writes a version 7 `JungleRuins.fjscene` containing geometry,
materials, instances, and runtime component categories, plus a companion
`JungleRuins.fjtex` containing texture pixels. The runtime renderer reads both
without linking OpenUSD. Run the Release cooker preset once to create both
files under `assets/cooked`; both renderer presets then use them. The first
cooker build creates a shared Release OpenUSD install
under `out/deps`, which both Debug and Release cooker builds reuse. The cooker
releases the OpenUSD stage before decoding textures, generates mip chains and
role-appropriate BC4, BC5, BC6H, or BC7 data one texture at a time, and writes
that data through a temporary payload. The final writer streams it into the
companion texture file without allocating a second file-sized memory buffer.
Before texture cooking, meshoptimizer generates 100%, 40%, 15%, and 4% mesh
LODs that share LOD0 vertices and add index ranges only. The runtime selects a
level using a 1-pixel projected-error threshold; static instances are selected
individually and points use renderer-derived 256-instance clusters.
Point instancers with the same resolved mesh and category are merged into one
`PointBatch`, without preserving source-instancer boundaries. Spatial bounds
are not cooked as authoritative data: the renderer derives them from the
static data before VFC. The v7 files do not yet apply instance compression or
GPU-oriented vertex packing.

The offline cooker will handle:

* USD mesh, material, texture, and PointInstancer extraction
* coordinate-system normalization
* compact instance representation
* scene chunking
* GPU-oriented vertex and index layouts
* texture resolution profiles and streaming layout
* meshlet generation and per-point-instance GPU LOD selection
* compact runtime scene serialization

The runtime renderer will load only the cooked data and will not depend on OpenUSD.

## Camera controls

The renderer uses the camera authored in the cooked `StaticScene` without an
extra scene-specific pose offset. Use `W/A/S/D` to move, `Q/E` to descend or
ascend, the arrow keys to look, and hold Shift to move faster.

Pass `--force-lod0` to render an interactive LOD0 preview. Because the full
Jungle scene expands to millions of instances, this mode keeps only LOD0 draws,
orders them nearest-first, and caps one frame's index invocations to avoid a
Windows GPU timeout. Normal runs retain projected-error LOD selection without
that preview budget.

Pass `--force-coarsest-lod --overview` to select each mesh's last, least
detailed LOD and frame the complete cooked world bounds. This overview includes
both point-instanced vegetation and all static scene objects; it does not alter
the authored camera used by normal runs.

The current importer boundary and the facts verified from the distributed scene
are documented in [docs/JungleSceneImport.md](docs/JungleSceneImport.md).
The current binary contract and its deliberate limits are documented in
[docs/JungleSceneFile.md](docs/JungleSceneFile.md).

## Rendering Direction

Planned rendering features include:

* frustum culling
* GPU instance compaction
* indirect drawing
* meshlet culling
* Hi-Z occlusion culling
* visibility-buffer rendering
* alpha-tested foliage
* basic PBR lighting
* shadows
* optional software rasterization experiments
