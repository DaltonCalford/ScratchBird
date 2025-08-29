cmake_minimum_required(VERSION 3.20)

project(scratchbird_cli LANGUAGES C CXX)

add_library(cli_common STATIC src/common.cpp)
target_include_directories(cli_common PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(cli_common PRIVATE scratchbird_engine)

add_executable(scratchbird-createdb src/createdb.cpp)
target_link_libraries(scratchbird-createdb PRIVATE cli_common scratchbird_engine)

add_executable(scratchbird-dropdb src/dropdb.cpp)
target_link_libraries(scratchbird-dropdb PRIVATE cli_common scratchbird_engine)

install(TARGETS scratchbird-createdb scratchbird-dropdb RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

