## ScratchBird Source CMake Configuration
## Core library and executable wiring

# Core library: scratchbird_core
file(GLOB_RECURSE SCRATCHBIRD_CORE_SOURCES
    core/*.cpp
)

add_library(scratchbird_core STATIC
    ${SCRATCHBIRD_CORE_SOURCES}
)

target_include_directories(scratchbird_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

if(LZ4_FOUND)
    target_include_directories(scratchbird_core PUBLIC ${LZ4_INCLUDE_DIRS})
    target_link_libraries(scratchbird_core PUBLIC ${LZ4_LIBRARIES})
endif()

set_target_properties(scratchbird_core PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

# Parser library: scratchbird_parser
file(GLOB_RECURSE SCRATCHBIRD_PARSER_SOURCES
    parser/*.cpp
)

add_library(scratchbird_parser STATIC
    ${SCRATCHBIRD_PARSER_SOURCES}
)

target_include_directories(scratchbird_parser PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(scratchbird_parser PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

# SBLR library: scratchbird_sblr
file(GLOB_RECURSE SCRATCHBIRD_SBLR_SOURCES
    sblr/*.cpp
)

add_library(scratchbird_sblr STATIC
    ${SCRATCHBIRD_SBLR_SOURCES}
)

target_include_directories(scratchbird_sblr PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(scratchbird_sblr PUBLIC
    scratchbird_parser
    scratchbird_core
)

set_target_properties(scratchbird_sblr PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

# Main executable (placeholder)
add_executable(scratchbird main.cpp)

target_link_libraries(scratchbird PRIVATE scratchbird_core scratchbird_parser scratchbird_sblr)

set_target_properties(scratchbird PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Installation (for future use)
install(TARGETS scratchbird scratchbird_core scratchbird_parser scratchbird_sblr
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)