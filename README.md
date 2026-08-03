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
In-memory source scene
  ↓ FastJungleCooker
JungleRuins.fjscene
  ↓ JungleSceneFile runtime reader
GPU renderer
```

The cooker currently writes a lossless version 0 scene file, and the runtime
renderer reads that file without linking OpenUSD. Run either cooker preset
once to create `assets/cooked/JungleRuins.fjscene`; both renderer presets then
use that file. The first cooker build creates a shared Release OpenUSD install
under `out/deps`, which both Debug and Release cooker builds reuse. The v0 file
deliberately keeps the analyzed source data intact
and does not yet apply instance compression or GPU-oriented packing.

The offline cooker will handle:

* USD mesh, material, texture, and PointInstancer extraction
* coordinate-system normalization
* compact instance representation
* scene chunking
* GPU-oriented vertex and index layouts
* texture conversion
* meshlet and LOD generation
* compact runtime scene serialization

The runtime renderer will load only the cooked data and will not depend on OpenUSD.

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
