# Intel Jungle scene import

## Scope

The importer is intentionally specific to the distributed Intel Jungle Ruins
scene. It is not a general USD scene loader. The local source package is copied
to `assets/scene/JungleRuins_1_0_1b` and remains ignored by Git. Its root layer
is `USD/JungleRuins_Karma.usda`.

OpenUSD is used only by `FastJungleCooker`. `JungleUsdImporter.cpp` returns a
`JungleScene` containing standard C++ values, then destroys its `UsdStage`.
`FastJungleScene` and the renderer do not include or link OpenUSD.

## Verified source facts

The facts below were measured from the composed stage, not inferred from file
names before opening the scene.

- Z-up, `metersPerUnit = 0.01`, time range 0 through 4800.
- 33 used layers and 3 composed root children: `/root`, `/World`, and
  `/Environment`.
- 3,429 composed prims.
- 778 `PointInstancer` prims containing 8,674,676 instances.
- Every point instancer targets exactly one prototype and has matching
  `protoIndices`, position, orientation, and scale array lengths.
- 53 distinct point-prototype names.
- 197 position records are exactly `(0, 0, 0)`. They are retained as data; the
  importer does not guess whether they are exporter sentinels.
- 741 native USD instance prims referencing 21 native prototype trees.
- OpenUSD's generated native prototype numbers are normalized to stable project
  paths using their composed instance relationships; authored prim paths are
  not renamed.
- 121 mesh, 37 `GeomSubset`, 134 material, 674 shader, 1 camera, and 1 dome-light
  prim in the composed traversal.
- Traversing the native prototype trees raises the owned totals to 142 meshes,
  85 subsets, 192 materials, and 930 shaders. These are definitions, not an
  incorrect flattening of the 741 instances.
- 559 authored texture references are read from shader graphs and the dome
  light; every reference resolves to a file in the copied package.
- No composed attribute has a time sample. The authored 0..4800 timeline does
  not currently require animation storage.

The 778 point instancers divide exactly into the following verified path groups:

| Group | Instancers | Instances |
| --- | ---: | ---: |
| Anthurium | 6 | 138 |
| Grass A | 6 | 280,985 |
| Grass B | 5 | 339,865 |
| Pyramid Grass B | 5 | 44,000 |
| Pyramid Moss | 138 | 2,034,610 |
| Queen Forest | 195 | 613,806 |
| River Forest | 195 | 2,407,967 |
| River Sapling | 5 | 45,000 |
| River Seedling | 80 | 2,266,462 |
| Shrub | 4 | 11,337 |
| Shrub Sorrel | 133 | 630,176 |
| Nettle | 6 | 330 |

## Information retained

`JungleScene` retains the composed prim hierarchy and local transforms, source
paths, verified semantic kind, visibility and purpose, source layers, native
instance/prototype relations, and typed payload indices. Typed payloads retain:

- complete mesh points, topology, normals, holes, subdivision/orientation,
  material binding, all observed primvar values and optional primvar indices;
- `GeomSubset` indices, family identity, and subset material binding;
- separate point-instancer prototype-index, position, quaternion, scale,
  velocity, acceleration, angular-velocity, ID, inactive-ID, invisible-ID, and
  primvar arrays;
- full Preview Surface shader nodes, typed input values, asset paths, graph
  connections, invalid connection paths, shader outputs, and material outputs;
- camera optics and clipping range, plus its transform in the node hierarchy;
- dome-light color, intensity, exposure, transform, and texture.

The representation does not merge object kinds into one instance buffer and
does not expand either point or native instances into duplicated geometry.

## Deliberate limits

The composed USD hierarchy is preserved as it actually exists. The USD export
does not directly encode the Blender authoring package's 205 scatter systems or
terrain-cell collection hierarchy. The importer therefore does not invent a
mapping from 778 exported instancers back to those collections. The `.blend`
files remain available for a separate, evidence-based authoring-hierarchy
investigation if that distinction becomes necessary.

Exporter-specific `customData`, `userProperties`, empty light-filter
relationships, and raw USD composition arcs are not mirrored into a generic
property database. They were inspected and are not needed for the rendering
data retained above. Unsupported mesh primvar or shader value types, invalid
connections, and any future time-sampled attributes produce explicit import
diagnostics instead of being silently dropped.

The first `.fjscene` layout now serializes this representation losslessly so
the renderer can run without OpenUSD. It intentionally does not choose a
coordinate conversion, instance compression, culling structure, or final GPU
upload layout. Those decisions can be made from `JungleScene` without
spreading USD objects into runtime code. See [JungleSceneFile.md](JungleSceneFile.md)
for the version 0 contract and limits.
