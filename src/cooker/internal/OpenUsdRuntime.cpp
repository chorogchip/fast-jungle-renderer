#include "OpenUsdRuntime.hpp"

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/core/util/Process.hpp"

#include <pxr/base/plug/registry.h>

#include <array>
#include <filesystem>
#include <mutex>

namespace fjr::cooker::internal {

    using log::fail;

    namespace {

        void register_plugins() {
            static std::once_flag once;
            std::call_once(once, [] {
                const auto runtime_root =
                    util::Process::executable_directory() / "openusd";
                const std::array manifests{
                    runtime_root / "lib/usd/plugInfo.json",
                    runtime_root / "plugin/usd/plugInfo.json",
                };

                for (const auto& manifest : manifests) {
                    if (!std::filesystem::is_regular_file(manifest)) {
                        fail(
                            "Missing OpenUSD plugin manifest: ",
                            manifest.generic_string());
                    }
                    pxr::PlugRegistry::GetInstance().RegisterPlugins(
                        manifest.generic_string());
                }
            });
        }

    } // namespace

    pxr::UsdStageRefPtr OpenUsdRuntime::open_stage(
        const std::filesystem::path& root_layer) {

        register_plugins();

        const auto absolute = std::filesystem::absolute(root_layer);
        if (!std::filesystem::is_regular_file(absolute)) {
            fail(
                "Intel Jungle root layer does not exist: ",
                absolute.generic_string());
        }

        auto stage = pxr::UsdStage::Open(
            absolute.generic_string(),
            pxr::UsdStage::LoadAll);
        if (!stage) {
            fail("OpenUSD could not open: ", absolute.generic_string());
        }
        return stage;
    }

} // namespace fjr::cooker::internal
