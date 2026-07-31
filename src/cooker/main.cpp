#include <Windows.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#ifndef FASTJUNGLE_DEFAULT_SCENE_USD
#define FASTJUNGLE_DEFAULT_SCENE_USD \
    "assets/scene/JungleRuins_1_0_1b/USD/JungleRuins_Karma.usda"
#endif

#ifndef FASTJUNGLE_DEFAULT_TEXTURE_DIR
#define FASTJUNGLE_DEFAULT_TEXTURE_DIR \
    "assets/scene/JungleRuins_1_0_1b/textures"
#endif

#ifndef FASTJUNGLE_DEFAULT_COOKED_DIR
#define FASTJUNGLE_DEFAULT_COOKED_DIR "assets/cooked"
#endif

namespace {

    struct CookerOptions {
        std::filesystem::path inputUsd;
        std::filesystem::path textureDirectory;
        std::filesystem::path outputFile;
    };

    [[nodiscard]] std::filesystem::path pathFromUtf8(const char* value) {
        return std::filesystem::path(value);
    }

    void printUsage(const wchar_t* executableName) {
        std::wcout
            << L"FastJungle offline scene cooker\n\n"
            << L"Usage:\n"
            << L"  " << executableName
            << L" [--input <root.usda>] [--textures <directory>]"
            L" [--output <scene.fjscene>]\n\n"
            << L"Defaults:\n"
            << L"  --input    " << pathFromUtf8(FASTJUNGLE_DEFAULT_SCENE_USD).wstring()
            << L'\n'
            << L"  --textures "
            << pathFromUtf8(FASTJUNGLE_DEFAULT_TEXTURE_DIR).wstring()
            << L'\n'
            << L"  --output   "
            << (pathFromUtf8(FASTJUNGLE_DEFAULT_COOKED_DIR)
                / L"JungleRuins_1_0_1b.fjscene").wstring()
            << L"\n\n"
            << L"Options:\n"
            << L"  -h, --help          Show this help message.\n";
    }

    struct ParseResult {
        std::optional<CookerOptions> options;
        bool helpRequested = false;
    };

    [[nodiscard]] ParseResult parseArguments(
        int argc,
        wchar_t* argv[]) {

        CookerOptions options{
            .inputUsd = pathFromUtf8(FASTJUNGLE_DEFAULT_SCENE_USD),
            .textureDirectory = pathFromUtf8(FASTJUNGLE_DEFAULT_TEXTURE_DIR),
            .outputFile =
                pathFromUtf8(FASTJUNGLE_DEFAULT_COOKED_DIR)
                / L"JungleRuins_1_0_1b.fjscene",
        };

        for (int index = 1; index < argc; ++index) {
            const std::wstring_view argument{ argv[index] };

            if (argument == L"-h" || argument == L"--help") {
                printUsage(argv[0]);
                return {
                    .options = std::nullopt,
                    .helpRequested = true,
                };
            }

            const auto readValue = [&](std::wstring_view option)
                -> std::optional<std::filesystem::path> {

                if (index + 1 >= argc) {
                    std::wcerr << L"Missing value for " << option << L".\n";
                    return std::nullopt;
                }

                ++index;
                return std::filesystem::path{ argv[index] };
                };

            if (argument == L"--input") {
                auto value = readValue(argument);
                if (!value) {
                    return {};
                }
                options.inputUsd = std::move(*value);
            } else if (argument == L"--textures") {
                auto value = readValue(argument);
                if (!value) {
                    return {};
                }
                options.textureDirectory = std::move(*value);
            } else if (argument == L"--output") {
                auto value = readValue(argument);
                if (!value) {
                    return {};
                }
                options.outputFile = std::move(*value);
            } else {
                std::wcerr << L"Unknown argument: " << argument << L"\n\n";
                printUsage(argv[0]);
                return {};
            }
        }

        return {
            .options = std::move(options),
            .helpRequested = false,
        };
    }

    [[nodiscard]] bool validateOptions(const CookerOptions& options) {
        std::error_code error;

        if (!std::filesystem::is_regular_file(options.inputUsd, error)) {
            std::wcerr
                << L"USD root layer does not exist:\n  "
                << options.inputUsd.wstring() << L'\n';
            return false;
        }

        error.clear();
        if (!std::filesystem::is_directory(options.textureDirectory, error)) {
            std::wcerr
                << L"Texture directory does not exist:\n  "
                << options.textureDirectory.wstring() << L'\n';
            return false;
        }

        const auto outputDirectory = options.outputFile.parent_path();
        if (!outputDirectory.empty()) {
            error.clear();
            std::filesystem::create_directories(outputDirectory, error);
            if (error) {
                std::wcerr
                    << L"Could not create output directory:\n  "
                    << outputDirectory.wstring()
                    << L"\n  error code: " << error.value() << L'\n';
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] int runCooker(const CookerOptions& options) {
        std::wcout
            << L"Input USD : " << options.inputUsd.wstring() << L'\n'
            << L"Textures  : " << options.textureDirectory.wstring() << L'\n'
            << L"Output    : " << options.outputFile.wstring() << L'\n';

        // Implement the offline pipeline here or delegate it to a Cooker class:
        //
        // 1. Compose the OpenUSD stage from options.inputUsd.
        // 2. Extract meshes, materials, instances, cameras, and required metadata.
        // 3. Convert referenced textures to DDS with DirectXTex.
        // 4. Pack scene tables, geometry, and DDS payloads into options.outputFile.
        //
        // Example:
        // FastJungle::Cooker cooker;
        // return cooker.cook(options.inputUsd,
        //                    options.textureDirectory,
        //                    options.outputFile)
        //     ? EXIT_SUCCESS
        //     : EXIT_FAILURE;

        std::wcout << L"Cooker implementation is not connected yet.\n";
        return EXIT_SUCCESS;
    }

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    try {
        auto result = parseArguments(argc, argv);
        if (result.helpRequested) {
            return EXIT_SUCCESS;
        }
        if (!result.options) {
            return EXIT_FAILURE;
        }

        if (!validateOptions(*result.options)) {
            return EXIT_FAILURE;
        }

        return runCooker(*result.options);
    } catch (const std::exception& exception) {
        std::cerr << "Unhandled cooker exception: " << exception.what() << '\n';
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Unhandled cooker exception: unknown error\n";
        return EXIT_FAILURE;
    }
}