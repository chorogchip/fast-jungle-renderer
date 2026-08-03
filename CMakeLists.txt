cmake_minimum_required(VERSION 3.26)

project(FastJungle VERSION 0.1.0 LANGUAGES CXX)

include(CheckIPOSupported)

# =============================================================================
# Supported environment
# =============================================================================

if(NOT WIN32)
    message(FATAL_ERROR "FastJungle supports Windows only.")
endif()

if(NOT MSVC)
    message(FATAL_ERROR "FastJungle requires the MSVC toolchain.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "FastJungle supports x64 builds only.")
endif()

# Use Visual Studio's CMake integration with Ninja. Debug and Release use
# separate build trees so their OpenUSD/oneTBB installations never mix.
if(CMAKE_CONFIGURATION_TYPES)
    message(FATAL_ERROR
        "FastJungle requires a single-config generator. "
        "Use Ninja with CMAKE_BUILD_TYPE=Debug or Release."
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
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# =============================================================================
# Paths and options
# =============================================================================

option(FASTJUNGLE_BUILD_RENDERER
    "Build the FastJungle runtime renderer"
    ON
)
option(FASTJUNGLE_BUILD_COOKER
    "Build the FastJungle offline cooker"
    OFF
)
option(FASTJUNGLE_ENABLE_IPO
    "Enable Release IPO/LTCG for FastJungle targets"
    ON
)

if(FASTJUNGLE_BUILD_RENDERER OR FASTJUNGLE_BUILD_COOKER)
    include(FetchContent)
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)
endif()

if(FASTJUNGLE_BUILD_COOKER)
    include(ExternalProject)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
endif()

set(FASTJUNGLE_SOURCE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(FASTJUNGLE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(FASTJUNGLE_SHADER_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(FASTJUNGLE_ASSET_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/assets")

set(FASTJUNGLE_COOKED_DIR
    "${FASTJUNGLE_ASSET_DIR}/cooked"
    CACHE PATH "Cooked scene output directory"
)

if(FASTJUNGLE_BUILD_COOKER)
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
    set(FASTJUNGLE_OPENUSD_TAG "v26.05" CACHE STRING "Pinned OpenUSD tag")
    set(FASTJUNGLE_ONETBB_TAG  "v2021.9.0" CACHE STRING "Pinned oneTBB tag")
    option(FASTJUNGLE_ENABLE_OPENEXR
        "Link an installed OpenEXR package for .exr source textures"
        OFF
    )
endif()

if(FASTJUNGLE_BUILD_RENDERER)
    set(FASTJUNGLE_DXC_VERSION "1.8.2505.32")
    set(FASTJUNGLE_DXC_EXECUTABLE "" CACHE FILEPATH
        "Optional dxc.exe override; empty downloads the pinned NuGet package"
    )
endif()

set(FASTJUNGLE_RUNTIME_DIR "${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(FASTJUNGLE_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(FASTJUNGLE_COMPILED_SHADER_DIR
    "${FASTJUNGLE_RUNTIME_DIR}/shaders"
)

# =============================================================================
# Project compiler settings
# =============================================================================

add_library(FastJungleProjectOptions INTERFACE)
add_library(FastJungle::ProjectOptions ALIAS FastJungleProjectOptions)
set_target_properties(FastJungleProjectOptions PROPERTIES FOLDER "CMake")

target_compile_features(FastJungleProjectOptions INTERFACE cxx_std_20)
target_compile_definitions(FastJungleProjectOptions INTERFACE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    "$<$<CONFIG:Debug>:_DEBUG>"
    "$<$<CONFIG:Release>:NDEBUG>"
)
target_compile_options(FastJungleProjectOptions INTERFACE
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
target_link_options(FastJungleProjectOptions INTERFACE
    "$<$<CONFIG:Debug>:/DEBUG:FULL>"
    "$<$<CONFIG:Release>:/DEBUG:FULL>"
    "$<$<CONFIG:Release>:/INCREMENTAL:NO>"
    "$<$<CONFIG:Release>:/OPT:REF>"
    "$<$<CONFIG:Release>:/OPT:ICF>"
)

check_ipo_supported(
    RESULT FASTJUNGLE_IPO_SUPPORTED
    OUTPUT FASTJUNGLE_IPO_ERROR
)

function(fastjungle_enable_ipo target_name)
    if(FASTJUNGLE_ENABLE_IPO AND FASTJUNGLE_IPO_SUPPORTED)
        set_property(TARGET "${target_name}" PROPERTY
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        )
    elseif(FASTJUNGLE_ENABLE_IPO)
        message(WARNING "IPO/LTCG is unavailable: ${FASTJUNGLE_IPO_ERROR}")
    endif()
endfunction()

# =============================================================================
# DirectXTex: offline DDS conversion
# =============================================================================

if(FASTJUNGLE_BUILD_RENDERER OR FASTJUNGLE_BUILD_COOKER)
    set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
    set(BUILD_DX11 OFF CACHE BOOL "" FORCE)
    set(BUILD_DX12 OFF CACHE BOOL "" FORCE)
    set(BC_USE_OPENMP OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        directxtex
        GIT_REPOSITORY https://github.com/microsoft/DirectXTex.git
        GIT_TAG may2026
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(directxtex)

    if(NOT TARGET DirectXTex)
        message(FATAL_ERROR "DirectXTex did not create the expected target.")
    endif()

    set_target_properties(DirectXTex PROPERTIES
        FOLDER "External/DirectXTex"
    )
endif()

# =============================================================================
# DXC: build-time HLSL compilation
# =============================================================================

if(FASTJUNGLE_BUILD_RENDERER)
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
endif()

# =============================================================================
# OpenUSD + oneTBB: shared monolithic build used only by the cooker
# =============================================================================

if(FASTJUNGLE_BUILD_COOKER)
set(FASTJUNGLE_EXTERNAL_ROOT "${CMAKE_BINARY_DIR}/external")

set(FASTJUNGLE_TBB_PREFIX  "${FASTJUNGLE_EXTERNAL_ROOT}/oneTBB-prefix")
set(FASTJUNGLE_TBB_BUILD   "${FASTJUNGLE_EXTERNAL_ROOT}/oneTBB-build")
set(FASTJUNGLE_TBB_INSTALL "${FASTJUNGLE_EXTERNAL_ROOT}/oneTBB-install")

set(FASTJUNGLE_USD_PREFIX  "${FASTJUNGLE_EXTERNAL_ROOT}/OpenUSD-prefix")
set(FASTJUNGLE_USD_BUILD   "${FASTJUNGLE_EXTERNAL_ROOT}/OpenUSD-build")
set(FASTJUNGLE_USD_INSTALL "${FASTJUNGLE_EXTERNAL_ROOT}/OpenUSD-install")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(FASTJUNGLE_EXTERNAL_CRT "MultiThreadedDebugDLL")
    set(FASTJUNGLE_TBB_IMPLIB
        "${FASTJUNGLE_TBB_INSTALL}/lib/tbb12_debug.lib"
    )
else()
    set(FASTJUNGLE_EXTERNAL_CRT "MultiThreadedDLL")
    set(FASTJUNGLE_TBB_IMPLIB
        "${FASTJUNGLE_TBB_INSTALL}/lib/tbb12.lib"
    )
endif()

set(FASTJUNGLE_USD_IMPLIB "${FASTJUNGLE_USD_INSTALL}/lib/usd_ms.lib")
set(FASTJUNGLE_USD_DLL    "${FASTJUNGLE_USD_INSTALL}/lib/usd_ms.dll")

set(FASTJUNGLE_EXTERNAL_CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>"
    "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
    "-DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
    "-DCMAKE_MSVC_RUNTIME_LIBRARY:STRING=${FASTJUNGLE_EXTERNAL_CRT}"
    "-DCMAKE_POLICY_DEFAULT_CMP0091:STRING=NEW"
)
if(CMAKE_MAKE_PROGRAM)
    list(APPEND FASTJUNGLE_EXTERNAL_CMAKE_ARGS
        "-DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
    )
endif()
if(CMAKE_RC_COMPILER)
    list(APPEND FASTJUNGLE_EXTERNAL_CMAKE_ARGS
        "-DCMAKE_RC_COMPILER:FILEPATH=${CMAKE_RC_COMPILER}"
    )
endif()

ExternalProject_Add(FastJungleOneTBBProject
    GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
    GIT_TAG "${FASTJUNGLE_ONETBB_TAG}"
    GIT_SHALLOW TRUE
    UPDATE_DISCONNECTED TRUE

    PREFIX "${FASTJUNGLE_TBB_PREFIX}"
    BINARY_DIR "${FASTJUNGLE_TBB_BUILD}"
    INSTALL_DIR "${FASTJUNGLE_TBB_INSTALL}"
    INSTALL_BYPRODUCTS "${FASTJUNGLE_TBB_IMPLIB}"

    CMAKE_ARGS
        ${FASTJUNGLE_EXTERNAL_CMAKE_ARGS}
        "-DCMAKE_POLICY_VERSION_MINIMUM:STRING=3.5"
        "-DBUILD_SHARED_LIBS:BOOL=ON"
        "-DTBB_TEST:BOOL=OFF"
        "-DTBB_EXAMPLES:BOOL=OFF"
        "-DTBB_STRICT:BOOL=OFF"
)

ExternalProject_Add(FastJungleOpenUSDProject
    GIT_REPOSITORY https://github.com/PixarAnimationStudios/OpenUSD.git
    GIT_TAG "${FASTJUNGLE_OPENUSD_TAG}"
    GIT_SHALLOW TRUE
    UPDATE_DISCONNECTED TRUE
    DEPENDS FastJungleOneTBBProject

    PREFIX "${FASTJUNGLE_USD_PREFIX}"
    BINARY_DIR "${FASTJUNGLE_USD_BUILD}"
    INSTALL_DIR "${FASTJUNGLE_USD_INSTALL}"
    INSTALL_BYPRODUCTS
        "${FASTJUNGLE_USD_IMPLIB}"
        "${FASTJUNGLE_USD_DLL}"

    CMAKE_ARGS
        ${FASTJUNGLE_EXTERNAL_CMAKE_ARGS}
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
)

set_target_properties(
    FastJungleOneTBBProject
    FastJungleOpenUSDProject
    PROPERTIES FOLDER "External/OpenUSD"
)

# Imported-target include paths must exist when CMake generates the project.
file(MAKE_DIRECTORY
    "${FASTJUNGLE_TBB_INSTALL}/include"
    "${FASTJUNGLE_USD_INSTALL}/include"
)

add_library(FastJungleOpenUSD SHARED IMPORTED GLOBAL)
add_library(FastJungle::OpenUSD ALIAS FastJungleOpenUSD)
set_target_properties(FastJungleOpenUSD PROPERTIES
    IMPORTED_IMPLIB "${FASTJUNGLE_USD_IMPLIB}"
    IMPORTED_LOCATION "${FASTJUNGLE_USD_DLL}"
    INTERFACE_INCLUDE_DIRECTORIES
        "${FASTJUNGLE_USD_INSTALL}/include;${FASTJUNGLE_TBB_INSTALL}/include"
    INTERFACE_LINK_LIBRARIES
        "${FASTJUNGLE_TBB_IMPLIB};Shlwapi;Dbghelp;Ws2_32"
)
endif()

# =============================================================================
# Automatic source discovery
# =============================================================================

function(fastjungle_glob_cpp output_variable directory)
    file(GLOB_RECURSE _files CONFIGURE_DEPENDS
        "${directory}/*.cpp"
        "${directory}/*.cc"
        "${directory}/*.cxx"
    )
    list(SORT _files)
    set(${output_variable} "${_files}" PARENT_SCOPE)
endfunction()

fastjungle_glob_cpp(FASTJUNGLE_CORE_SOURCES
    "${FASTJUNGLE_SOURCE_DIR}/core"
)
fastjungle_glob_cpp(FASTJUNGLE_SCENE_SOURCES
    "${FASTJUNGLE_SOURCE_DIR}/scene"
)
if(FASTJUNGLE_BUILD_COOKER)
    fastjungle_glob_cpp(FASTJUNGLE_COOKER_SOURCES
        "${FASTJUNGLE_SOURCE_DIR}/cooker"
    )
endif()
if(FASTJUNGLE_BUILD_RENDERER)
    fastjungle_glob_cpp(FASTJUNGLE_RENDERER_SOURCES
        "${FASTJUNGLE_SOURCE_DIR}/renderer"
    )
    file(GLOB_RECURSE FASTJUNGLE_RENDERER_INTERNAL_HEADERS CONFIGURE_DEPENDS
        "${FASTJUNGLE_SOURCE_DIR}/renderer/*.hpp"
    )
    list(APPEND FASTJUNGLE_RENDERER_SOURCES
        ${FASTJUNGLE_RENDERER_INTERNAL_HEADERS}
    )
    list(SORT FASTJUNGLE_RENDERER_SOURCES)
    fastjungle_glob_cpp(FASTJUNGLE_DX12_SOURCES
        "${FASTJUNGLE_SOURCE_DIR}/dx12"
    )
endif()

file(GLOB_RECURSE FASTJUNGLE_HEADERS CONFIGURE_DEPENDS
    "${FASTJUNGLE_INCLUDE_DIR}/*.h"
    "${FASTJUNGLE_INCLUDE_DIR}/*.hpp"
    "${FASTJUNGLE_INCLUDE_DIR}/*.inl"
)
list(SORT FASTJUNGLE_HEADERS)

if(FASTJUNGLE_BUILD_COOKER)
    set(FASTJUNGLE_COOKER_MAIN
        "${FASTJUNGLE_SOURCE_DIR}/cooker/main.cpp"
    )
    if(NOT EXISTS "${FASTJUNGLE_COOKER_MAIN}")
        message(FATAL_ERROR
            "Missing ${FASTJUNGLE_COOKER_MAIN}; the cooker entry point must be wmain."
        )
    endif()
    list(REMOVE_ITEM FASTJUNGLE_COOKER_SOURCES
        "${FASTJUNGLE_COOKER_MAIN}"
    )
endif()
if(FASTJUNGLE_BUILD_RENDERER)
    set(FASTJUNGLE_RENDERER_MAIN
        "${FASTJUNGLE_SOURCE_DIR}/renderer/main.cpp"
    )
    if(NOT EXISTS "${FASTJUNGLE_RENDERER_MAIN}")
        message(FATAL_ERROR
            "Missing ${FASTJUNGLE_RENDERER_MAIN}; the renderer entry point must be wWinMain."
        )
    endif()
endif()
if(NOT FASTJUNGLE_CORE_SOURCES)
    message(FATAL_ERROR "No C++ sources found under src/core.")
endif()
if(NOT FASTJUNGLE_SCENE_SOURCES)
    message(FATAL_ERROR "No C++ sources found under src/scene.")
endif()

# =============================================================================
# Shared project libraries
# =============================================================================

add_library(FastJungleCore STATIC
    ${FASTJUNGLE_CORE_SOURCES}
    ${FASTJUNGLE_HEADERS}
)
add_library(FastJungle::Core ALIAS FastJungleCore)
target_link_libraries(FastJungleCore PUBLIC FastJungle::ProjectOptions)
target_include_directories(FastJungleCore PUBLIC "${FASTJUNGLE_INCLUDE_DIR}")
set_target_properties(FastJungleCore PROPERTIES
    FOLDER "FastJungle/Libraries"
)
fastjungle_enable_ipo(FastJungleCore)

# This library contains only cooked-scene/runtime-neutral structures.
# OpenUSD-specific conversion code belongs under src/cooker.
add_library(FastJungleScene STATIC
    ${FASTJUNGLE_SCENE_SOURCES}
    ${FASTJUNGLE_HEADERS}
)
add_library(FastJungle::Scene ALIAS FastJungleScene)
target_link_libraries(FastJungleScene PUBLIC
    FastJungle::ProjectOptions
    FastJungle::Core
)
target_include_directories(FastJungleScene PUBLIC "${FASTJUNGLE_INCLUDE_DIR}")
set_target_properties(FastJungleScene PROPERTIES
    FOLDER "FastJungle/Libraries"
)
fastjungle_enable_ipo(FastJungleScene)

# =============================================================================
# Cooker: console executable with wmain
# =============================================================================

if(FASTJUNGLE_BUILD_COOKER)
add_library(FastJungleUsdImporter STATIC
    ${FASTJUNGLE_COOKER_SOURCES}
    ${FASTJUNGLE_HEADERS}
)
target_link_libraries(FastJungleUsdImporter PUBLIC
    FastJungle::ProjectOptions
    FastJungle::Core
    FastJungle::Scene
    FastJungle::OpenUSD
    DirectXTex
)
target_include_directories(FastJungleUsdImporter PUBLIC
    "${FASTJUNGLE_INCLUDE_DIR}"
)
add_dependencies(FastJungleUsdImporter FastJungleOpenUSDProject)
set_target_properties(FastJungleUsdImporter PROPERTIES
    FOLDER "FastJungle/Libraries"
)
fastjungle_enable_ipo(FastJungleUsdImporter)

add_executable(FastJungleCooker
    ${FASTJUNGLE_COOKER_MAIN}
    ${FASTJUNGLE_HEADERS}
)
target_link_libraries(FastJungleCooker PRIVATE
    FastJungle::ProjectOptions
    FastJungle::Core
    FastJungle::Scene
    FastJungleUsdImporter
    DirectXTex
    windowscodecs
    ole32
    shell32
)
if(FASTJUNGLE_ENABLE_OPENEXR)
    target_link_libraries(FastJungleCooker PRIVATE OpenEXR::OpenEXR)
    target_compile_definitions(FastJungleCooker PRIVATE
        FASTJUNGLE_HAS_OPENEXR=1
    )
endif()

target_include_directories(FastJungleCooker PRIVATE
    "${FASTJUNGLE_INCLUDE_DIR}"
)
target_compile_definitions(FastJungleCooker PRIVATE
    "FASTJUNGLE_DEFAULT_SCENE_USD=\"${FASTJUNGLE_SCENE_USD}\""
    "FASTJUNGLE_DEFAULT_TEXTURE_DIR=\"${FASTJUNGLE_TEXTURE_DIR}\""
    "FASTJUNGLE_DEFAULT_COOKED_DIR=\"${FASTJUNGLE_COOKED_DIR}\""
)
set_target_properties(FastJungleCooker PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${FASTJUNGLE_RUNTIME_DIR}"
    PDB_OUTPUT_DIRECTORY "${FASTJUNGLE_RUNTIME_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    FOLDER "FastJungle/Applications"
)
fastjungle_enable_ipo(FastJungleCooker)

# Deploy OpenUSD, oneTBB, and plugin manifests beside the cooker.
file(MAKE_DIRECTORY "${FASTJUNGLE_GENERATED_DIR}")
set(FASTJUNGLE_DEPLOY_OPENUSD_SCRIPT
    "${FASTJUNGLE_GENERATED_DIR}/DeployOpenUSD.cmake"
)
file(WRITE "${FASTJUNGLE_DEPLOY_OPENUSD_SCRIPT}" [=[
if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED DESTINATION)
    message(FATAL_ERROR "SOURCE_ROOT and DESTINATION are required")
endif()

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

function(fastjungle_deploy_openusd target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${FASTJUNGLE_USD_DLL}"
            "$<TARGET_FILE_DIR:${target}>"
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_ROOT=${FASTJUNGLE_USD_INSTALL}"
            "-DDESTINATION=$<TARGET_FILE_DIR:${target}>"
            -P "${FASTJUNGLE_DEPLOY_OPENUSD_SCRIPT}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${FASTJUNGLE_TBB_INSTALL}/bin"
            "$<TARGET_FILE_DIR:${target}>"
        COMMENT "Deploying OpenUSD and oneTBB runtime files"
        VERBATIM
    )
endfunction()

fastjungle_deploy_openusd(FastJungleCooker)
endif()

# =============================================================================
# Shaders: main entry point, Shader Model 6.6
# =============================================================================

if(FASTJUNGLE_BUILD_RENDERER)
file(GLOB_RECURSE FASTJUNGLE_HLSL_SOURCES CONFIGURE_DEPENDS
    "${FASTJUNGLE_SHADER_DIR}/*.hlsl"
)
file(GLOB_RECURSE FASTJUNGLE_HLSL_INCLUDES CONFIGURE_DEPENDS
    "${FASTJUNGLE_SHADER_DIR}/*.hlsli"
)
list(SORT FASTJUNGLE_HLSL_SOURCES)
list(SORT FASTJUNGLE_HLSL_INCLUDES)

set(FASTJUNGLE_COMPILED_SHADERS "")
foreach(_shader IN LISTS FASTJUNGLE_HLSL_SOURCES)
    file(RELATIVE_PATH _relative_shader
        "${FASTJUNGLE_SHADER_DIR}"
        "${_shader}"
    )

    if(_shader MATCHES "\\.vs\\.hlsl$")
        set(_profile "vs_6_6")
    elseif(_shader MATCHES "\\.ps\\.hlsl$")
        set(_profile "ps_6_6")
    elseif(_shader MATCHES "\\.cs\\.hlsl$")
        set(_profile "cs_6_6")
    elseif(_shader MATCHES "\\.gs\\.hlsl$")
        set(_profile "gs_6_6")
    elseif(_shader MATCHES "\\.hs\\.hlsl$")
        set(_profile "hs_6_6")
    elseif(_shader MATCHES "\\.ds\\.hlsl$")
        set(_profile "ds_6_6")
    else()
        message(FATAL_ERROR
            "Unknown shader stage for '${_relative_shader}'. "
            "Use *.vs.hlsl, *.ps.hlsl, *.cs.hlsl, *.gs.hlsl, "
            "*.hs.hlsl, or *.ds.hlsl."
        )
    endif()

    string(REGEX REPLACE "\\.hlsl$" ".dxil"
        _relative_output
        "${_relative_shader}"
    )
    set(_output
        "${FASTJUNGLE_COMPILED_SHADER_DIR}/${_relative_output}"
    )
    get_filename_component(_output_directory "${_output}" DIRECTORY)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(_dxc_config_flags -Zi -Qembed_debug -Od)
    else()
        set(_dxc_config_flags -O3 -Qstrip_debug)
    endif()

    add_custom_command(
        OUTPUT "${_output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_output_directory}"
        COMMAND "${FASTJUNGLE_DXC_EXECUTABLE}"
            -nologo
            -E main
            -T "${_profile}"
            -HV 2021
            -I "${FASTJUNGLE_SHADER_DIR}"
            ${_dxc_config_flags}
            -Fo "${_output}"
            "${_shader}"
        DEPENDS
            "${_shader}"
            ${FASTJUNGLE_HLSL_INCLUDES}
        COMMENT "DXC ${_profile}: ${_relative_shader}"
        VERBATIM
        COMMAND_EXPAND_LISTS
    )
    list(APPEND FASTJUNGLE_COMPILED_SHADERS "${_output}")
endforeach()

add_custom_target(FastJungleShaders ALL
    DEPENDS ${FASTJUNGLE_COMPILED_SHADERS}
)
set_target_properties(FastJungleShaders PROPERTIES
    FOLDER "FastJungle/Shaders"
)

if(FASTJUNGLE_HLSL_SOURCES OR FASTJUNGLE_HLSL_INCLUDES)
    set_source_files_properties(
        ${FASTJUNGLE_HLSL_SOURCES}
        ${FASTJUNGLE_HLSL_INCLUDES}
        PROPERTIES
            HEADER_FILE_ONLY TRUE
            VS_TOOL_OVERRIDE "None"
    )
endif()

# =============================================================================
# Renderer: WIN32 executable with wWinMain
# =============================================================================

add_executable(FastJungleRenderer WIN32
    ${FASTJUNGLE_RENDERER_SOURCES}
    ${FASTJUNGLE_DX12_SOURCES}
    ${FASTJUNGLE_HEADERS}
    ${FASTJUNGLE_HLSL_SOURCES}
    ${FASTJUNGLE_HLSL_INCLUDES}
)
target_link_libraries(FastJungleRenderer PRIVATE
    FastJungle::ProjectOptions
    FastJungle::Core
    FastJungle::Scene
    DirectXTex
    d3d12
    dxgi
    dxguid
    user32
    gdi32
    shell32
    ole32
    windowscodecs
)
target_include_directories(FastJungleRenderer PRIVATE
    "${FASTJUNGLE_INCLUDE_DIR}"
    "${FASTJUNGLE_SHADER_DIR}"
)

# Fast floating-point is restricted to renderer translation units.
target_compile_options(FastJungleRenderer PRIVATE
    "$<$<CONFIG:Release>:/fp:fast>"
)
target_compile_definitions(FastJungleRenderer PRIVATE
    "FASTJUNGLE_DEFAULT_COOKED_DIR=\"${FASTJUNGLE_COOKED_DIR}\""
    "FASTJUNGLE_SHADER_OUTPUT_DIR=\"${FASTJUNGLE_COMPILED_SHADER_DIR}\""
)
add_dependencies(FastJungleRenderer FastJungleShaders)
set_target_properties(FastJungleRenderer PROPERTIES
    OUTPUT_NAME "FastJungle"
    RUNTIME_OUTPUT_DIRECTORY "${FASTJUNGLE_RUNTIME_DIR}"
    PDB_OUTPUT_DIRECTORY "${FASTJUNGLE_RUNTIME_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    FOLDER "FastJungle/Applications"
)
fastjungle_enable_ipo(FastJungleRenderer)
endif()

# =============================================================================
# IDE organization
# =============================================================================

source_group(TREE "${FASTJUNGLE_SOURCE_DIR}" PREFIX "Source"
    FILES
        ${FASTJUNGLE_CORE_SOURCES}
        ${FASTJUNGLE_SCENE_SOURCES}
        ${FASTJUNGLE_COOKER_MAIN}
        ${FASTJUNGLE_COOKER_SOURCES}
        ${FASTJUNGLE_RENDERER_SOURCES}
)
source_group(TREE "${FASTJUNGLE_INCLUDE_DIR}" PREFIX "Include"
    FILES ${FASTJUNGLE_HEADERS}
)
if(FASTJUNGLE_HLSL_SOURCES OR FASTJUNGLE_HLSL_INCLUDES)
    source_group(TREE "${FASTJUNGLE_SHADER_DIR}" PREFIX "Shaders"
        FILES
            ${FASTJUNGLE_HLSL_SOURCES}
            ${FASTJUNGLE_HLSL_INCLUDES}
    )
endif()

message(STATUS "FastJungle configuration : ${CMAKE_BUILD_TYPE}")
message(STATUS "Runtime output           : ${FASTJUNGLE_RUNTIME_DIR}")
message(STATUS "Build renderer           : ${FASTJUNGLE_BUILD_RENDERER}")
message(STATUS "Build cooker             : ${FASTJUNGLE_BUILD_COOKER}")
message(STATUS "Cooked output            : ${FASTJUNGLE_COOKED_DIR}")
if(FASTJUNGLE_BUILD_RENDERER)
    message(STATUS "DXC                       : ${FASTJUNGLE_DXC_EXECUTABLE}")
endif()
if(FASTJUNGLE_BUILD_COOKER)
    message(STATUS "USD root                  : ${FASTJUNGLE_SCENE_USD}")
    message(STATUS "OpenUSD tag               : ${FASTJUNGLE_OPENUSD_TAG}")
endif()
