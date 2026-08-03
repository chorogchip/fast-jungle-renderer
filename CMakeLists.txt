cmake_minimum_required(VERSION 3.26)

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

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(FASTJUNGLE_BUILD "renderer" CACHE STRING "FastJungle target to build")
set_property(CACHE FASTJUNGLE_BUILD PROPERTY STRINGS renderer cooker)

if(NOT FASTJUNGLE_BUILD STREQUAL "renderer" AND
   NOT FASTJUNGLE_BUILD STREQUAL "cooker")
    message(FATAL_ERROR "FASTJUNGLE_BUILD must be 'renderer' or 'cooker'.")
endif()

if(NOT CMAKE_CONFIGURATION_TYPES)
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build configuration" FORCE)
    endif()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release)
endif()

option(FASTJUNGLE_ENABLE_IPO
    "Enable Release IPO/LTCG for FastJungle"
    ON
)

set(FASTJUNGLE_SOURCE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(FASTJUNGLE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(FASTJUNGLE_SHADER_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(FASTJUNGLE_ASSET_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/assets")

set(FASTJUNGLE_COOKED_DIR
    "${FASTJUNGLE_ASSET_DIR}/cooked"
    CACHE PATH "Cooked scene output directory"
)

if(CMAKE_CONFIGURATION_TYPES)
    set(FASTJUNGLE_RUNTIME_DIR "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
else()
    set(FASTJUNGLE_RUNTIME_DIR "${CMAKE_BINARY_DIR}/bin")
endif()
set(FASTJUNGLE_COMPILED_SHADER_DIR
    "${FASTJUNGLE_RUNTIME_DIR}/shaders"
)

if(FASTJUNGLE_BUILD STREQUAL "cooker")
    # OpenUSD is built once as Release, so every cooker dependency uses /MD.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
endif()

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

if(FASTJUNGLE_BUILD STREQUAL "renderer")
    include(FetchContent)
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

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
    set_property(TARGET FastJungleRenderer PROPERTY
        MSVC_RUNTIME_LIBRARY
            "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
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
    include(FetchContent)
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

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
        OFF
    )

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
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

    # cooker-debug still links the Release OpenUSD ABI. DirectXTex must match it.
    set_property(TARGET DirectXTex PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
    )
    get_target_property(FASTJUNGLE_DIRECTXTEX_TYPE DirectXTex TYPE)
    if(NOT FASTJUNGLE_DIRECTXTEX_TYPE STREQUAL "INTERFACE_LIBRARY")
        target_compile_definitions(DirectXTex PRIVATE
            "$<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>"
        )
    endif()

    if(FASTJUNGLE_ENABLE_OPENEXR)
        find_package(OpenEXR CONFIG REQUIRED)
    endif()

    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/OpenUSD.cmake")

    add_executable(FastJungleCooker
        ${FASTJUNGLE_COMMON_SOURCES}
        ${FASTJUNGLE_COOKER_SOURCES}
    )
    set_property(TARGET FastJungleCooker PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
    )
    fastjungle_apply_options(FastJungleCooker)
    target_compile_definitions(FastJungleCooker PRIVATE
        "$<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>"
        "FASTJUNGLE_DEFAULT_SCENE_USD=\"${FASTJUNGLE_SCENE_USD}\""
        "FASTJUNGLE_DEFAULT_TEXTURE_DIR=\"${FASTJUNGLE_TEXTURE_DIR}\""
        "FASTJUNGLE_DEFAULT_COOKED_DIR=\"${FASTJUNGLE_COOKED_DIR}\""
    )
    target_include_directories(FastJungleCooker PRIVATE
        "${FASTJUNGLE_INCLUDE_DIR}"
    )
    target_link_libraries(FastJungleCooker PRIVATE
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

    fastjungle_use_openusd(FastJungleCooker)

    set_target_properties(FastJungleCooker PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${FASTJUNGLE_RUNTIME_DIR}"
        PDB_OUTPUT_DIRECTORY "${FASTJUNGLE_RUNTIME_DIR}"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()

message(STATUS "FastJungle target        : ${FASTJUNGLE_BUILD}")
message(STATUS "Runtime output           : ${FASTJUNGLE_RUNTIME_DIR}")
message(STATUS "Cooked output            : ${FASTJUNGLE_COOKED_DIR}")
