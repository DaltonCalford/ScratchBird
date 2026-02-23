# Windows dependency resolution contract for deterministic cross-OS builds.

if(NOT WIN32)
    return()
endif()

option(SCRATCHBIRD_WINDOWS_REQUIRE_VCPKG
    "Require vcpkg manifest-mode toolchain on Windows configure."
    ON
)

set(SCRATCHBIRD_WINDOWS_VCPKG_BASELINE
    "05442024c3fda64320bd25d2251cc9807b84fb6f"
    CACHE STRING "Pinned vcpkg baseline commit for deterministic Windows dependencies."
)

set(_scratchbird_windows_vcpkg FALSE)
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    string(TOLOWER "${CMAKE_TOOLCHAIN_FILE}" _scratchbird_toolchain_lower)
    if(_scratchbird_toolchain_lower MATCHES "vcpkg")
        set(_scratchbird_windows_vcpkg TRUE)
    endif()
endif()

if(SCRATCHBIRD_WINDOWS_REQUIRE_VCPKG AND NOT _scratchbird_windows_vcpkg)
    message(FATAL_ERROR
        "Windows builds require vcpkg manifest mode for deterministic dependencies. "
        "Set CMAKE_TOOLCHAIN_FILE to <vcpkg>/scripts/buildsystems/vcpkg.cmake "
        "and keep vcpkg.json baseline at ${SCRATCHBIRD_WINDOWS_VCPKG_BASELINE}."
    )
endif()

if(_scratchbird_windows_vcpkg)
    if(NOT DEFINED VCPKG_TARGET_TRIPLET)
        set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg target triplet" FORCE)
    endif()
    if(NOT DEFINED VCPKG_FEATURE_FLAGS)
        set(VCPKG_FEATURE_FLAGS "manifests,registries" CACHE STRING "vcpkg feature flags")
    endif()
    message(STATUS
        "Windows deterministic dependency mode enabled (vcpkg baseline "
        "${SCRATCHBIRD_WINDOWS_VCPKG_BASELINE}, triplet ${VCPKG_TARGET_TRIPLET})"
    )
endif()
