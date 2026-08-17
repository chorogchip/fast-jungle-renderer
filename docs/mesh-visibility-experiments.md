# Mesh visibility experiments

## Measurement protocol

- Date: 2026-08-17
- Resolution: 1920x1080
- LOD projection: current viewport height
- GPU clocks: Nsight Graphics base clocks
- Each accepted result: a new process, 300 warm-up frames, then 60 captured frames
- Screening: Top-Level Triage without real-time shader profiling
- Formal comparison: Top-Level Triage with real-time shader profiling
- Representative formal value: median of three per-run frame-time medians
- Executables, shaders, cooked scenes, traces, and exports:
  `out/visibility-final-experiments`

The older 3840x2160 measurements used a hard-coded 1080-pixel LOD
projection. They are not selection results.

## Correctness finding

The first 64-triangle captures were invalid. The mesh shader encoded the
primitive as `cluster * 64 + triangle`, but Resolve still decoded it with
`/ 128` and `% 128`. This produced incorrect cluster lookups and visibly
corrupted shading. The cluster payload itself passed count, local vertex, and
packed triangle-index range checks, and the same scene rendered correctly on
the HW path.

Resolve was changed to use the matching 64-triangle stride, the corrected
128V/64T frame was visually verified, and every 64-triangle selection result
below was recaptured. Directories without `correct` in the affected T64
artifact names are retained only as rejected evidence.

## Cluster topology sweep

The cooker was run at BelowNormal process priority. A triangle candidate of
64T permits at most 192 local indices; 128T permits at most 384.

| Cluster cap | Clusters | Stored vertices | Triangles | Vertex fill | Triangle fill | LOD3 screening (ms) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 64V / 64T | 932,387 | 58,919,828 | 30,557,072 | 98.74% | 51.21% | 6.137330 corrected |
| 64V / 128T | 928,571 | 58,882,210 | 30,557,072 | 99.08% | 25.71% | 6.209535 |
| 128V / 64T | 497,986 | 58,721,431 | 30,557,072 | 92.12% | 95.88% | 6.138915 corrected |
| 128V / 128T | 462,442 | 58,518,462 | 30,557,072 | 98.86% | 51.62% | 6.155250 |
| 192V / 64T | 478,543 | 58,902,310 | 30,557,072 | 64.11% | 99.77% | 6.210880 corrected |
| 192V / 128T | 313,880 | 58,413,934 | 30,557,072 | 96.93% | 76.06% | 6.192640 |

Increasing only the triangle cap from 64T to 128T barely reduces cluster
count at 64V, while increasing unused primitive capacity. Increasing the
vertex cap to 128V removes almost half the clusters. A further increase to
192V helps only when the triangle cap is also 128T.

## LOD cutoff retuning

The two strongest cluster shapes were retuned instead of reusing one global
LOD threshold. Results are corrected screening medians with 64 threads.

| Cluster cap | LOD2 | LOD3 | LOD4 | Selected cutoff |
| --- | ---: | ---: | ---: | --- |
| 64V / 64T | 6.107665 | 6.137330 | 6.128655 | LOD2 |
| 128V / 64T | 6.093645 | 6.138915 | 6.148080 | LOD2 |
| 128V / 128T | 6.114765 | 6.155250 | 6.136335 | LOD2 |

For all three shapes, moving the mesh path from LOD3 to LOD2 is beneficial in
this scene.

## Thread-count retuning

| Cluster cap and cutoff | 64 threads (ms) | 32 threads (ms) | Winner |
| --- | ---: | ---: | --- |
| 64V / 64T, LOD2 | 6.107665 | 6.084625 | 32 |
| 128V / 64T, LOD2 | 6.093645 | 6.048770 | 32 |
| 128V / 128T, LOD2 | 6.114765 | 6.104050 | 32 |

The smaller workgroup wins after the LOD and cluster shape are retuned.

## Formal comparison

| Configuration | Run medians (ms) | Representative (ms) |
| --- | --- | ---: |
| Matched HW, version-20 scene | 6.350335, 6.388220, 6.331935 | 6.350335 |
| Corrected general HW baseline | 6.297615, 6.328880, 6.378495 | 6.328880 |
| 192V / 128T, 64 threads, LOD3 | 6.025220, 6.188270, 6.190065 | 6.188270 |
| 128V / 64T, 32 threads, LOD2 | 6.184930, 6.220800, 6.200830 | 6.200830 |
| 128V / 128T, 32 threads, LOD2 | 6.114030, 6.108175, 6.117920 | 6.114030 |

The selected mesh configuration is 128V / 128T, 32 threads, starting at
LOD2. It is 0.236305 ms (3.72%) faster than the matched HW reference and
0.074240 ms (1.20%) faster than the earlier 192V / 128T mesh configuration.
Against the separate corrected HW baseline it is 0.214850 ms (3.40%) faster.

Mesh work still has low absolute residency: the middle formal run averaged
about 0.78 active mesh warps per SM and about 1.81 total VTG warps per SM.
The gain therefore comes from geometry scheduling and reuse, not from filling
the nominal 48 warp slots.

## Selected artifacts

- Cooked scene: `scenes/v128-t128/JungleRuins.fjscene`
- Selected binary: `binaries/mesh-v128-t128-th32-lod2`
- Formal captures: `nsight/mesh-v128-t128-th32-lod2-final-r1` through `r3`
- Corrected T64 binaries and captures include `correct` in their names
- Matched HW binary: `binaries/mesh-matched-hw-v192-t128`

The renderer's fixed Nsight runner contains the selected binary and shaders
at the time this branch is committed.
