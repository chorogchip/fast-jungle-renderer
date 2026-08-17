# LOD and billboard threshold experiments

Date: 2026-08-17

## Setup

- 3840x2160, default camera
- cooked raster clusters: 192 vertices / 128 triangles
- SW policy: opaque LOD 4+ and alpha LOD 4+
- screening capture: 300 warm-up frames, 60 GPU Trace frames, base clocks
- formal capture: the same setup with the real-time shader profiler enabled
- frame result: median of 60 GPU frame times

The projection scale was held at the 1080p value so that resolution could not
silently change the selected LOD during this sweep. These measurements were
originally mislabeled as 1920x1080; the preserved executable contains the 4K
window literal.

## Screening results

| LOD error (px) | Billboard radius (px) | GPU frame (ms) | Result |
| ---: | ---: | ---: | --- |
| 13.5 | 256 | 5.682035 | Reference |
| 13.5 | 192 | 5.672915 | -0.009120 ms; measurement noise |
| 13.5 | 128 | 6.010205 | Rejected; slower |
| 13.5 | 64 | 7.472065 | Rejected; much slower |
| 16.0 | 192 | 5.335775 | Quality-changing bound |
| 18.0 | 192 | 5.040335 | Quality-changing bound |
| 20.0 | 192 | 4.895840 | Quality-changing bound |

Moving the billboard transition inward did not produce a speedup. At 128 px
and 64 px it increased the frame time, so the original 256 px value remains the
selected value.

Raising the LOD error threshold did reduce work, but it also deliberately
selected coarser geometry earlier. The default-camera visual check showed a
different branch and foliage distribution at 20 px. Those measurements are a
throughput/quality tradeoff, not an equal-quality renderer optimization.

## LOD 20 repeatability

The 20 px / 192 px candidate was measured three times before the visual result
was rejected:

| Run | GPU frame (ms) |
| ---: | ---: |
| 1 | 4.902255 |
| 2 | 4.895875 |
| 3 | 4.861310 |

Median of run medians: **4.895875 ms**. The number is repeatable, but it is not
used as the final performance result because image quality is not matched.

## Decision

- Keep `LOD_ERROR_THRESHOLD_PX = 13.5`.
- Keep `IMPOSTOR_TRANSITION_RADIUS_PX = 256`.
- Do not count the LOD 16/18/20 timing reduction as an implementation win.
- Any future LOD search needs multiple fixed cameras and an explicit image
  quality acceptance gate before timing can select a candidate.

Executables and Nsight captures are under
`out/visibility-final-experiments/binaries` and
`out/visibility-final-experiments/nsight`, using the names in the tables above.
