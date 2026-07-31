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
| Native instances        |       142 |
| Scatter systems         |       205 |
| Cinematic terrain cells |        16 |
| Extended terrain cells  |        64 |

Most instances belong to a small number of vegetation groups, including River Forest, River Seedling, Pyramid Moss, and Queen Forest.

The current uncompressed instance representation uses approximately 430 MiB. Instance compression and spatial organization are therefore major parts of the project.

## Pipeline

```text
USD scene
  бщ OpenUSD
In-memory source scene
  бщ offline cooker
FastJungle custom scene binary
  бщ runtime loader
GPU renderer
```

The offline cooker will handle:

* USD mesh, material, texture, and PointInstancer extraction
* coordinate-system normalization
* compact instance representation
* scene chunking
* GPU-oriented vertex and index layouts
* texture conversion
* meshlet and LOD generation
* custom scene binary serialization

The runtime renderer will load only the cooked data and will not depend on OpenUSD.

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
