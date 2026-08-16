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
| 34 | Opaque-only 1080p, HW raster/depth plus 64-bit typed-UAV atomic min | Test whether bypassing CROP with a depth-prefiltered atomic visibility key is faster |
| 35 | 34 with a regular 64-bit typed-UAV store | Separate the atomic cost from the UAV/CROP-routing change |
| 36 | 02 with each active draw left as one command | Control for same-work draw splitting |
| 37 | 36 with each instance range split across at most two commands | Test two-way draw/partition granularity |
| 38 | 36 with each instance range split across at most four commands | Test four-way draw/partition granularity |
| 39 | 36 with each instance range split across at most eight commands | Test eight-way draw/partition granularity |

Variants 02 through 15, 17 through 27, and 36 through 39 use the isolated opaque pass and 1x1 viewport unless the row says otherwise. Instance-count and index-count caps deliberately change total vertex work; use them to inspect launch behavior and normalize throughput, not as direct frame-time optimizations. Variants 36 through 39 instead preserve each draw's complete visible-instance range and only divide it into more indirect commands.

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
| HW raster + 64-bit UAV atomic | 1.967 | 1.263 | 510.2 | 2996 | CROP is bypassed; L2 atomic activity is only 1.73% |
| HW raster + 64-bit UAV store | 1.990 | 1.283 | 518.7 | 3013 | Nearly identical to the atomic path |
| Same work, at most eight commands per draw | 1.903 | 1.303 | 504.9 | 2889 | Draw count rises from 120 to 714 without improving residency or time |

The instance/index supply sweep is continuous: 27.5k launched VTG warps costs about 0.92 ms, 73.1k costs 1.06 ms, 173.6k costs 1.32 ms, 196.7k costs 1.39 ms, 347.8k costs 1.80 ms, and the full 383.8k costs 1.94 ms. Draw count stays at 120. This rules out a shortage of draw calls and shows geometry work scaling normally.

At 1080p, removing the color/PS path reduces opaque-only time from 2.038 ms to 1.60-1.64 ms. Changing the color target from R32G32_UINT to R32_UINT or R16_UINT only reaches 2.02 or 1.99 ms. The cost is therefore associated with enabling the visibility color/PS path, not primarily its 64-bit bandwidth.

Keeping HW rasterization and the D32 early-depth path while replacing the visibility render target with a typed `R32G32_UINT` UAV atomic reduces opaque-only time from 2.038 ms to 1.967 ms: 0.071 ms, or 3.5%. CROP throughput falls from 4.60% to 0.07%, while L2 atomic-input active cycles reach only 1.73%. A non-atomic 64-bit UAV store measures 1.990 ms. The 0.023 ms difference between the two UAV variants is too small to make the atomic itself a meaningful limiter in this scene. Early depth is important here: it prevents the pixel shader from atomically processing all covered fragments.

This experiment measures the output mechanism, not a production visibility encoding. The current HW visibility identity occupies 64 bits by itself, so variant 34 hashes it into the key's low 32 bits. A decodable opaque-only encoding is feasible for the captured workload as `[batch:8 | local primitive:24]`, where `local primitive = local instance * triangle count + triangle`. There are 134 active opaque draws and the largest draw contains about 2.34 million triangle-instances, below the 24-bit limit. Production use would therefore require a frame-local batch descriptor and the same range-slicing rule used by the SW path.

## Fixed world/primitive scheduling analysis

NVIDIA defines the world pipe as the Primitive Distributor, Vertex Attribute Fetch, and the Primitive Engine/VPC path. The Primitive Distributor fetches indices and sends triangles to the vertex shader; PES/VPC then orchestrates VTG shader data and clipping/culling before rasterization. The screen pipe starts at raster and continues through PROP, ZROP, and CROP. See the [Nsight Graphics system architecture guide](https://docs.nvidia.com/nsight-graphics/UserGuide/gpu-trace-system-architecture.html).

The experiments locate the opaque limiter in the classic world/VTG work-production path, but do not prove that one named fixed-function unit is saturated:

| Experiment | Frame ms | Evidence |
|---|---:|---|
| Opaque 1x1 | 1.936 | 383.8k VTG warps launched |
| All output clipped | 1.894 | Same 383.8k VTG warps; VPC cull-input count collapses from 5.20M to 193 |
| No vertex input | 1.886 | VAF throughput falls from 10.8% to 0.94% |
| No dependent loads | 1.894 | Active VS warps fall from 1.26 to 0.28 |
| Double opaque submit | 2.994 | VTG launches double to 767.5k and time rises with the work |

The same-work split experiment directly tests draw-level scheduling granularity:

| Maximum commands per original draw | Actual draws | Input primitives | GPU frame ms | Active VS warps/SM | World Pipe | PDA input | VAF |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 120 | 7,095,820 | 1.8821 | 1.291 | 15.38% | 10.78% | 11.13% |
| 2 | 222 | 7,095,820 | 1.8872 | 1.297 | 15.33% | 10.75% | 11.10% |
| 4 | 374 | 7,207,790 | 1.8913 | 1.306 | 15.89% | 10.90% | 11.24% |
| 8 | 714 | 7,095,820 | 1.9026 | 1.303 | 15.22% | 10.66% | 11.01% |

The one-, two-, and eight-way captures process exactly the same 7,095,820 input primitives. Increasing the actual draw count almost sixfold does not raise VS residency or any exposed world-pipe rate, and costs 0.0205 ms at eight-way split. The four-way capture contains 1.58% more visible primitive work than the matched captures and is retained only as corroborating data. Its repeat capture was blocked by a transient Nsight performance-counter lock. The matched captures are sufficient to reject lack of independent draws or coarse per-draw distribution as the limiter. Extra command boundaries add front-end work and slightly reduce VTG warp packing; they do not expose idle shader capacity.

The mechanism is an arrival-rate limit, not an SM-capacity limit. The fixed graphics path consumes the indexed primitive stream, forms/reuses vertices, launches short VTG warps, retains their output attributes, and reconstructs primitives for VPC. The normal VS retires quickly, so only about 1.2 warps per SM are resident at once. Artificially lengthening the VS makes more in-flight warps accumulate without improving primitive arrival rate; this raises residency until the separate approximately eight-warp VTG admission ceiling becomes visible.

The composite World Pipe throughput is only about 15%, and its exposed subunits are also far below 100%. Therefore the data does **not** justify saying "PD is saturated", "ISBE throughput is saturated", or "TRAM is the root cause". The narrowest supported conclusion is that classic primitive-to-VTG scheduling, including an unexposed admission/partition/latency constraint, controls the rate. Nsight's nominal peak-throughput counters do not expose which internal queue or partition causes that rate.

The useful ways to improve this path, in priority order, are:

1. Reduce triangle-instance work before it reaches the world pipe: retain coarser LODs farther away, use tighter low-LOD foliage proxies, and cull static raster clusters before emitting work.
2. Use the clustered mesh-shader path selectively for small-triangle LODs. It bypasses classic IA/PD/VAF vertex formation, restores explicit vertex reuse, and permits cluster culling before primitive export. It still pays VPC, raster, depth, and visibility output costs, so large/near geometry should remain on the classic path.
3. Use clustered SW raster selectively where bypassing both classic primitive scheduling and alpha-test work outweighs compute raster/atomic cost. Async overlap is an additional benefit, not the primary explanation.
4. Treat HW raster plus atomic visibility as an independent output optimization. It saves about 0.07 ms for opaque here but leaves World Pipe throughput, launched VTG work, and VS residency essentially unchanged.

More index reordering, draw merging, and draw splitting are low-priority for this capture. Removing VAF work and all dependent loads barely changes time, the cooker already runs vertex-cache optimization, and increasing the command count from 120 to 714 with unchanged primitive work does not improve throughput. The remaining fixed rate is finer-grained than an ExecuteIndirect command: primitive/VTG work formation or an internal queue and partition that Nsight's exposed utilization counters do not name.

## Interpretation

The dependent loads are real latency, but they are not the current throughput limiter. Removing all of them cuts active VS residency from about 1.26 to 0.28 warps/SM without reducing frame time. Adding arithmetic does the inverse: residency and ISBE allocation grow while time remains flat until the shader becomes sufficiently long. Low baseline occupancy is therefore a consequence of VTG warp arrival rate multiplied by a short warp lifetime, not proof that ISBE prevents additional baseline warps from residing.

The approximately eight-warp plateau for a deliberately long VS is real, so an internal VTG admission or graphics resource-partition limit may still exist. These measurements do not identify that limit specifically as ISBE or TRAM. They do show that the normal shader operates far below it and is limited elsewhere in the graphics geometry/output path.

VAF, output size, dependent memory, screen resolution, draw count, and render-target bit width have each been excluded as the primary cause. The remaining evidence points to fixed graphics world-pipeline/primitive scheduling plus visibility color-output coupling. Mesh and software raster paths remain useful because they bypass parts of that fixed graphics path; their benefit should not be described as merely filling nominally unused SM warp slots.
