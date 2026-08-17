# Mesh visibility experiments

## Measurement protocol

- Date: 2026-08-17
- Resolution: 1920x1080
- LOD projection: current viewport height
- Scene: RasterCluster scene version 20, 192 vertices / 128 triangles
- GPU clocks: Nsight Graphics base clocks
- Each accepted result: a new process, 300 warm-up frames, then 60 captured frames
- Representative value: median of three per-run frame-time medians
- Matching executables, shaders, raw traces, and exports are under
  `out/visibility-final-experiments`.

The older mesh measurements used a 3840x2160 window while keeping a hard-coded
1080-pixel LOD projection. They are not used below.

## Matched hardware reference

The hardware-only reference uses the same version-20 cooked scene and renderer
build, with mesh routing disabled.

| Run | GPU frame median (ms) |
| --- | ---: |
| 1 | 6.350335 |
| 2 | 6.388220 |
| 3 | 6.331935 |
| Representative | 6.350335 |

## Thread-count sweep

Both candidates route opaque, single-sided LOD5 and farther geometry through
the mesh path.

| Cluster | Threads | Run medians (ms) | Representative (ms) | Change vs hardware |
| --- | ---: | --- | ---: | ---: |
| 192V / 128T | 32 | 6.303235, 6.310385, 6.229550 | 6.303235 | -0.047100 (-0.74%) |
| 192V / 128T | 64 | 6.310910, 6.246400, 6.224400 | 6.246400 | -0.103935 (-1.64%) |

At 1080p, 64 threads wins by 0.056835 ms. This reverses the old 4K result,
which was measured with the incorrect LOD projection and is therefore not used
as a selection result.

## Mesh cutoff sweep

The thread count is fixed at 64. LOD3 uses the three input-free repetitions
r3-r5. The interrupted camera-input captures are preserved with `invalid` in
their artifact directory names and are excluded.

| Mesh path starts at | Accepted run medians (ms) | Representative (ms) | Mesh active warps | Change vs hardware |
| --- | --- | ---: | ---: | ---: |
| LOD5 | 6.310910, 6.246400, 6.224400 | 6.246400 | 0.423 | -0.103935 (-1.64%) |
| LOD4 | 6.327905, 6.239760, 6.272510 | 6.272510 | 0.642 | -0.077825 (-1.23%) |
| LOD3 | 6.025220, 6.188270, 6.190065 | 6.188270 | 0.702 | -0.162065 (-2.55%) |
| LOD2 | 6.266355, 6.194195, 6.214130 | 6.214130 | 0.789 | -0.136205 (-2.14%) |

LOD2 raises mesh residency but does not improve frame time. The selected
configuration for this scene is therefore 192V / 128T, 64 threads, starting at
LOD3. It reduces the matched full-frame GPU time by 0.162065 ms (2.55%).

This does not make the original low VS residency disappear. Mesh work itself
also has low absolute active-warp residency. The measured gain comes from
changing geometry scheduling and reuse for the selected LODs, not from filling
the nominal 48 warp slots.

## Artifact names

- `binaries/mesh-matched-hw-v192-t128`
- `binaries/mesh-v192-t128-th32-lod5`
- `binaries/mesh-v192-t128-th64-lod5`
- `binaries/mesh-v192-t128-th64-lod4`
- `binaries/mesh-v192-t128-th64-lod3`
- `binaries/mesh-v192-t128-th64-lod2`
- Corresponding captures: `nsight/<binary-name>-rN`

