# Hybrid HW atomic visibility experiment

Date: 2026-08-17

## Question

The isolated HW opaque experiment reduced 1080p opaque visibility from 2.038
ms to 1.967 ms by replacing the 64-bit visibility render target output with a
depth-prefiltered 64-bit UAV atomic. That prototype used a non-decodable hash.
This experiment tests a decodable key inside the selected SW hybrid renderer.

## Prototype

The prototype did not add another full-screen buffer or merge. HW opaque and
SW raster shared the existing 64-bit key:

```
[ depth 32 ][ batch 8 | local primitive 24 ]
```

HW batch descriptors stored the visible-instance range, submesh, and triangle
count. Resolve decoded `local primitive` into the original instance and
triangle. Pyramid, terrain, river, and HW alpha-test visibility continued to
use the existing render target. HW opaque retained D32 early depth and used a
shader-model 6.6 `InterlockedMin64` pixel shader with no color target.

The key clear was moved before the direct/compute queue split so both raster
paths could safely write the same buffer. The default-camera output matched the
existing hybrid renderer in the visual correctness check.

## Result

1920x1080, default camera, 192-vertex/128-triangle cooked clusters, opaque LOD
4+ and alpha LOD 4+ routed to SW raster:

| Variant | Median GPU frame (ms) | Delta |
| --- | ---: | ---: |
| Existing hybrid | 5.682035 | Reference |
| Hybrid plus decodable HW atomic | 5.719980 | +0.037945 ms (+0.67%) |

Both values use the 300-frame warm-up, 60-frame screening GPU Trace protocol at
base clocks without the real-time shader profiler.

## Decision

Reject the integrated HW atomic path. The selected hybrid already moves the
small-triangle opaque workload to SW raster, leaving too little HW opaque work
for the isolated 0.071 ms CROP saving to survive the added batch allocation,
atomic pixel shader, and key decode costs. A separate key texture would be even
worse: the measured empty-key full-screen merge cost is about 0.134 ms.

The prototype and its revert are retained in this branch. The executable and
matching DXIL are under
`out/visibility-final-experiments/binaries/sw-v192-t128-opaque4-alpha4-hw-atomic`.
The Nsight report is under
`out/visibility-final-experiments/nsight/sw-v192-t128-opaque4-alpha4-hw-atomic-screen`.
