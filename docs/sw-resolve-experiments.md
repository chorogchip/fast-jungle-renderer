# Software visibility resolve experiments

## Purpose

These tests isolate the cost introduced by merging the HW visibility/depth
result with the clustered SW 64-bit depth/primitive key. They use the selected
192-vertex / 128-triangle clusters and the opaque LOD4+ / alpha LOD4+ policy.

The formal protocol is the same as the workload experiment: 300 warm-up
frames, 60 GPU Trace frames, base clocks, and the real-time shader profiler.

## Results

| Resolve case | GPU frame time (ms) | Notes |
| --- | ---: | --- |
| HW-only, SW merge compiled out | 7.783155 representative | Three runs: 7.815200, 7.783155, 7.669810; output matches HW-only |
| HW-only, current empty-key merge | 7.917490 representative | Three runs from the matched workload test |
| Async hybrid, key/depth winner test only | 5.601535 | One diagnostic run; SW winner writes a constant |
| Async hybrid, plus primitive identity decode | 5.600000 | One diagnostic run; SW winner writes a constant after batch/cluster/triangle decode |
| Async hybrid, full reconstruction and shading | 5.771805 representative | Three valid-output runs |

The full-screen SW-key load and winner branch cost about 0.134335 ms even when
the key is empty everywhere. This is the clean output-equivalent upper bound
for optimizing the merge itself in this camera.

The frame-local primitive decode is below measurement resolution: adding the
batch lookup, dense `local_work` divide/remainder, cluster lookup, and three
local vertex-index loads changed 5.601535 ms to 5.600000 ms. The remaining
approximately 0.172 ms is the geometry reconstruction and material shading of
pixels won by SW visibility. That shading is useful work, not an avoidable
decode tax.

The key-only and primitive-only shaders deliberately change SW-winner pixels
to a constant color and are diagnostic measurements, not correct renderers.
Their binaries and traces are preserved as:

- `sw-v192-t128-opaque4-alpha4-resolve-key-only`
- `sw-v192-t128-opaque4-alpha4-resolve-primitive-only`
- `sw-v20-hw-only-resolve-no-merge`

The branch tip restores the full, correct resolve shader.

## Decision

Keep the current dense primitive ID decode. It is not a useful optimization
target.

A per-tile SW-presence mask could avoid the 64-bit key load in tiles with no SW
coverage, but its measured best-case ceiling here is only about 0.134 ms. It
would also add producer-side mask writes and another resolve branch. Do not add
that structure unless a wider resolution/camera sweep shows a materially
larger merge cost.

The earlier HW raster + D32 + typed 64-bit UAV atomic experiment remains a
separate output-path candidate. It improved opaque-only visibility from 2.038
ms to 1.967 ms (0.071 ms / 3.5%), with atomic activity itself low. That test
used a hashed, non-decodable primitive payload; production adoption still
requires a frame-local decodable batch/local-primitive encoding and slicing.
