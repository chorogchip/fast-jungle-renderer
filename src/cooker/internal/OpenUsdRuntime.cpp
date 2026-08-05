#include "OpenUsdRuntime.hpp"

#include "CookError.hpp"

#include <pxr/base/plug/registry.h>

#include <Windows.h>

#include <array>
#include <filesystem>
#include <mutex>
#include <string_view>
#include <vector>

namespace fjr::cooker::internal {

    namespace {

        [[nodiscard]] std::filesystem::path executable_directory() {
            std::vector<wchar_t> buffer(1024);
            for (;;) {
                const DWORD length = GetModuleFileNameW(
                    nullptr,
                    buffer.data(),
                    static_cast<DWORD>(buffer.size()));
                if (length == 0) {
                    fail("GetModuleFileNameW failed.");
                }
                if (length < buffer.size() - 1) {
                    return std::filesystem::path{
                        std::wstring_view{buffer.data(), length}}
                        .parent_path();
                }
                buffer.resize(buffer.size() * 2);
            }
        }

        void register_plugins() {
            static std::once_flag once;
            std::call_once(once, [] {
                const auto runtime_root = executable_directory() / "openusd";
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
