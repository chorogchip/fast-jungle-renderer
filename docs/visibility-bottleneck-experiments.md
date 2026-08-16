# Visibility bottleneck experiments

All measurements in this experiment set use a 1920x1080 window and viewport-driven LOD selection. The saved executables and their matching DXIL files are under `out/visibility-experiments/binaries`.

## Variants

| ID | Change | Question |
|---|---|---|
| 00 | Complete HW visibility and resolve | Real 1080p baseline |
| 01 | Opaque only, resolve omitted | Cost of the opaque workload |
| 02 | 01 with a 1x1 viewport | Does downstream raster/fragment work back-pressure VS? |
| 03 | All instances use the first visible instance | Does per-instance dependent addressing matter? |
| 04 | Fixed instance ID 0 | What is the cost of the visible-instance lookup? |
| 05 | Constant identity instance transform | What is the cost of the instance buffer lookup? |
| 06 | Constant instance transform and vertex decode | What is the cost of all dependent structured-buffer loads? |
| 07 | 02 plus 8 dependent FMAs | VS latency/admission response |
| 08 | 02 plus 32 dependent FMAs | VS latency/admission response |
| 09 | 02 plus 128 dependent FMAs | VS latency/admission response and saturation point |
| 10 | Position-only VS output and constant instance ID in PS | Minimum ISBE/TRAM attribute footprint |
| 11 | 02 plus four consumed float4 VS outputs | Attribute-allocation sensitivity |
| 12 | 02 plus eight consumed float4 VS outputs | Attribute-allocation sensitivity |
| 13 | No PS or color target; depth only | Remove PS, TRAM interpolation, and CROP work |
| 14 | PS retained but color write mask zero | Isolate color-output pressure |
| 15 | Depth disabled and no DSV bound | Isolate depth/ZROP pressure |
| 16 | Opaque-only 960x540 viewport | Screen-work scaling between 1x1 and 1080p |
| 17 | Instance count capped to 1 per draw | VTG launch/admission granularity |
| 18 | Instance count capped to 32 per draw | VTG launch/admission granularity |
| 19 | Instance count capped to 128 per draw | VTG launch/admission granularity |
| 20 | Index count capped to 384 per draw | Geometry supply per draw |
| 21 | Index count capped to 1536 per draw | Geometry supply per draw |
| 22 | Index count capped to 6144 per draw | Geometry supply per draw |
| 23 | All primitives forced outside the clip volume | Remove raster work while retaining the transform path |
| 24 | Back-face culling disabled | Primitive/raster throughput sensitivity |
| 25 | No vertex input or input layout | Remove VAF attribute fetch |
| 26 | Constant vertex decode, normal visible-instance and transform chain | Isolate the decode-parameter lookup |
| 27 | Opaque ExecuteIndirect submitted twice | Is occupancy limited by total queued work? |
| 28 | 02 plus 512 dependent FMAs | Find the point where shader execution becomes critical |
| 29 | 02 plus 2048 dependent FMAs | Measure the long-shader VTG residency plateau |
| 30 | Opaque-only 1080p depth-only PSO | Measure the real-resolution cost of the color/PS path |
| 31 | Opaque-only 1080p with color writes disabled | Separate color/PS coupling from depth work |
| 32 | Opaque-only 1080p R32_UINT color output | Test whether 64-bit visibility bandwidth is limiting |
| 33 | Opaque-only 1080p R16_UINT color output | Establish the render-target width trend |

Variants 02 through 15 and 17 through 27 use the isolated opaque pass and 1x1 viewport unless the row says otherwise. Instance-count and index-count caps deliberately change total vertex work; use them to inspect launch behavior and normalize throughput, not as direct frame-time optimizations.

## Nsight collection

GPU Trace uses the Blackwell GB20x Top-Level Triage metric set, real-time shader profiling, and base clocks. Nsight Graphics 2026.3 reports that multi-pass metrics are unavailable on Blackwell GB20x, so the sweep uses the supported single-pass metric set. Every result below is the median of 60 or 120 captured frames after warm-up. The complete `.ngfx-gputrace` reports and exported spreadsheets are under `out/visibility-experiments/nsight`.

## Key results

| Variant | GPU frame ms | Active VS warps/SM | VTG latency | ISBE bytes | Relevant observation |
|---|---:|---:|---:|---:|---|
| Full HW 1080p | 6.269 | 1.841 | 788.3 | 3420 | Reproduces the original low occupancy |
| Opaque-only 1080p | 2.038 | 1.271 | 487.2 | 2732 | Isolated real-resolution reference |
| Opaque-only 1x1 | 1.936 | 1.257 | 471.9 | 2803 | Screen work is only a small part of opaque cost |
| No visible-instance chain | 1.885 | 0.925 | 260.1 | 1606 | Less latency lowers residency, not frame time |
| No dependent loads | 1.894 | 0.281 | 158.2 | Dependent loads are not throughput-critical here |
| ALU 128 | 1.907 | 2.495 | 934.5 | 3834 | Added work still fits in existing slack |
| ALU 512 | 2.436 | 4.883 | 1786.7 | 5465 | Shader execution becomes critical |
| ALU 2048 | 5.120 | 7.749 | 7378.6 | 7428 | Reproduces the approximately eight-warp plateau |
| Position-only output | 1.874 | 1.292 | 502.6 | 2483 | Minimum output footprint |
| Plus four float4 outputs | 1.899 | 1.309 | 508.1 | 4551 | ISBE grows with no time change |
| Plus eight float4 outputs | 1.894 | 1.337 | 355.0 | 6281 | Baseline is not ISBE-capacity limited |
| No vertex input | 1.886 | 1.208 | 457.9 | 2030 | VAF falls 11.2% to 0.94% with no speedup |
| Double opaque submit | 2.994 | 1.583 | 389.8 | 3560 | Twice the VTG work raises residency; no hard low ceiling |

The instance/index supply sweep is continuous: 27.5k launched VTG warps costs about 0.92 ms, 73.1k costs 1.06 ms, 173.6k costs 1.32 ms, 196.7k costs 1.39 ms, 347.8k costs 1.80 ms, and the full 383.8k costs 1.94 ms. Draw count stays at 120. This rules out a shortage of draw calls and shows geometry work scaling normally.

At 1080p, removing the color/PS path reduces opaque-only time from 2.038 ms to 1.60-1.64 ms. Changing the color target from R32G32_UINT to R32_UINT or R16_UINT only reaches 2.02 or 1.99 ms. The cost is therefore associated with enabling the visibility color/PS path, not primarily its 64-bit bandwidth.

## Interpretation

The dependent loads are real latency, but they are not the current throughput limiter. Removing all of them cuts active VS residency from about 1.26 to 0.28 warps/SM without reducing frame time. Adding arithmetic does the inverse: residency and ISBE allocation grow while time remains flat until the shader becomes sufficiently long. Low baseline occupancy is therefore a consequence of VTG warp arrival rate multiplied by a short warp lifetime, not proof that ISBE prevents additional baseline warps from residing.

The approximately eight-warp plateau for a deliberately long VS is real, so an internal VTG admission or graphics resource-partition limit may still exist. These measurements do not identify that limit specifically as ISBE or TRAM. They do show that the normal shader operates far below it and is limited elsewhere in the graphics geometry/output path.

VAF, output size, dependent memory, screen resolution, draw count, and render-target bit width have each been excluded as the primary cause. The remaining evidence points to fixed graphics world-pipeline/primitive scheduling plus visibility color-output coupling. Mesh and software raster paths remain useful because they bypass parts of that fixed graphics path; their benefit should not be described as merely filling nominally unused SM warp slots.
