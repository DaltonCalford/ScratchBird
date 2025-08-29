cmake_minimum_required(VERSION 3.20)

project(scratchbird_diag LANGUAGES C CXX)

add_executable(scratchbird-heap-dump src/heap_dump.cpp)
target_include_directories(scratchbird-heap-dump PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(scratchbird-heap-dump PRIVATE scratchbird_engine)

install(TARGETS scratchbird-heap-dump RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

