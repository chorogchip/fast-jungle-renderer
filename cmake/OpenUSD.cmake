include(ExternalProject)

set(FASTJUNGLE_OPENUSD_TAG "v26.05" CACHE STRING "Pinned OpenUSD tag")
set(FASTJUNGLE_ONETBB_TAG "v2021.9.0" CACHE STRING "Pinned oneTBB tag")
set(FASTJUNGLE_DEPENDENCY_DIR
    "${CMAKE_SOURCE_DIR}/out/deps"
    CACHE PATH "Shared FastJungle dependency directory"
)

set(FASTJUNGLE_TBB_ROOT "${FASTJUNGLE_DEPENDENCY_DIR}/onetbb-release")
set(FASTJUNGLE_TBB_BUILD "${FASTJUNGLE_DEPENDENCY_DIR}/onetbb-build")
set(FASTJUNGLE_TBB_INSTALL "${FASTJUNGLE_DEPENDENCY_DIR}/onetbb-install")

set(FASTJUNGLE_OPENUSD_ROOT "${FASTJUNGLE_DEPENDENCY_DIR}/openusd-release")
set(FASTJUNGLE_OPENUSD_BUILD "${FASTJUNGLE_DEPENDENCY_DIR}/openusd-build")
set(FASTJUNGLE_OPENUSD_INSTALL "${FASTJUNGLE_DEPENDENCY_DIR}/openusd-install")

set(FASTJUNGLE_TBB_IMPLIB
    "${FASTJUNGLE_TBB_INSTALL}/lib/tbb12.lib"
)
set(FASTJUNGLE_TBB_DLL
    "${FASTJUNGLE_TBB_INSTALL}/bin/tbb12.dll"
)
set(FASTJUNGLE_OPENUSD_IMPLIB
    "${FASTJUNGLE_OPENUSD_INSTALL}/lib/usd_ms.lib"
)
set(FASTJUNGLE_OPENUSD_DLL
    "${FASTJUNGLE_OPENUSD_INSTALL}/lib/usd_ms.dll"
)

if(NOT EXISTS "${FASTJUNGLE_OPENUSD_IMPLIB}" OR
   NOT EXISTS "${FASTJUNGLE_OPENUSD_DLL}" OR
   NOT EXISTS "${FASTJUNGLE_TBB_IMPLIB}" OR
   NOT EXISTS "${FASTJUNGLE_TBB_DLL}")

    ExternalProject_Add(FastJungleOneTBB
        GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
        GIT_TAG "${FASTJUNGLE_ONETBB_TAG}"
        GIT_SHALLOW TRUE
        UPDATE_DISCONNECTED TRUE

        PREFIX "${FASTJUNGLE_TBB_ROOT}"
        BINARY_DIR "${FASTJUNGLE_TBB_BUILD}"
        INSTALL_DIR "${FASTJUNGLE_TBB_INSTALL}"
        INSTALL_BYPRODUCTS
            "${FASTJUNGLE_TBB_IMPLIB}"
            "${FASTJUNGLE_TBB_DLL}"

        CMAKE_ARGS
            "-DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>"
            "-DCMAKE_BUILD_TYPE:STRING=Release"
            "-DCMAKE_MSVC_RUNTIME_LIBRARY:STRING=MultiThreadedDLL"
            "-DCMAKE_POLICY_DEFAULT_CMP0091:STRING=NEW"
            "-DCMAKE_POLICY_VERSION_MINIMUM:STRING=3.5"
            "-DBUILD_SHARED_LIBS:BOOL=ON"
            "-DTBB_TEST:BOOL=OFF"
            "-DTBB_EXAMPLES:BOOL=OFF"
            "-DTBB_STRICT:BOOL=OFF"

        BUILD_COMMAND
            "${CMAKE_COMMAND}" --build <BINARY_DIR>
                --config Release --parallel
        INSTALL_COMMAND
            "${CMAKE_COMMAND}" --install <BINARY_DIR>
                --config Release
    )

    ExternalProject_Add(FastJungleOpenUSD
        GIT_REPOSITORY https://github.com/PixarAnimationStudios/OpenUSD.git
        GIT_TAG "${FASTJUNGLE_OPENUSD_TAG}"
        GIT_SHALLOW TRUE
        UPDATE_DISCONNECTED TRUE
        DEPENDS FastJungleOneTBB

        PREFIX "${FASTJUNGLE_OPENUSD_ROOT}"
        BINARY_DIR "${FASTJUNGLE_OPENUSD_BUILD}"
        INSTALL_DIR "${FASTJUNGLE_OPENUSD_INSTALL}"
        INSTALL_BYPRODUCTS
            "${FASTJUNGLE_OPENUSD_IMPLIB}"
            "${FASTJUNGLE_OPENUSD_DLL}"

        CMAKE_ARGS
            "-DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>"
            "-DCMAKE_BUILD_TYPE:STRING=Release"
            "-DCMAKE_MSVC_RUNTIME_LIBRARY:STRING=MultiThreadedDLL"
            "-DCMAKE_POLICY_DEFAULT_CMP0091:STRING=NEW"
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
                --config Release --parallel
        INSTALL_COMMAND
            "${CMAKE_COMMAND}" --install <BINARY_DIR>
                --config Release
    )
endif()

function(fastjungle_use_openusd target)
    add_library(FastJungleOpenUSDLibrary SHARED IMPORTED)
    set_target_properties(FastJungleOpenUSDLibrary PROPERTIES
        IMPORTED_IMPLIB "${FASTJUNGLE_OPENUSD_IMPLIB}"
        IMPORTED_LOCATION "${FASTJUNGLE_OPENUSD_DLL}"
    )

    target_include_directories(${target} PRIVATE
        "${FASTJUNGLE_OPENUSD_INSTALL}/include"
        "${FASTJUNGLE_TBB_INSTALL}/include"
    )
    target_link_libraries(${target} PRIVATE
        FastJungleOpenUSDLibrary
        "${FASTJUNGLE_TBB_IMPLIB}"
        Shlwapi
        Dbghelp
        Ws2_32
    )

    if(TARGET FastJungleOpenUSD)
        add_dependencies(${target} FastJungleOpenUSD)
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${FASTJUNGLE_OPENUSD_DLL}"
            "$<TARGET_FILE_DIR:${target}>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${FASTJUNGLE_TBB_DLL}"
            "$<TARGET_FILE_DIR:${target}>"
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_ROOT=${FASTJUNGLE_OPENUSD_INSTALL}"
            "-DDESTINATION=$<TARGET_FILE_DIR:${target}>"
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/DeployOpenUSD.cmake"
        COMMENT "Deploying OpenUSD runtime files"
        VERBATIM
    )
endfunction()
