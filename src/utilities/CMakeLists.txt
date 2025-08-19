# Modern GPRE-free ScratchBird utilities
# These utilities are rewritten without GPRE dependencies

# sb_gfix - Database maintenance utility
add_executable(sb_gfix sb_gfix.cpp)
target_link_libraries(sb_gfix ${CMAKE_THREAD_LIBS_INIT})

# sb_gsec - Security management utility  
add_executable(sb_gsec sb_gsec.cpp)
target_link_libraries(sb_gsec ${CMAKE_THREAD_LIBS_INIT})

# sb_gstat - Database statistics utility
add_executable(sb_gstat sb_gstat.cpp)
target_link_libraries(sb_gstat ${CMAKE_THREAD_LIBS_INIT})

# sb_gbak - Backup and restore utility
add_executable(sb_gbak sb_gbak.cpp)
target_link_libraries(sb_gbak ${CMAKE_THREAD_LIBS_INIT})

# sb_isql - Interactive SQL utility
add_executable(sb_isql sb_isql.cpp)
target_link_libraries(sb_isql ${CMAKE_THREAD_LIBS_INIT} readline)

# Installation
install(TARGETS sb_gfix sb_gsec sb_gstat sb_gbak sb_isql
        RUNTIME DESTINATION bin)