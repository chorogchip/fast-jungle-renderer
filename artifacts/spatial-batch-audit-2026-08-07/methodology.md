# Methodology

- Input: FastJungle `FJSCENE` v8 metadata produced by the Release cooker.
- Production parity: category cell sizes and Morton/cell/source ordering mirror `SceneBatchBuilder.cpp`; clusters never cross a source batch or cell and use cap 256.
- Bounds: each LOD0 mesh AABB is transformed with `PointBatch::local_transform * PointInstance SRT`, matching `SceneBoundsBuilder.cpp`.
- Visibility: the same six-plane positive-vertex AABB/frustum test as `Frustum.hpp`. `actual` means an individual transformed instance AABB intersects; it is still conservative geometry visibility.
- Sweep: 0.5x/1x/2x category cell sizes crossed with caps 128/256/512, measured on 24 deterministic orbit cameras.
- Workload: triangle and draw figures use LOD0 intentionally so LOD policy does not hide spatial batching effects.
- Balanced score: instance waste + 0.15 × draw count relative to the current configuration. It is a comparison aid, not an engine objective function.
