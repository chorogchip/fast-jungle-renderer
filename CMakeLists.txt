cmake_minimum_required(VERSION 3.26)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(FastJungle VERSION 0.1.0 LANGUAGES CXX)

include(CheckIPOSupported)

if(NOT WIN32)
    message(FATAL_ERROR "FastJungle supports Windows only.")
endif()

if(NOT MSVC)
    message(FATAL_ERROR "FastJungle requires the MSVC toolchain.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "FastJungle supports x64 builds only.")
endif()

# The presets use Ninja and separate build trees for each configuration.
# Keeping this project single-config also keeps ExternalProject dependencies
# from mixing Debug and Release artifacts.
if(CMAKE_CONFIGURATION_TYPES)
    message(FATAL_ERROR
        "FastJungle requires a single-config generator. "
        "Use the Ninja presets from CMakePresets.json."
    )
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build configuration" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND
   NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR
        "Unsupported CMAKE_BUILD_TYPE='${CMAKE_BUILD_TYPE}'. "
        "Only Debug and Release are supported."
    )
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

set(FASTJUNGLE_BUILD "renderer" CACHE STRING "FastJungle target to build")
set_property(CACHE FASTJUNGLE_BUILD PROPERTY STRINGS renderer cooker)

if(NOT FASTJUNGLE_BUILD STREQUAL "renderer" AND
   NOT FASTJUNGLE_BUILD STREQUAL "cooker")
    message(FATAL_ERROR "FASTJUNGLE_BUILD must be 'renderer' or 'cooker'.")
endif()

option(FASTJUNGLE_ENABLE_IPO
    "Enable Release IPO/LTCG for FastJungle"
    ON
)
option(FASTJUNGLE_BUILD_TESTS
    "Build FastJungle project tests"
    OFF
)
option(FASTJUNGLE_LOG_ABORT_TO_STDERR
    "Write fatal Logger output to stderr and exit without an abort dialog"
    OFF
)

set(FASTJUNGLE_SOURCE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(FASTJUNGLE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(FASTJUNGLE_SHADER_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(FASTJUNGLE_ASSET_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/assets")

set(FASTJUNGLE_COOKED_DIR
    "${FASTJUNGLE_ASSET_DIR}/cooked"
    CACHE PATH "Cooked scene output directory"
)

set(FASTJUNGLE_RUNTIME_DIR "${CMAKE_BINARY_DIR}/bin")
set(FASTJUNGLE_COMPILED_SHADER_DIR
    "${FASTJUNGLE_RUNTIME_DIR}/shaders"
)

check_ipo_supported(
    RESULT FASTJUNGLE_IPO_SUPPORTED
    OUTPUT FASTJUNGLE_IPO_ERROR
)

function(fastjungle_apply_options target)
    target_compile_features(${target} PRIVATE cxx_std_20)

    target_compile_definitions(${target} PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        "$<$<BOOL:${FASTJUNGLE_LOG_ABORT_TO_STDERR}>:FASTJUNGLE_LOG_ABORT_TO_STDERR>"
        "$<$<CONFIG:Debug>:_DEBUG>"
        "$<$<CONFIG:Release>:NDEBUG>"
    )

    target_compile_options(${target} PRIVATE
        /W4
        /MP
        /EHsc
        /permissive-
        /Zc:__cplusplus
        /Zc:preprocessor
        /utf-8
        /diagnostics:caret
        "$<$<CONFIG:Release>:/O2>"
        "$<$<CONFIG:Release>:/Ob2>"
        "$<$<CONFIG:Release>:/Oi>"
        "$<$<CONFIG:Release>:/Ot>"
        "$<$<CONFIG:Release>:/Gy>"
        "$<$<CONFIG:Release>:/Gw>"
        "$<$<CONFIG:Release>:/arch:AVX2>"
    )

    target_link_options(${target} PRIVATE
        "$<$<CONFIG:Debug>:/DEBUG:FULL>"
        "$<$<CONFIG:Release>:/DEBUG:FULL>"
        "$<$<CONFIG:Release>:/INCREMENTAL:NO>"
        "$<$<CONFIG:Release>:/OPT:REF>"
        "$<$<CONFIG:Release>:/OPT:ICF>"
    )

    if(FASTJUNGLE_ENABLE_IPO AND FASTJUNGLE_IPO_SUPPORTED)
        set_property(TARGET ${target} PROPERTY
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        )
    elseif(FASTJUNGLE_ENABLE_IPO)
        message(WARNING "IPO/LTCG is unavailable: ${FASTJUNGLE_IPO_ERROR}")
    endif()
endfunction()

# src/cooker is cooker-only, src/renderer and src/dx12 are renderer-only.
# Every other source directory is shared automatically.
file(GLOB_RECURSE FASTJUNGLE_ALL_SOURCES CONFIGURE_DEPENDS
    "${FASTJUNGLE_SOURCE_DIR}/*.cpp"
    "${FASTJUNGLE_SOURCE_DIR}/*.cc"
    "${FASTJUNGLE_SOURCE_DIR}/*.cxx"
)
file(GLOB_RECURSE FASTJUNGLE_COOKER_SOURCES CONFIGURE_DEPENDS
    "${FASTJUNGLE_SOURCE_DIR}/cooker/*.cpp"
    "${FASTJUNGLE_SOURCE_DIR}/cooker/*.cc"
    "${FASTJUNGLE_SOURCE_DIR}/cooker/*.cxx"
)
file(GLOB_RECURSE FASTJUNGLE_RENDERER_SOURCES CONFIGURE_DEPENDS
    "${FASTJUNGLE_SOURCE_DIR}/renderer/*.cpp"
    "${FASTJUNGLE_SOURCE_DIR}/renderer/*.cc"
    "${FASTJUNGLE_SOURCE_DIR}/renderer/*.cxx"
)
file(GLOB_RECURSE FASTJUNGLE_DX12_SOURCES CONFIGURE_DEPENDS
    "${FASTJUNGLE_SOURCE_DIR}/dx12/*.cpp"
    "${FASTJUNGLE_SOURCE_DIR}/dx12/*.cc"
    "${FASTJUNGLE_SOURCE_DIR}/dx12/*.cxx"
)

set(FASTJUNGLE_COMMON_SOURCES ${FASTJUNGLE_ALL_SOURCES})
list(REMOVE_ITEM FASTJUNGLE_COMMON_SOURCES
    ${FASTJUNGLE_COOKER_SOURCES}
    ${FASTJUNGLE_RENDERER_SOURCES}
    ${FASTJUNGLE_DX12_SOURCES}
)

list(SORT FASTJUNGLE_COMMON_SOURCES)
list(SORT FASTJUNGLE_COOKER_SOURCES)
list(SORT FASTJUNGLE_RENDERER_SOURCES)
list(SORT FASTJUNGLE_DX12_SOURCES)

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

if(FASTJUNGLE_BUILD STREQUAL "renderer")
    set(FASTJUNGLE_DXC_VERSION "1.8.2505.32")
    set(FASTJUNGLE_DXC_EXECUTABLE "" CACHE FILEPATH
        "Optional dxc.exe override"
    )

    if(NOT FASTJUNGLE_DXC_EXECUTABLE)
        find_program(FASTJUNGLE_FOUND_DXC NAMES dxc.exe dxc)
        if(FASTJUNGLE_FOUND_DXC)
            set(FASTJUNGLE_DXC_EXECUTABLE
                "${FASTJUNGLE_FOUND_DXC}"
                CACHE FILEPATH "Optional dxc.exe override" FORCE
            )
        endif()
    endif()

    if(NOT FASTJUNGLE_DXC_EXECUTABLE)
        FetchContent_Declare(
            fastjungle_dxc
            URL
                "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.DXC/${FASTJUNGLE_DXC_VERSION}"
            URL_HASH
                SHA256=C6E82B70C14552F1DD58E4A79C93EEAB1567EEB0A9EE63A51564C410429BCE3E
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(fastjungle_dxc)
        set(FASTJUNGLE_DXC_EXECUTABLE
            "${fastjungle_dxc_SOURCE_DIR}/build/native/bin/x64/dxc.exe"
        )
    endif()

    if(NOT EXISTS "${FASTJUNGLE_DXC_EXECUTABLE}")
        message(FATAL_ERROR "dxc.exe was not found: ${FASTJUNGLE_DXC_EXECUTABLE}")
    endif()

    file(GLOB_RECURSE FASTJUNGLE_HLSL_SOURCES CONFIGURE_DEPENDS
        "${FASTJUNGLE_SHADER_DIR}/*.hlsl"
    )
    file(GLOB_RECURSE FASTJUNGLE_HLSL_INCLUDES CONFIGURE_DEPENDS
        "${FASTJUNGLE_SHADER_DIR}/*.hlsli"
    )
    list(SORT FASTJUNGLE_HLSL_SOURCES)
    list(SORT FASTJUNGLE_HLSL_INCLUDES)

    set(FASTJUNGLE_COMPILED_SHADERS "")
    foreach(shader IN LISTS FASTJUNGLE_HLSL_SOURCES)
        file(RELATIVE_PATH relative_shader
            "${FASTJUNGLE_SHADER_DIR}"
            "${shader}"
        )

        if(shader MATCHES "\\.vs\\.hlsl$")
            set(profile "vs_6_6")
        elseif(shader MATCHES "\\.ps\\.hlsl$")
            set(profile "ps_6_6")
        elseif(shader MATCHES "\\.cs\\.hlsl$")
            set(profile "cs_6_6")
        elseif(shader MATCHES "\\.gs\\.hlsl$")
            set(profile "gs_6_6")
        elseif(shader MATCHES "\\.hs\\.hlsl$")
            set(profile "hs_6_6")
        elseif(shader MATCHES "\\.ds\\.hlsl$")
            set(profile "ds_6_6")
        else()
            message(FATAL_ERROR
                "Unknown shader stage for '${relative_shader}'."
            )
        endif()

        string(REGEX REPLACE "\\.hlsl$" ".dxil"
            relative_output
            "${relative_shader}"
        )
        set(output
            "${FASTJUNGLE_COMPILED_SHADER_DIR}/${relative_output}"
        )
        get_filename_component(output_directory "${output}" DIRECTORY)

        add_custom_command(
            OUTPUT "${output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_directory}"
            COMMAND "${FASTJUNGLE_DXC_EXECUTABLE}"
                -nologo
                -E main
                -T "${profile}"
                -HV 2021
                -I "${FASTJUNGLE_SHADER_DIR}"
                "$<$<CONFIG:Debug>:-Zi>"
                "$<$<CONFIG:Debug>:-Qembed_debug>"
                "$<$<CONFIG:Debug>:-Od>"
                "$<$<CONFIG:Release>:-O3>"
                "$<$<CONFIG:Release>:-Qstrip_debug>"
                -Fo "${output}"
                "${shader}"
            DEPENDS
                "${shader}"
                ${FASTJUNGLE_HLSL_INCLUDES}
            COMMENT "DXC ${profile}: ${relative_shader}"
            VERBATIM
            COMMAND_EXPAND_LISTS
        )
        list(APPEND FASTJUNGLE_COMPILED_SHADERS "${output}")
    endforeach()

    add_custom_target(FastJungleShaders ALL
        DEPENDS ${FASTJUNGLE_COMPILED_SHADERS}
    )

    add_executable(FastJungleRenderer WIN32
        ${FASTJUNGLE_COMMON_SOURCES}
        ${FASTJUNGLE_RENDERER_SOURCES}
        ${FASTJUNGLE_DX12_SOURCES}
    )
    fastjungle_apply_options(FastJungleRenderer)

    target_compile_options(FastJungleRenderer PRIVATE
        "$<$<CONFIG:Release>:/fp:fast>"
    )
    target_compile_definitions(FastJungleRenderer PRIVATE
        "FASTJUNGLE_DEFAULT_COOKED_DIR=\"${FASTJUNGLE_COOKED_DIR}\""
        "FASTJUNGLE_SHADER_OUTPUT_DIR=\"${FASTJUNGLE_COMPILED_SHADER_DIR}\""
    )
    target_include_directories(FastJungleRenderer PRIVATE
        "${FASTJUNGLE_INCLUDE_DIR}"
        "${FASTJUNGLE_SHADER_DIR}"
    )
    target_link_libraries(FastJungleRenderer PRIVATE
        d3d12
        dxgi
        dxguid
        user32
        gdi32
        shell32
        ole32
        windowscodecs
    )
    add_dependencies(FastJungleRenderer FastJungleShaders)

    set_target_properties(FastJungleRenderer PROPERTIES
        OUTPUT_NAME "FastJungle"
        RUNTIME_OUTPUT_DIRECTORY "${FASTJUNGLE_RUNTIME_DIR}"
        PDB_OUTPUT_DIRECTORY "${FASTJUNGLE_RUNTIME_DIR}"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
else()
    include(ExternalProject)

    set(FASTJUNGLE_SCENE_DIR
        "${FASTJUNGLE_ASSET_DIR}/scene/JungleRuins_1_0_1b"
        CACHE PATH "Jungle Ruins source directory"
    )
    set(FASTJUNGLE_SCENE_USD
        "${FASTJUNGLE_SCENE_DIR}/USD/JungleRuins_Karma.usda"
        CACHE FILEPATH "Root USD layer"
    )
    set(FASTJUNGLE_TEXTURE_DIR
        "${FASTJUNGLE_SCENE_DIR}/textures"
        CACHE PATH "Source texture directory"
    )
    option(FASTJUNGLE_ENABLE_OPENEXR
        "Enable .exr source textures"
        ON
    )
    set(FASTJUNGLE_OPENEXR_TAG
        "v3.4.12"
        CACHE STRING "Pinned OpenEXR tag"
    )

    # DirectXTex is built in this build tree, so it automatically inherits
    # /MDd for Debug and /MD for Release from CMAKE_MSVC_RUNTIME_LIBRARY.
    # The Jungle source scene contains EXR textures. Build OpenEXR as a static
    # dependency first, then opt DirectXTex into compiling its auxiliary EXR
    # codec. This keeps the cooker preset self-contained instead of requiring
    # a separately installed OpenEXR package.
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
    set(BUILD_DX11 OFF CACHE BOOL "" FORCE)
    set(BUILD_DX12 OFF CACHE BOOL "" FORCE)
    set(BC_USE_OPENMP ON CACHE BOOL "" FORCE)

    if(FASTJUNGLE_ENABLE_OPENEXR)
        set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
        set(IMATH_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(IMATH_BUILD_PYTHON OFF CACHE BOOL "" FORCE)
        set(IMATH_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(IMATH_INSTALL OFF CACHE BOOL "" FORCE)
        set(IMATH_INSTALL_PKG_CONFIG OFF CACHE BOOL "" FORCE)
        set(OPENEXR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(OPENEXR_BUILD_PYTHON OFF CACHE BOOL "" FORCE)
        set(OPENEXR_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
        set(OPENEXR_FORCE_INTERNAL_DEFLATE ON CACHE BOOL "" FORCE)
        set(OPENEXR_FORCE_INTERNAL_IMATH ON CACHE BOOL "" FORCE)
        set(OPENEXR_INSTALL OFF CACHE BOOL "" FORCE)
        set(OPENEXR_INSTALL_DEVELOPER_TOOLS OFF CACHE BOOL "" FORCE)
        set(OPENEXR_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
        set(OPENEXR_INSTALL_PKG_CONFIG OFF CACHE BOOL "" FORCE)
        set(OPENEXR_INSTALL_TOOLS OFF CACHE BOOL "" FORCE)
        set(OPENEXR_TEST_LIBRARIES OFF CACHE BOOL "" FORCE)
        set(OPENEXR_TEST_PYTHON OFF CACHE BOOL "" FORCE)
        set(OPENEXR_TEST_TOOLS OFF CACHE BOOL "" FORCE)

        FetchContent_Declare(
            openexr
            GIT_REPOSITORY
                https://github.com/AcademySoftwareFoundation/openexr.git
            GIT_TAG "${FASTJUNGLE_OPENEXR_TAG}"
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(openexr)

        if(NOT TARGET OpenEXR::OpenEXR)
            message(FATAL_ERROR
                "OpenEXR did not create the expected OpenEXR::OpenEXR target."
            )
        endif()

        # DirectXTex calls find_package(OpenEXR) even when an in-tree target
        # already exists. Supply a build-tree find-module so that call reuses
        # this exact target without requiring OpenEXR to be installed first.
        set(FASTJUNGLE_FIND_MODULE_DIR
            "${CMAKE_BINARY_DIR}/cmake/find-modules"
        )
        file(MAKE_DIRECTORY "${FASTJUNGLE_FIND_MODULE_DIR}")
        file(WRITE
            "${FASTJUNGLE_FIND_MODULE_DIR}/FindOpenEXR.cmake"
            [=[
if(TARGET OpenEXR::OpenEXR)
    set(OpenEXR_FOUND TRUE)
else()
    set(OpenEXR_FOUND FALSE)
    set(OpenEXR_NOT_FOUND_MESSAGE
        "The in-tree OpenEXR::OpenEXR target is missing.")
endif()
]=]
        )
        list(PREPEND CMAKE_MODULE_PATH "${FASTJUNGLE_FIND_MODULE_DIR}")

        set(ENABLE_OPENEXR_SUPPORT ON CACHE BOOL "" FORCE)
    else()
        set(ENABLE_OPENEXR_SUPPORT OFF CACHE BOOL "" FORCE)
    endif()

    FetchContent_Declare(
        directxtex
        GIT_REPOSITORY https://github.com/microsoft/DirectXTex.git
        GIT_TAG may2026
        GIT_SHALLOW TRUE
    )

    # We consume DirectXTex only from the build tree. Its install export does
    # not support an in-tree OpenEXR dependency, so suppress only the
    # dependency's unused install rules while it is configured.
    set(FASTJUNGLE_SAVED_SKIP_INSTALL_RULES "${CMAKE_SKIP_INSTALL_RULES}")
    set(CMAKE_SKIP_INSTALL_RULES ON)
    FetchContent_MakeAvailable(directxtex)
    set(CMAKE_SKIP_INSTALL_RULES "${FASTJUNGLE_SAVED_SKIP_INSTALL_RULES}")
    unset(FASTJUNGLE_SAVED_SKIP_INSTALL_RULES)

    if(NOT TARGET DirectXTex)
        message(FATAL_ERROR "DirectXTex did not create the expected target.")
    endif()

    set(FASTJUNGLE_OPENUSD_TAG
        "v26.05"
        CACHE STRING "Pinned OpenUSD tag"
    )
    set(FASTJUNGLE_ONETBB_TAG
        "v2021.9.0"
        CACHE STRING "Pinned oneTBB tag"
    )
    set(FASTJUNGLE_DEPENDENCY_ROOT
        "${CMAKE_SOURCE_DIR}/out/deps"
        CACHE PATH "FastJungle dependency root directory"
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(FASTJUNGLE_EXTERNAL_RUNTIME "MultiThreadedDebugDLL")
        set(FASTJUNGLE_TBB_LIBRARY_NAME "tbb12_debug")
    else()
        set(FASTJUNGLE_EXTERNAL_RUNTIME "MultiThreadedDLL")
        set(FASTJUNGLE_TBB_LIBRARY_NAME "tbb12")
    endif()

    # Compiler, configuration and dependency versions are all part of the
    # directory key. Changing any of them creates a clean external ABI tree.
    string(MAKE_C_IDENTIFIER
        "msvc_${CMAKE_CXX_COMPILER_VERSION}_${CMAKE_BUILD_TYPE}_usd_${FASTJUNGLE_OPENUSD_TAG}_tbb_${FASTJUNGLE_ONETBB_TAG}"
        FASTJUNGLE_DEPENDENCY_ABI
    )
    set(FASTJUNGLE_DEPENDENCY_DIR
        "${FASTJUNGLE_DEPENDENCY_ROOT}/${FASTJUNGLE_DEPENDENCY_ABI}"
    )

    set(FASTJUNGLE_TBB_PREFIX
        "${FASTJUNGLE_DEPENDENCY_DIR}/onetbb-prefix"
    )
    set(FASTJUNGLE_TBB_BUILD
        "${FASTJUNGLE_DEPENDENCY_DIR}/onetbb-build"
    )
    set(FASTJUNGLE_TBB_INSTALL
        "${FASTJUNGLE_DEPENDENCY_DIR}/onetbb-install"
    )

    set(FASTJUNGLE_OPENUSD_PREFIX
        "${FASTJUNGLE_DEPENDENCY_DIR}/openusd-prefix"
    )
    set(FASTJUNGLE_OPENUSD_BUILD
        "${FASTJUNGLE_DEPENDENCY_DIR}/openusd-build"
    )
    set(FASTJUNGLE_OPENUSD_INSTALL
        "${FASTJUNGLE_DEPENDENCY_DIR}/openusd-install"
    )

    set(FASTJUNGLE_TBB_IMPLIB
        "${FASTJUNGLE_TBB_INSTALL}/lib/${FASTJUNGLE_TBB_LIBRARY_NAME}.lib"
    )
    set(FASTJUNGLE_TBB_DLL
        "${FASTJUNGLE_TBB_INSTALL}/bin/${FASTJUNGLE_TBB_LIBRARY_NAME}.dll"
    )
    set(FASTJUNGLE_OPENUSD_IMPLIB
        "${FASTJUNGLE_OPENUSD_INSTALL}/lib/usd_ms.lib"
    )
    set(FASTJUNGLE_OPENUSD_DLL
        "${FASTJUNGLE_OPENUSD_INSTALL}/lib/usd_ms.dll"
    )

    if(NOT EXISTS "${FASTJUNGLE_TBB_IMPLIB}" OR
       NOT EXISTS "${FASTJUNGLE_TBB_DLL}")
        ExternalProject_Add(FastJungleOneTBB
            GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
            GIT_TAG "${FASTJUNGLE_ONETBB_TAG}"
            GIT_SHALLOW TRUE
            UPDATE_DISCONNECTED TRUE

            PREFIX "${FASTJUNGLE_TBB_PREFIX}"
            BINARY_DIR "${FASTJUNGLE_TBB_BUILD}"
            INSTALL_DIR "${FASTJUNGLE_TBB_INSTALL}"
            INSTALL_BYPRODUCTS
                "${FASTJUNGLE_TBB_IMPLIB}"
                "${FASTJUNGLE_TBB_DLL}"

            CMAKE_ARGS
                "-DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>"
                "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
                "-DCMAKE_MSVC_RUNTIME_LIBRARY:STRING=${FASTJUNGLE_EXTERNAL_RUNTIME}"
                "-DCMAKE_POLICY_DEFAULT_CMP0091:STRING=NEW"
                "-DCMAKE_POLICY_VERSION_MINIMUM:STRING=3.5"
                "-DBUILD_SHARED_LIBS:BOOL=ON"
                "-DTBB_TEST:BOOL=OFF"
                "-DTBB_EXAMPLES:BOOL=OFF"
                "-DTBB_STRICT:BOOL=OFF"

            BUILD_COMMAND
                "${CMAKE_COMMAND}" --build <BINARY_DIR>
                    --config "${CMAKE_BUILD_TYPE}" --parallel
            INSTALL_COMMAND
                "${CMAKE_COMMAND}" --install <BINARY_DIR>
                    --config "${CMAKE_BUILD_TYPE}"
        )
    endif()

    set(FASTJUNGLE_OPENUSD_DEPENDS_ARGS "")
    if(TARGET FastJungleOneTBB)
        list(APPEND FASTJUNGLE_OPENUSD_DEPENDS_ARGS
            DEPENDS FastJungleOneTBB
        )
    endif()

    if(NOT EXISTS "${FASTJUNGLE_OPENUSD_IMPLIB}" OR
       NOT EXISTS "${FASTJUNGLE_OPENUSD_DLL}")
        ExternalProject_Add(FastJungleOpenUSD
            GIT_REPOSITORY https://github.com/PixarAnimationStudios/OpenUSD.git
            GIT_TAG "${FASTJUNGLE_OPENUSD_TAG}"
            GIT_SHALLOW TRUE
            UPDATE_DISCONNECTED TRUE
            ${FASTJUNGLE_OPENUSD_DEPENDS_ARGS}

            PREFIX "${FASTJUNGLE_OPENUSD_PREFIX}"
            BINARY_DIR "${FASTJUNGLE_OPENUSD_BUILD}"
            INSTALL_DIR "${FASTJUNGLE_OPENUSD_INSTALL}"
            INSTALL_BYPRODUCTS
                "${FASTJUNGLE_OPENUSD_IMPLIB}"
                "${FASTJUNGLE_OPENUSD_DLL}"

            CMAKE_ARGS
                "-DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>"
                "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
                "-DCMAKE_MSVC_RUNTIME_LIBRARY:STRING=${FASTJUNGLE_EXTERNAL_RUNTIME}"
                "-DCMAKE_POLICY_DEFAULT_CMP0091:STRING=NEW"
                "-DCMAKE_POLICY_VERSION_MINIMUM:STRING=3.5"
                "-DTBB_ROOT_DIR:PATH=${FASTJUNGLE_TBB_INSTALL}"
                "-DTBB_DIR:PATH=${FASTJUNGLE_TBB_INSTALL}/lib/cmake/TBB"
                "-DCMAKE_PREFIX_PATH:PATH=${FASTJUNGLE_TBB_INSTALL}"
                "-DBUILD_SHARED_LIBS:BOOL=ON"
                "-DPXR_BUILD_MONOLITHIC:BOOL=ON"
                "-DPXR_BUILD_TESTS:BOOL=OFF"
                "-DPXR_BUILD_EXAMPLES:BOOL=OFF"
                "-DPXR_BUILD_TUTORIALS:BOOL=OFF"
                "-DPXR_BUILD_USD_TOOLS:BOOL=OFF"
                "-DPXR_BUILD_IMAGING:BOOL=OFF"
                "-DPXR_BUILD_USD_IMAGING:BOOL=OFF"
                "-DPXR_BUILD_USD_VALIDATION:BOOL=OFF"
                "-DPXR_BUILD_EXEC:BOOL=OFF"
                "-DPXR_BUILD_USDVIEW:BOOL=OFF"
                "-DPXR_BUILD_DOCUMENTATION:BOOL=OFF"
                "-DPXR_ENABLE_PYTHON_SUPPORT:BOOL=OFF"
                "-DPXR_ENABLE_GL_SUPPORT:BOOL=OFF"
                "-DPXR_ENABLE_VULKAN_SUPPORT:BOOL=OFF"
                "-DPXR_ENABLE_MATERIALX_SUPPORT:BOOL=OFF"
                "-DPXR_BUILD_OPENIMAGEIO_PLUGIN:BOOL=OFF"
                "-DPXR_BUILD_OPENCOLORIO_PLUGIN:BOOL=OFF"
                "-DPXR_BUILD_ALEMBIC_PLUGIN:BOOL=OFF"
                "-DPXR_BUILD_DRACO_PLUGIN:BOOL=OFF"
                "-DPXR_ENABLE_PRECOMPILED_HEADERS:BOOL=ON"
                "-DPXR_PREFER_SAFETY_OVER_SPEED:BOOL=ON"

            BUILD_COMMAND
                "${CMAKE_COMMAND}" --build <BINARY_DIR>
                    --config "${CMAKE_BUILD_TYPE}" --parallel
            INSTALL_COMMAND
                "${CMAKE_COMMAND}" --install <BINARY_DIR>
                    --config "${CMAKE_BUILD_TYPE}"
        )
    endif()

    add_library(FastJungleOneTBBLibrary SHARED IMPORTED GLOBAL)
    set_target_properties(FastJungleOneTBBLibrary PROPERTIES
        IMPORTED_IMPLIB "${FASTJUNGLE_TBB_IMPLIB}"
        IMPORTED_LOCATION "${FASTJUNGLE_TBB_DLL}"
    )
    add_library(FastJungle::OneTBB ALIAS FastJungleOneTBBLibrary)

    add_library(FastJungleOpenUSDLibrary SHARED IMPORTED GLOBAL)
    set_target_properties(FastJungleOpenUSDLibrary PROPERTIES
        IMPORTED_IMPLIB "${FASTJUNGLE_OPENUSD_IMPLIB}"
        IMPORTED_LOCATION "${FASTJUNGLE_OPENUSD_DLL}"
    )
    add_library(FastJungle::OpenUSD ALIAS FastJungleOpenUSDLibrary)

    add_executable(FastJungleCooker
        ${FASTJUNGLE_COMMON_SOURCES}
        ${FASTJUNGLE_COOKER_SOURCES}
    )
    fastjungle_apply_options(FastJungleCooker)

    target_compile_definitions(FastJungleCooker PRIVATE
        "FASTJUNGLE_DEFAULT_SCENE_USD=\"${FASTJUNGLE_SCENE_USD}\""
        "FASTJUNGLE_DEFAULT_TEXTURE_DIR=\"${FASTJUNGLE_TEXTURE_DIR}\""
        "FASTJUNGLE_DEFAULT_COOKED_DIR=\"${FASTJUNGLE_COOKED_DIR}\""
    )
    target_include_directories(FastJungleCooker PRIVATE
        "${FASTJUNGLE_INCLUDE_DIR}"
        "${FASTJUNGLE_OPENUSD_INSTALL}/include"
        "${FASTJUNGLE_TBB_INSTALL}/include"
    )
    target_link_libraries(FastJungleCooker PRIVATE
        DirectXTex
        FastJungle::OpenUSD
        FastJungle::OneTBB
        windowscodecs
        ole32
        shell32
        Shlwapi
        Dbghelp
        Ws2_32
    )

    if(FASTJUNGLE_ENABLE_OPENEXR)
        # DirectXTex intentionally does not publish its Auxiliary include
        # directory. The EXR API lives there, so expose it only to the cooker.
        target_include_directories(FastJungleCooker PRIVATE
            "${directxtex_SOURCE_DIR}/Auxiliary"
        )
        target_compile_definitions(FastJungleCooker PRIVATE
            FASTJUNGLE_HAS_OPENEXR=1
        )
    endif()

    if(TARGET FastJungleOneTBB)
        add_dependencies(FastJungleCooker FastJungleOneTBB)
    endif()
    if(TARGET FastJungleOpenUSD)
        add_dependencies(FastJungleCooker FastJungleOpenUSD)
    endif()

    # Generated from this root CMakeLists.txt so no source-side deployment
    # script is required.
    set(FASTJUNGLE_OPENUSD_DEPLOY_SCRIPT
        "${CMAKE_BINARY_DIR}/FastJungleDeployOpenUSD.cmake"
    )
    file(WRITE "${FASTJUNGLE_OPENUSD_DEPLOY_SCRIPT}" [=[
if(EXISTS "${SOURCE_ROOT}/bin")
    file(COPY "${SOURCE_ROOT}/bin/" DESTINATION "${DESTINATION}")
endif()

if(EXISTS "${SOURCE_ROOT}/lib/usd")
    file(COPY "${SOURCE_ROOT}/lib/usd/"
        DESTINATION "${DESTINATION}/openusd/lib/usd")
endif()

if(EXISTS "${SOURCE_ROOT}/plugin/usd")
    file(COPY "${SOURCE_ROOT}/plugin/usd/"
        DESTINATION "${DESTINATION}/openusd/plugin/usd")
endif()
]=])

    add_custom_command(TARGET FastJungleCooker POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${FASTJUNGLE_OPENUSD_DLL}"
            "$<TARGET_FILE_DIR:FastJungleCooker>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${FASTJUNGLE_TBB_DLL}"
            "$<TARGET_FILE_DIR:FastJungleCooker>"
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_ROOT=${FASTJUNGLE_OPENUSD_INSTALL}"
            "-DDESTINATION=$<TARGET_FILE_DIR:FastJungleCooker>"
            -P "${FASTJUNGLE_OPENUSD_DEPLOY_SCRIPT}"
        COMMENT "Deploying OpenUSD runtime files"
        VERBATIM
    )

    set_target_properties(FastJungleCooker PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${FASTJUNGLE_RUNTIME_DIR}"
        PDB_OUTPUT_DIRECTORY "${FASTJUNGLE_RUNTIME_DIR}"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()

if(FASTJUNGLE_BUILD_TESTS)
    enable_testing()

    add_executable(FastJungleSceneStreamingTest
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/scene/StaticSceneStreamingTest.cpp"
        "${FASTJUNGLE_SOURCE_DIR}/core/util/BinaryStream.cpp"
        "${FASTJUNGLE_SOURCE_DIR}/core/util/DateTime.cpp"
        "${FASTJUNGLE_SOURCE_DIR}/core/util/File.cpp"
        "${FASTJUNGLE_SOURCE_DIR}/core/util/Logger.cpp"
        "${FASTJUNGLE_SOURCE_DIR}/core/util/TemporaryFile.cpp"
        "${FASTJUNGLE_SOURCE_DIR}/scene/StaticSceneFileFormat.cpp"
        "${FASTJUNGLE_SOURCE_DIR}/scene/StaticSceneStreamReader.cpp"
        "${FASTJUNGLE_SOURCE_DIR}/scene/StaticSceneStreamWriter.cpp"
        "${FASTJUNGLE_SOURCE_DIR}/scene/StaticSceneValidation.cpp"
        "${FASTJUNGLE_SOURCE_DIR}/scene/StaticTextureFileFormat.cpp"
    )
    fastjungle_apply_options(FastJungleSceneStreamingTest)
    target_include_directories(FastJungleSceneStreamingTest PRIVATE
        "${FASTJUNGLE_INCLUDE_DIR}"
    )

    add_test(
        NAME FastJungle.SceneStreaming
        COMMAND FastJungleSceneStreamingTest
    )

    if(FASTJUNGLE_BUILD STREQUAL "cooker")
        add_executable(FastJungleTextureCompressionPolicyTest
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/cooker/TextureCompressionPolicyTest.cpp"
            "${FASTJUNGLE_SOURCE_DIR}/cooker/TextureCompressionPolicy.cpp"
        )
        fastjungle_apply_options(FastJungleTextureCompressionPolicyTest)
        target_include_directories(
            FastJungleTextureCompressionPolicyTest PRIVATE
            "${FASTJUNGLE_INCLUDE_DIR}"
        )

        add_test(
            NAME FastJungle.TextureCompressionPolicy
            COMMAND FastJungleTextureCompressionPolicyTest
        )

        add_executable(FastJungleTextureCookerTest
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/cooker/TextureCookerTest.cpp"
            "${FASTJUNGLE_SOURCE_DIR}/cooker/TextureCompressionPolicy.cpp"
            "${FASTJUNGLE_SOURCE_DIR}/cooker/TextureCooker.cpp"
            "${FASTJUNGLE_SOURCE_DIR}/cooker/TextureImageProcessor.cpp"
            "${FASTJUNGLE_SOURCE_DIR}/core/util/DateTime.cpp"
            "${FASTJUNGLE_SOURCE_DIR}/core/util/Logger.cpp"
            "${FASTJUNGLE_SOURCE_DIR}/scene/StaticSceneValidation.cpp"
        )
        fastjungle_apply_options(FastJungleTextureCookerTest)
        target_include_directories(FastJungleTextureCookerTest PRIVATE
            "${FASTJUNGLE_INCLUDE_DIR}"
        )
        target_link_libraries(FastJungleTextureCookerTest PRIVATE
            DirectXTex
            windowscodecs
            ole32
        )

        add_test(
            NAME FastJungle.TextureCooker
            COMMAND FastJungleTextureCookerTest
        )
    endif()
endif()

message(STATUS "FastJungle target        : ${FASTJUNGLE_BUILD}")
message(STATUS "Build configuration      : ${CMAKE_BUILD_TYPE}")
message(STATUS "MSVC runtime             : ${CMAKE_MSVC_RUNTIME_LIBRARY}")
message(STATUS "Runtime output           : ${FASTJUNGLE_RUNTIME_DIR}")
message(STATUS "Cooked output            : ${FASTJUNGLE_COOKED_DIR}")
message(STATUS "Logger abort to stderr   : ${FASTJUNGLE_LOG_ABORT_TO_STDERR}")

if(FASTJUNGLE_BUILD STREQUAL "cooker")
    message(STATUS "External CRT             : ${FASTJUNGLE_EXTERNAL_RUNTIME}")
    message(STATUS "Dependency ABI directory : ${FASTJUNGLE_DEPENDENCY_DIR}")
    message(STATUS "OpenUSD tag              : ${FASTJUNGLE_OPENUSD_TAG}")
    message(STATUS "oneTBB tag               : ${FASTJUNGLE_ONETBB_TAG}")
    message(STATUS "OpenEXR support          : ${FASTJUNGLE_ENABLE_OPENEXR}")
    if(FASTJUNGLE_ENABLE_OPENEXR)
        message(STATUS "OpenEXR tag              : ${FASTJUNGLE_OPENEXR_TAG}")
    endif()
endif()
