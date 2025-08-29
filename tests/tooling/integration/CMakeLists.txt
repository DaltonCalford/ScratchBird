set(GOLDEN_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME isql_golden_script_ok
  COMMAND $<TARGET_FILE:isql> -f ${GOLDEN_DIR}/isql_golden_simple.sql --timing
)

