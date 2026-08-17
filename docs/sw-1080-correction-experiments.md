# Software visibility resolution correction and cross-checks

Date: 2026-08-17

## Why these runs were repeated

The first clustered-SW workload, resolve, LOD, and integrated-HW-atomic
experiments were documented as 1920 x 1080, but their preserved executables
actually created a 3840 x 2160 client area. The packed executable literal is
`0000087000000f00` (2160 and 3840). The mesh-shader and isolated HW bottleneck
executables contain the expected `0000043800000780` literal (1080 and 1920).

The old SW comparisons remain matched comparisons because each candidate and
its HW-only baseline used the same 4K viewport. They are not 1080p results,
however, and their LOD projection scale was temporarily fixed to 1080 pixels.
This document replaces the mislabeled headline number with real 1080p and
native-4K dynamic-LOD measurements.

## Protocol

- GPU: NVIDIA GeForce RTX 5060 Ti
- scene format: 20
- selected raster clusters: 192 vertices / 128 triangles
- selected routing: opaque LOD4+ and alpha LOD4+ to SW raster
- LOD constants: 13.5-pixel error and 256-pixel impostor transition
- GPU Trace: 300 warm-up frames, 60 captured frames, base clocks
- screening value: median of 60 GPU frame samples
- formal value: median of three independent run medians
- formal captures: real-time shader profiler enabled
- cooker process priority: BelowNormal

## Correct 1920 x 1080 selection

### Workload split

| Policy | GPU frame median (ms) | Difference from HW-only |
| --- | ---: | ---: |
| HW-only | 6.166880 | baseline |
| SW opaque LOD4+ only | 5.317300 | -0.849580 (-13.78%) |
| SW alpha LOD3+ only | 4.821235 | -1.345645 (-21.82%) |
| SW opaque LOD4+, alpha LOD3+ | 4.743245 | -1.423635 (-23.09%) |
| SW opaque LOD4+, alpha LOD4+ | 4.657295 | -1.509585 (-24.48%) |
| SW opaque LOD4+, alpha LOD5+ | 4.771090 | -1.395790 (-22.63%) |

The original policy decision survives the corrected-resolution sweep: route
opaque LOD4+ and alpha LOD4+ to SW raster.

### Cluster limits

Each candidate used the selected workload policy and was recooked when its
vertex or triangle limit changed.

| Maximum vertices | Maximum triangles | GPU frame median (ms) |
| ---: | ---: | ---: |
| 64 | 64 | 5.077775 |
| 64 | 128 | 5.221090 |
| 128 | 64 | 4.824305 |
| 128 | 128 | 4.918675 |
| 192 | 64 | 4.798770 |
| 192 | 128 | 4.657295 |

The 192-vertex / 128-triangle cluster remains the best tested configuration.
Reducing triangle capacity to 64 or vertex capacity to 128 increases workgroup
count enough to outweigh any per-group occupancy benefit.

### Formal result

| Configuration | Run 1 (ms) | Run 2 (ms) | Run 3 (ms) | Representative (ms) |
| --- | ---: | ---: | ---: | ---: |
| HW-only | 6.188895 | 6.047650 | 6.053950 | 6.053950 |
| Selected async SW hybrid | 4.710800 | 4.687200 | 4.677985 | 4.687200 |

The corrected 1080p improvement is **1.366750 ms / 22.58%**.

A serialized screening run measured 6.338750 ms versus 4.657295 ms for the
async screening run. It is slower than the 6.166880 ms HW-only screen, so the
benefit still depends on overlap rather than a serial SW kernel win.

## Native 3840 x 2160 dynamic LOD

The production LOD projection now uses the actual viewport height. This keeps
the 4K image and its LOD selection consistent.

| Configuration | Run 1 (ms) | Run 2 (ms) | Run 3 (ms) | Representative (ms) |
| --- | ---: | ---: | ---: | ---: |
| HW-only | 7.745380 | 7.668220 | 7.619455 | 7.668220 |
| Selected async SW hybrid | 5.679680 | 5.608095 | 5.650940 | 5.650940 |

The native-4K improvement is **2.017280 ms / 26.31%**. The earlier 27.10%
number belongs to 4K output with fixed-1080 LOD selection; it must not be
reported as a 1080p result.

## Alternate camera and visual check

At 1080p, a deterministic foliage-stress camera 40 meters behind the authored
camera measured:

| Configuration | GPU frame median (ms) |
| --- | ---: |
| HW-only | 5.530305 |
| Selected async SW hybrid | 4.321810 |

That is a **1.208495 ms / 21.85%** improvement, so the win is not confined to
the authored starting view.

The close pyramid looks noisy because dense, tiny alpha-tested foliage lies in
front of it. The same appearance is present with SW routing disabled, the
active scene file matches the saved v20 cooked scene hash, and an older
renderer screenshot shows the same near-camera pattern. It is not a clustered
SW primitive/depth corruption.

Aligned SW/HW screenshots provide a gross-regression check:

| Camera | Mean absolute RGB difference | Pixels with max-channel difference > 32 |
| --- | ---: | ---: |
| authored 4K camera | 0.9838 / 255 | 1.94% |
| 1080p foliage-stress camera | 1.3159 / 255 | 2.23% |

This does not make the existing alpha-foliage aliasing acceptable. It means
that problem must be treated as an independent alpha coverage/LOD quality
issue rather than attributed to SW visibility.

## Decision

Keep the 192/128 clusters, opaque LOD4+ / alpha LOD4+ routing, async queue
execution, direct depth use, and dense primitive decode. Restore viewport-based
LOD projection and use 1920 x 1080 as the default test window.

Do not promote far-alpha removal based only on a mesh LOD index. A production
quality policy should also gate it by projected alpha or triangle footprint,
because the current scene already produces objectionable high-frequency alpha
coverage near the pyramid even on the HW-only path.

Corrected binaries, screenshots, and GPU Trace reports are under:

- `out/visibility-final-experiments/binaries/sw1080-*`
- `out/visibility-final-experiments/binaries/sw4k-dynamiclod-*`
- `out/visibility-final-experiments/nsight/sw1080-*`
- `out/visibility-final-experiments/nsight/sw4k-dynamiclod-*`
