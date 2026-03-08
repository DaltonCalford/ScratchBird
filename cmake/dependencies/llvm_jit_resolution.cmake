option(SCRATCHBIRD_ENABLE_LLVM_JIT
    "Build the LLVM-backed JIT provider when a usable LLVM toolchain is present."
    ON)
set(SCRATCHBIRD_LLVM_CONFIG_EXECUTABLE "" CACHE FILEPATH
    "Explicit llvm-config executable used for LLVM JIT provider discovery")
set(SCRATCHBIRD_LLVM_PREFERRED_VERSIONS "21;20;19;18;17;16" CACHE STRING
    "Preferred LLVM major versions for JIT provider discovery")

if(TARGET scratchbird_llvm_jit_provider)
    return()
endif()

function(_scratchbird_set_llvm_provider_definitions available provider_id provider_version host_triple)
    set_property(TARGET scratchbird_llvm_jit_provider PROPERTY INTERFACE_COMPILE_DEFINITIONS
        "SCRATCHBIRD_HAVE_LLVM_JIT=${available}"
        "SCRATCHBIRD_LLVM_JIT_PROVIDER_ID=\"${provider_id}\""
        "SCRATCHBIRD_LLVM_JIT_PROVIDER_VERSION=\"${provider_version}\""
        "SCRATCHBIRD_LLVM_JIT_HOST_TRIPLE=\"${host_triple}\"")
endfunction()

add_library(scratchbird_llvm_jit_provider INTERFACE)

set(SCRATCHBIRD_LLVM_JIT_AVAILABLE FALSE)
set(SCRATCHBIRD_LLVM_JIT_PROVIDER_ID "llvm")
set(SCRATCHBIRD_LLVM_JIT_PROVIDER_VERSION "unavailable")
set(SCRATCHBIRD_LLVM_JIT_HOST_TRIPLE "native")
set(SCRATCHBIRD_LLVM_JIT_CONFIG_PATH "")
_scratchbird_set_llvm_provider_definitions(
    0
    "${SCRATCHBIRD_LLVM_JIT_PROVIDER_ID}"
    "${SCRATCHBIRD_LLVM_JIT_PROVIDER_VERSION}"
    "${SCRATCHBIRD_LLVM_JIT_HOST_TRIPLE}")

if(NOT SCRATCHBIRD_ENABLE_LLVM_JIT)
    message(STATUS "LLVM JIT provider: DISABLED by SCRATCHBIRD_ENABLE_LLVM_JIT=OFF")
    return()
endif()

if(SCRATCHBIRD_WINDOWS_CROSS_COMPILE)
    message(STATUS "LLVM JIT provider: DISABLED during Windows cross-compilation")
    return()
endif()

set(_scratchbird_llvm_config "")
if(SCRATCHBIRD_LLVM_CONFIG_EXECUTABLE)
    if(EXISTS "${SCRATCHBIRD_LLVM_CONFIG_EXECUTABLE}")
        set(_scratchbird_llvm_config "${SCRATCHBIRD_LLVM_CONFIG_EXECUTABLE}")
    else()
        message(WARNING
            "SCRATCHBIRD_LLVM_CONFIG_EXECUTABLE was set to '${SCRATCHBIRD_LLVM_CONFIG_EXECUTABLE}' but that path does not exist")
    endif()
endif()

if(NOT _scratchbird_llvm_config)
    unset(_scratchbird_llvm_config CACHE)
    set(_scratchbird_llvm_config_names llvm-config)
    set(_scratchbird_llvm_hints "")
    foreach(_scratchbird_llvm_version IN LISTS SCRATCHBIRD_LLVM_PREFERRED_VERSIONS)
        list(APPEND _scratchbird_llvm_config_names "llvm-config-${_scratchbird_llvm_version}")
        list(APPEND _scratchbird_llvm_hints
            "/usr/lib/llvm-${_scratchbird_llvm_version}/bin"
            "/usr/local/opt/llvm@${_scratchbird_llvm_version}/bin"
            "/opt/homebrew/opt/llvm@${_scratchbird_llvm_version}/bin")
    endforeach()
    foreach(_scratchbird_llvm_candidate IN LISTS _scratchbird_llvm_config_names)
        find_program(_scratchbird_llvm_config_candidate_path
            NAMES ${_scratchbird_llvm_candidate}
            HINTS ${_scratchbird_llvm_hints}
            NO_CACHE)
        if(_scratchbird_llvm_config_candidate_path)
            set(_scratchbird_llvm_config "${_scratchbird_llvm_config_candidate_path}")
            break()
        endif()
    endforeach()
endif()

if(NOT _scratchbird_llvm_config)
    message(STATUS "LLVM JIT provider: DISABLED (no usable llvm-config executable found)")
    return()
endif()

execute_process(
    COMMAND "${_scratchbird_llvm_config}" --version
    OUTPUT_VARIABLE _scratchbird_llvm_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _scratchbird_llvm_version_rc)
execute_process(
    COMMAND "${_scratchbird_llvm_config}" --host-target
    OUTPUT_VARIABLE _scratchbird_llvm_host_target
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _scratchbird_llvm_host_target_rc)
execute_process(
    COMMAND "${_scratchbird_llvm_config}" --includedir
    OUTPUT_VARIABLE _scratchbird_llvm_include_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _scratchbird_llvm_include_rc)
execute_process(
    COMMAND "${_scratchbird_llvm_config}" --libfiles core orcjit native nativecodegen mcjit mc support
    OUTPUT_VARIABLE _scratchbird_llvm_libfiles_raw
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _scratchbird_llvm_libfiles_rc)
execute_process(
    COMMAND "${_scratchbird_llvm_config}" --system-libs
    OUTPUT_VARIABLE _scratchbird_llvm_system_libs_raw
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _scratchbird_llvm_system_libs_rc)

if(NOT _scratchbird_llvm_version_rc EQUAL 0 OR
   NOT _scratchbird_llvm_host_target_rc EQUAL 0 OR
   NOT _scratchbird_llvm_include_rc EQUAL 0 OR
   NOT _scratchbird_llvm_libfiles_rc EQUAL 0 OR
   NOT EXISTS "${_scratchbird_llvm_include_dir}")
    message(STATUS
        "LLVM JIT provider: DISABLED (llvm-config at ${_scratchbird_llvm_config} did not return a usable toolchain description)")
    return()
endif()

string(REPLACE "\n" " " _scratchbird_llvm_libfiles_raw "${_scratchbird_llvm_libfiles_raw}")
string(REPLACE "\n" " " _scratchbird_llvm_system_libs_raw "${_scratchbird_llvm_system_libs_raw}")
separate_arguments(_scratchbird_llvm_libfiles NATIVE_COMMAND "${_scratchbird_llvm_libfiles_raw}")
if(_scratchbird_llvm_system_libs_raw)
    separate_arguments(_scratchbird_llvm_system_libs NATIVE_COMMAND "${_scratchbird_llvm_system_libs_raw}")
else()
    set(_scratchbird_llvm_system_libs "")
endif()

if(NOT _scratchbird_llvm_libfiles)
    message(STATUS
        "LLVM JIT provider: DISABLED (llvm-config at ${_scratchbird_llvm_config} did not report linkable libraries)")
    return()
endif()

foreach(_scratchbird_llvm_lib IN LISTS _scratchbird_llvm_libfiles)
    if(_scratchbird_llvm_lib MATCHES "^-l")
        continue()
    endif()
    if(NOT EXISTS "${_scratchbird_llvm_lib}")
        message(STATUS
            "LLVM JIT provider: DISABLED (reported library '${_scratchbird_llvm_lib}' does not exist)")
        return()
    endif()
endforeach()

set(SCRATCHBIRD_LLVM_JIT_AVAILABLE TRUE)
set(SCRATCHBIRD_LLVM_JIT_PROVIDER_VERSION "${_scratchbird_llvm_version}")
set(SCRATCHBIRD_LLVM_JIT_HOST_TRIPLE "${_scratchbird_llvm_host_target}")
set(SCRATCHBIRD_LLVM_JIT_CONFIG_PATH "${_scratchbird_llvm_config}")

target_include_directories(scratchbird_llvm_jit_provider INTERFACE
    "${_scratchbird_llvm_include_dir}")
target_link_libraries(scratchbird_llvm_jit_provider INTERFACE
    ${_scratchbird_llvm_libfiles}
    ${_scratchbird_llvm_system_libs})
_scratchbird_set_llvm_provider_definitions(
    1
    "${SCRATCHBIRD_LLVM_JIT_PROVIDER_ID}"
    "${SCRATCHBIRD_LLVM_JIT_PROVIDER_VERSION}"
    "${SCRATCHBIRD_LLVM_JIT_HOST_TRIPLE}")

message(STATUS
    "LLVM JIT provider: ENABLED (${SCRATCHBIRD_LLVM_JIT_PROVIDER_VERSION}, ${SCRATCHBIRD_LLVM_JIT_HOST_TRIPLE}, ${SCRATCHBIRD_LLVM_JIT_CONFIG_PATH})")
