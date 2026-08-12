#pragma once

#include "SceneHandles.hpp"

#include <pxr/usd/usd/prim.h>

#include <memory>

namespace fjr::cooker::internal {

    class SceneSpace;
    class StaticSceneDataBuilder;
    class UsdMaterialBuilder;

    class UsdMeshBuilder final {
    public:
        UsdMeshBuilder(
            SceneSpace& scene_space,
            StaticSceneDataBuilder& scene_builder,
            UsdMaterialBuilder& material_builder);
        ~UsdMeshBuilder();

        UsdMeshBuilder(const UsdMeshBuilder&) = delete;
        UsdMeshBuilder& operator=(const UsdMeshBuilder&) = delete;

        [[nodiscard]] MeshId build(const pxr::UsdPrim& prim);

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace fjr::cooker::internal
