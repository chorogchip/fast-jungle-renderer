# Third-Party Notices

FastJungle uses the third-party open-source software listed below. Each
component remains subject to its own license; the FastJungle license does not
replace or modify those terms.

The versions in this document match the dependencies pinned in
`CMakeLists.txt`.

## Direct dependencies

| Component | Version | Use in FastJungle | License |
| --- | --- | --- | --- |
| [OpenUSD](https://github.com/PixarAnimationStudios/OpenUSD/tree/v26.05) | v26.05 | USD scene import in the cooker; OpenUSD runtime files are copied beside the cooker executable | [Tomorrow Open Source Technology License 1.0 and bundled notices](https://github.com/PixarAnimationStudios/OpenUSD/blob/v26.05/LICENSE.txt) |
| [oneTBB](https://github.com/oneapi-src/oneTBB/tree/v2021.9.0) | v2021.9.0 | Parallel runtime used by OpenUSD; the oneTBB DLL is copied beside the cooker executable | [Apache License 2.0](https://github.com/oneapi-src/oneTBB/blob/v2021.9.0/LICENSE.txt) |
| [DirectXTex](https://github.com/microsoft/DirectXTex/tree/may2026) | may2026 | Texture processing in the cooker | [MIT License](https://github.com/microsoft/DirectXTex/blob/may2026/LICENSE) |
| [meshoptimizer](https://github.com/zeux/meshoptimizer/tree/v1.2) | v1.2 | Mesh optimization, LOD generation, and clustering in the cooker | [MIT License](https://github.com/zeux/meshoptimizer/blob/v1.2/LICENSE.md) |
| [OpenEXR](https://github.com/AcademySoftwareFoundation/openexr/tree/v3.4.12) | v3.4.12 | OpenEXR support used by the cooker dependency graph | [BSD 3-Clause License](https://github.com/AcademySoftwareFoundation/openexr/blob/v3.4.12/LICENSE.md) |
| [DirectX Shader Compiler](https://github.com/microsoft/DirectXShaderCompiler) | NuGet package 1.8.2505.32 | Build-time HLSL compilation when a system `dxc` executable is unavailable | [LLVM Release License and bundled notices](https://github.com/microsoft/DirectXShaderCompiler/blob/main/LICENSE.TXT) |

## Copyright notices

### DirectXTex

Copyright (c) Microsoft Corporation.

### meshoptimizer

Copyright (c) 2016-2026 Arseny Kapoulkine.

### OpenEXR

Copyright (c) Contributors to the OpenEXR Project. All rights reserved.

### DirectX Shader Compiler and LLVM

Copyright (c) 2003-2015 University of Illinois at Urbana-Champaign.
All rights reserved.

OpenUSD, oneTBB, OpenEXR, and DirectX Shader Compiler distributions contain
additional copyright and third-party notices. Refer to the complete upstream
license files linked above for those notices.

## Distribution

The FastJungle source tree does not vendor these dependencies. The build
downloads them from their upstream projects or, for DXC, may use an existing
system installation.

When distributing FastJungle binaries, include the complete, unmodified
license and notice files shipped with every distributed third-party artifact,
including applicable transitive dependencies. This overview is not a
replacement for those files.

## Intel Jungle Ruins assets

The Intel Jungle Ruins scene is downloaded separately by the user and is not
part of FastJungle. It is subject to Intel's applicable download and asset
license terms.