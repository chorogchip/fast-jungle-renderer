#pragma once

#include <pxr/usd/usd/stage.h>

#include <filesystem>

namespace fjr::cooker::internal {

    class OpenUsdRuntime final {
    public:
        OpenUsdRuntime() = delete;

        [[nodiscard]] static pxr::UsdStageRefPtr open_stage(
            const std::filesystem::path& root_layer);
    };

} // namespace fjr::cooker::internal
