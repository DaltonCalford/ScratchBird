# ScratchBird Tools CMake Configuration

# sb_timezone_loader - Timezone data loader
add_executable(sb_timezone_loader sb_timezone_loader.cpp)

target_link_libraries(sb_timezone_loader PRIVATE scratchbird_link_group)
if(NOT WIN32)
    target_link_libraries(sb_timezone_loader PRIVATE
        "-Wl,--start-group"
        scratchbird_core
        scratchbird_sblr
        "-Wl,--end-group"
    )
endif()

target_include_directories(sb_timezone_loader PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(sb_timezone_loader PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Installation
install(TARGETS sb_timezone_loader
    RUNTIME DESTINATION bin
)

# Benchmark tools (existing)
# NOTE: Temporarily disabled - uses TupleHeader API (back_version_tid, ctid_page, ctid_item)
#       that doesn't match current implementation (back_version_gpid)
# add_executable(benchmark_mga benchmark_mga.cpp)
# target_link_libraries(benchmark_mga PRIVATE scratchbird_core)
# target_include_directories(benchmark_mga PRIVATE ${CMAKE_SOURCE_DIR}/include)
# set_target_properties(benchmark_mga PROPERTIES
#     CXX_STANDARD 17
#     CXX_STANDARD_REQUIRED YES
#     CXX_EXTENSIONS NO
# )

# NOTE: Temporarily disabled - uses ProcArrayManager::initialize with wrong signature
# add_executable(benchmark_group_commit benchmark_group_commit.cpp)
# target_link_libraries(benchmark_group_commit PRIVATE scratchbird_core)
# target_include_directories(benchmark_group_commit PRIVATE ${CMAKE_SOURCE_DIR}/include)
# set_target_properties(benchmark_group_commit PROPERTIES
#     CXX_STANDARD 17
#     CXX_STANDARD_REQUIRED YES
#     CXX_EXTENSIONS NO
# )

# NOTE: Temporarily disabled - uses TupleHeader API (back_version_tid, ctid_page, ctid_item)
# add_executable(validate_mga_alpha validate_mga_alpha.cpp)
# target_link_libraries(validate_mga_alpha PRIVATE scratchbird_core)
# target_include_directories(validate_mga_alpha PRIVATE ${CMAKE_SOURCE_DIR}/include)
# set_target_properties(validate_mga_alpha PROPERTIES
#     CXX_STANDARD 17
#     CXX_STANDARD_REQUIRED YES
#     CXX_EXTENSIONS NO
# )
