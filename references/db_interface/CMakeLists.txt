# CMakeLists.txt for Multi-Database C++ Interface
cmake_minimum_required(VERSION 3.14)
project(DatabaseInterface VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Options for enabling/disabling specific databases
option(ENABLE_MYSQL "Enable MySQL support" ON)
option(ENABLE_MARIADB "Enable MariaDB support" ON)
option(ENABLE_POSTGRESQL "Enable PostgreSQL support" ON)
option(ENABLE_MSSQL "Enable Microsoft SQL Server support" ON)
option(ENABLE_FIREBIRD "Enable Firebird SQL support" ON)
option(ENABLE_ODBC "Enable generic ODBC support" ON)
option(ENABLE_JDBC "Enable JDBC via JNI support" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_TESTS "Build unit tests" OFF)

# Find required packages
find_package(Threads REQUIRED)

# Find database libraries
if(ENABLE_MYSQL OR ENABLE_MARIADB)
    # Try to find MariaDB Connector/C++
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(MARIADB libmariadb)
        if(MARIADB_FOUND)
            message(STATUS "Found MariaDB Connector: ${MARIADB_VERSION}")
        endif()
    endif()
    
    # If MariaDB not found, try MySQL
    if(NOT MARIADB_FOUND AND ENABLE_MYSQL)
        find_path(MYSQL_INCLUDE_DIR mysql.h
            PATHS /usr/include/mysql /usr/local/include/mysql
            PATH_SUFFIXES mysql)
        
        find_library(MYSQL_LIBRARY
            NAMES mysqlclient mysql
            PATHS /usr/lib /usr/local/lib
            PATH_SUFFIXES mysql)
        
        if(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
            set(MYSQL_FOUND TRUE)
            message(STATUS "Found MySQL: ${MYSQL_LIBRARY}")
        endif()
    endif()
endif()

if(ENABLE_POSTGRESQL)
    # Find PostgreSQL
    find_package(PostgreSQL)
    if(PostgreSQL_FOUND)
        message(STATUS "Found PostgreSQL: ${PostgreSQL_VERSION}")
        
        # Try to find libpqxx
        find_path(PQXX_INCLUDE_DIR pqxx/pqxx
            PATHS /usr/include /usr/local/include)
        
        find_library(PQXX_LIBRARY
            NAMES pqxx
            PATHS /usr/lib /usr/local/lib)
        
        if(PQXX_INCLUDE_DIR AND PQXX_LIBRARY)
            set(PQXX_FOUND TRUE)
            message(STATUS "Found libpqxx: ${PQXX_LIBRARY}")
        endif()
    endif()
endif()

if(ENABLE_MSSQL)
    # Find ODBC
    if(WIN32)
        set(ODBC_FOUND TRUE)
        set(ODBC_LIBRARIES odbc32 odbccp32)
        message(STATUS "Using Windows ODBC")
    else()
        # Find unixODBC or iODBC on Unix/Linux
        find_path(ODBC_INCLUDE_DIR sql.h
            PATHS /usr/include /usr/local/include
            PATH_SUFFIXES odbc iodbc)
        
        find_library(ODBC_LIBRARY
            NAMES odbc iodbc unixodbc
            PATHS /usr/lib /usr/local/lib)
        
        if(ODBC_INCLUDE_DIR AND ODBC_LIBRARY)
            set(ODBC_FOUND TRUE)
            set(ODBC_LIBRARIES ${ODBC_LIBRARY})
            message(STATUS "Found ODBC: ${ODBC_LIBRARY}")
        endif()
        
        # Try to find FreeTDS as alternative
        find_path(FREETDS_INCLUDE_DIR sybdb.h
            PATHS /usr/include /usr/local/include
            PATH_SUFFIXES freetds)
        
        find_library(FREETDS_LIBRARY
            NAMES sybdb tdsodbc
            PATHS /usr/lib /usr/local/lib)
        
        if(FREETDS_INCLUDE_DIR AND FREETDS_LIBRARY)
            set(FREETDS_FOUND TRUE)
            message(STATUS "Found FreeTDS: ${FREETDS_LIBRARY}")
        endif()
    endif()
endif()

if(ENABLE_FIREBIRD)
    # Find Firebird
    find_path(FIREBIRD_INCLUDE_DIR ibase.h
        PATHS /usr/include /usr/include/firebird /opt/firebird/include
        PATH_SUFFIXES firebird)
    
    find_library(FIREBIRD_LIBRARY
        NAMES fbclient fbembed
        PATHS /usr/lib /usr/lib/x86_64-linux-gnu /opt/firebird/lib)
    
    if(FIREBIRD_INCLUDE_DIR AND FIREBIRD_LIBRARY)
        set(FIREBIRD_FOUND TRUE)
        message(STATUS "Found Firebird: ${FIREBIRD_LIBRARY}")
    endif()
    
    # Try to find IBPP
    find_path(IBPP_INCLUDE_DIR ibpp.h
        PATHS /usr/include /usr/local/include ${CMAKE_CURRENT_SOURCE_DIR}/ibpp)
    
    if(IBPP_INCLUDE_DIR)
        set(IBPP_FOUND TRUE)
        message(STATUS "Found IBPP: ${IBPP_INCLUDE_DIR}")
    endif()
endif()

# ODBC is already handled above for MSSQL, but ensure it's available generically
if(ENABLE_ODBC AND NOT ODBC_FOUND)
    if(WIN32)
        set(ODBC_FOUND TRUE)
        set(ODBC_LIBRARIES odbc32 odbccp32)
        message(STATUS "Using Windows ODBC")
    else()
        find_path(ODBC_INCLUDE_DIR sql.h
            PATHS /usr/include /usr/local/include
            PATH_SUFFIXES odbc iodbc)
        
        find_library(ODBC_LIBRARY
            NAMES odbc iodbc unixodbc
            PATHS /usr/lib /usr/local/lib)
        
        if(ODBC_INCLUDE_DIR AND ODBC_LIBRARY)
            set(ODBC_FOUND TRUE)
            set(ODBC_LIBRARIES ${ODBC_LIBRARY})
            message(STATUS "Found ODBC: ${ODBC_LIBRARY}")
        endif()
    endif()
endif()

if(ENABLE_JDBC)
    # Find JNI
    find_package(JNI)
    if(JNI_FOUND)
        message(STATUS "Found JNI: ${JNI_LIBRARIES}")
        message(STATUS "JNI Include Dirs: ${JNI_INCLUDE_DIRS}")
    endif()
endif()

# Create interface library for database support
add_library(dbinterface INTERFACE)

# Add include directories
target_include_directories(dbinterface INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link libraries based on what was found
target_link_libraries(dbinterface INTERFACE Threads::Threads)

# Add database-specific compile definitions and libraries
if(MARIADB_FOUND)
    target_compile_definitions(dbinterface INTERFACE HAS_MARIADB)
    target_link_libraries(dbinterface INTERFACE ${MARIADB_LIBRARIES})
    target_include_directories(dbinterface INTERFACE ${MARIADB_INCLUDE_DIRS})
elseif(MYSQL_FOUND)
    target_compile_definitions(dbinterface INTERFACE HAS_MYSQL)
    target_link_libraries(dbinterface INTERFACE ${MYSQL_LIBRARY})
    target_include_directories(dbinterface INTERFACE ${MYSQL_INCLUDE_DIR})
endif()

if(PostgreSQL_FOUND)
    target_compile_definitions(dbinterface INTERFACE HAS_POSTGRESQL)
    target_link_libraries(dbinterface INTERFACE PostgreSQL::PostgreSQL)
    
    if(PQXX_FOUND)
        target_compile_definitions(dbinterface INTERFACE HAS_PQXX)
        target_link_libraries(dbinterface INTERFACE ${PQXX_LIBRARY})
        target_include_directories(dbinterface INTERFACE ${PQXX_INCLUDE_DIR})
    endif()
endif()

if(ODBC_FOUND)
    target_compile_definitions(dbinterface INTERFACE HAS_ODBC)
    target_link_libraries(dbinterface INTERFACE ${ODBC_LIBRARIES})
    if(NOT WIN32)
        target_include_directories(dbinterface INTERFACE ${ODBC_INCLUDE_DIR})
    endif()
endif()

if(FREETDS_FOUND)
    target_compile_definitions(dbinterface INTERFACE HAS_FREETDS)
    target_link_libraries(dbinterface INTERFACE ${FREETDS_LIBRARY})
    target_include_directories(dbinterface INTERFACE ${FREETDS_INCLUDE_DIR})
endif()

if(FIREBIRD_FOUND)
    target_compile_definitions(dbinterface INTERFACE HAS_FIREBIRD)
    target_link_libraries(dbinterface INTERFACE ${FIREBIRD_LIBRARY})
    target_include_directories(dbinterface INTERFACE ${FIREBIRD_INCLUDE_DIR})
    
    if(IBPP_FOUND)
        target_compile_definitions(dbinterface INTERFACE HAS_IBPP)
        target_include_directories(dbinterface INTERFACE ${IBPP_INCLUDE_DIR})
    endif()
endif()

if(JNI_FOUND)
    target_compile_definitions(dbinterface INTERFACE HAS_JDBC_JNI)
    target_link_libraries(dbinterface INTERFACE ${JNI_LIBRARIES})
    target_include_directories(dbinterface INTERFACE ${JNI_INCLUDE_DIRS})
endif()

# Build examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Build tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation rules
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Generate and install CMake config files
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/DatabaseInterfaceConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DatabaseInterfaceConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/DatabaseInterfaceConfig.cmake"
    INSTALL_DESTINATION lib/cmake/DatabaseInterface
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/DatabaseInterfaceConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/DatabaseInterfaceConfigVersion.cmake"
    DESTINATION lib/cmake/DatabaseInterface
)

# Export targets
install(TARGETS dbinterface
    EXPORT DatabaseInterfaceTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(EXPORT DatabaseInterfaceTargets
    FILE DatabaseInterfaceTargets.cmake
    NAMESPACE DatabaseInterface::
    DESTINATION lib/cmake/DatabaseInterface
)

# Print configuration summary
message(STATUS "")
message(STATUS "Database Interface Configuration Summary:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "Database Support:")
message(STATUS "  MySQL/MariaDB: ${MARIADB_FOUND}${MYSQL_FOUND}")
message(STATUS "  PostgreSQL: ${PostgreSQL_FOUND} (libpqxx: ${PQXX_FOUND})")
message(STATUS "  MSSQL (ODBC): ${ODBC_FOUND}")
message(STATUS "  MSSQL (FreeTDS): ${FREETDS_FOUND}")
message(STATUS "  Firebird: ${FIREBIRD_FOUND} (IBPP: ${IBPP_FOUND})")
message(STATUS "  Generic ODBC: ${ODBC_FOUND}")
message(STATUS "  JDBC/JNI: ${JNI_FOUND}")
message(STATUS "")
message(STATUS "Options:")
message(STATUS "  Build Examples: ${BUILD_EXAMPLES}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
message(STATUS "")