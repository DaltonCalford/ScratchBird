cmake_minimum_required(VERSION 3.20)

project(scratchbird_bench LANGUAGES C CXX)

add_executable(scratchbird-bench src/main.cpp)
target_include_directories(scratchbird-bench PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(scratchbird-bench PRIVATE scratchbird_engine)

install(TARGETS scratchbird-bench RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

