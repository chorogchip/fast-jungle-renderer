#pragma once

#include "SceneHandles.hpp"

#include <pxr/usd/usd/prim.h>

#include <memory>

namespace fjr::cooker::internal {

    class SceneSpace;
    class StaticSceneAssembler;
    class UsdMaterialCompiler;

    class UsdMeshCompiler final {
    public:
        UsdMeshCompiler(
            SceneSpace& scene_space,
            StaticSceneAssembler& assembler,
            UsdMaterialCompiler& material_compiler);
        ~UsdMeshCompiler();

        UsdMeshCompiler(const UsdMeshCompiler&) = delete;
        UsdMeshCompiler& operator=(const UsdMeshCompiler&) = delete;

        [[nodiscard]] MeshId compile(const pxr::UsdPrim& prim);

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace fjr::cooker::internal
