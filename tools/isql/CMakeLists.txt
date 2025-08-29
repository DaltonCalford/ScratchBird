cmake_minimum_required(VERSION 3.20)

project(isql_tool LANGUAGES C CXX)

add_executable(isql src/main.cpp)

target_include_directories(isql PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(isql PRIVATE scratchbird_engine)

install(TARGETS isql RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
