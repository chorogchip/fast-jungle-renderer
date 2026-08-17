# Clustered software-visibility workload experiments

## Test setup

- GPU: NVIDIA GeForce RTX 5060 Ti
- viewport: 1920 x 1080
- scene format: 20
- raster cluster: 192 vertices / 128 triangles
- GPU Trace: 300 warm-up frames, 60 captured frames, base clocks
- formal captures: real-time shader profiler enabled
- reported run time: median of the 60 `GPU frame time` samples
- reported representative: median of three independent run medians

The fixed launch executable was used for every capture. Each candidate binary
is preserved under `out/visibility-final-experiments/binaries`, and every GPU
Trace report is under `out/visibility-final-experiments/nsight`.

## Workload split screening

| Policy | GPU frame median (ms) | Difference from HW-only |
| --- | ---: | ---: |
| HW-only | 7.828765 | baseline |
| SW opaque LOD4+ only | 6.795390 | -1.033375 (-13.20%) |
| SW alpha LOD3+ only | 6.232670 | -1.596095 (-20.39%) |
| SW opaque LOD4+, alpha LOD3+ | 5.897790 | -1.930975 (-24.67%) |
| SW opaque LOD4+, alpha LOD4+ | 5.682035 | -2.146730 (-27.42%) |
| SW opaque LOD4+, alpha LOD5+ | 5.988560 | -1.840205 (-23.51%) |

Far alpha is the larger individual win in this camera. Moving alpha LOD3 to
software costs more than it saves, while waiting until LOD5 leaves useful HW
alpha work on the graphics queue. The selected policy is therefore opaque
LOD4+ and alpha LOD4+.

## Formal matched result

| Configuration | Run 1 (ms) | Run 2 (ms) | Run 3 (ms) | Representative (ms) |
| --- | ---: | ---: | ---: | ---: |
| Same-v20 HW-only | 7.973235 | 7.886575 | 7.917490 | 7.917490 |
| Selected async SW hybrid | 5.849615 | 5.771805 | 5.752850 | 5.771805 |
| Selected SW hybrid, serialized | 7.974465 | 7.996940 | 7.951360 | 7.974465 |

The selected async hybrid is 2.145685 ms, or 27.10%, faster than the matched
HW-only path. Serializing SW after HW makes it 2.202660 ms, or 27.62%, slower
than the async hybrid. Its representative time returns to approximately the
HW-only time.

The middle formal captures also show the expected workload transfer:

| Median frame metric | HW-only | Async SW hybrid |
| --- | ---: | ---: |
| async compute warps / cycle | 4.347585 | 17.996100 |
| VTG warps / cycle | 1.571040 | 1.203460 |
| compute engine active | 12.42% | 98.14% |
| total SM throughput | 22.11% | 58.70% |

The result supports the intended mechanism: small/far geometry leaves the
low-residency graphics path, the compute queue becomes fully active, and its
work overlaps the remaining HW visibility pass. The gain is not evidence that
the SW raster kernel wins when placed serially on the critical path.

## Correctness and artifacts

The selected hybrid was inspected at the fixed authored camera. No new holes
or missing tree trunks and branches were visible. The noisy close pyramid
surface is also present in the pre-mesh scene-format-17 HW baseline and is not
caused by SW visibility.

Important preserved binaries:

- `sw-v20-hw-only`
- `sw-v192-t128-opaque4-only`
- `sw-v192-t128-alpha3-only`
- `sw-v192-t128-opaque4-alpha3`
- `sw-v192-t128-opaque4-alpha4`
- `sw-v192-t128-opaque4-alpha5`
- `sw-v192-t128-opaque4-alpha4-serialized`

The selected source keeps async execution and changes only the alpha threshold
from LOD3+ to LOD4+. No serialized queue wait remains in the branch tip.
