# Mesh visibility cluster-culling experiments

## Scope

This branch tests per-raster-cluster rejection on top of the selected mesh
visibility configuration: 128 vertices, 128 triangles, 32 threads, and mesh
visibility beginning at LOD2.

The cooker prototype stored the `meshopt_computeMeshletBounds` sphere and
normal cone in each raster cluster. The mesh shader variants tested were:

- no cluster culling;
- cluster bounding-sphere frustum culling;
- normal-cone backface culling;
- normal-cone backface culling with reflected-instance winding correction.

The scene-format-21 cooked data and the experiment executables are preserved
under:

- `out/visibility-final-experiments/scenes/v21-v128-t128-bounds`
- `out/visibility-final-experiments/binaries/mesh-v128-t128-th32-lod2-cull-none-v21`
- `out/visibility-final-experiments/binaries/mesh-v128-t128-th32-lod2-sphere`
- `out/visibility-final-experiments/binaries/mesh-v128-t128-th32-lod2-cull-cone-v21`
- `out/visibility-final-experiments/binaries/mesh-v128-t128-th32-lod2-cull-cone-reflect-v21`
- `out/visibility-final-experiments/binaries/hw-v21-bounds-scene`

## Correctness result

Both culling methods failed the visual correctness gate.

The sphere variant removed visible geometry, including close branches. The
normal-cone variant also rejected visible branches. Correcting the cone axis
for negative-determinant instance transforms changed the failures but did not
make the image match the same-v21 HW visibility reference.

The cone test used meshoptimizer's documented bounding-sphere form:

```text
dot(center - camera, cone_axis)
    >= cone_cutoff * length(center - camera) + radius
```

No GPU timing is reported for sphere or cone culling. A variant that changes
the visible image is not a valid performance candidate.

The prototype implementation is retained in this branch's history, followed
by a revert. The branch tip therefore preserves the selected no-cluster-cull
implementation.

## Pyramid observation

The close pyramid view looks noisy and visually suspicious, but it is not a
regression from the cluster metadata or mesh culling work. The same default
camera was compared against the pre-mesh scene-format-17 HW baseline and the
pyramid geometry and surface pattern were the same. The pyramid is also routed
through its dedicated HW raster class, not the mesh visibility path tested
here.

For that reason, tree trunks and branches rendered by the candidate mesh path
were used for the culling correctness comparison. The existing pyramid and
foliage appearance should be investigated separately from cluster culling.

## Decision

Do not enable per-cluster sphere or cone culling in the current mesh path.
Keep the 128-vertex, 128-triangle, 32-thread, LOD2 mesh configuration without
cluster culling. Revisit cluster rejection only with a separately validated
bound transform and image-difference test; amplification-shader or pre-dispatch
culling would also be a better execution point than spending a mesh workgroup
only to emit zero primitives.
