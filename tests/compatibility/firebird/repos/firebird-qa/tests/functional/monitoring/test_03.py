#coding:utf-8

"""
ID:          monitoring-tables-03
TITLE:       table MON$COMPILED_STATEMENTS and columns MON$STATEMENTS.MON$COMPILED_STATEMENT_ID and MON$CALL_STACK.MON$COMPILED_STATEMENT_ID
DESCRIPTION:
    See https://github.com/FirebirdSQL/firebird/commit/3a452630b67f26d100c60234941a40d4468c170e (11-aug-2022)
    NB: this test verifies only presence of new table / columns.
    More complicated tests must be implemented for check the whole functionality.
    Checked on 5.0.0.958
NOTES:
    [16.12.2023] pzotov
    Replaced splitted code with assigning appropiate expected text using if-else depending on act.is_version result.
    Adjusted substitutions: runtime error must not be filtered out by '?!(...)' pattern
    ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
    Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set sqlda_display on;
    set planonly;
    select * from mon$compiled_statements;
    select mon$statements.mon$compiled_statement_id from mon$statements;
    select mon$call_stack.mon$compiled_statement_id from mon$call_stack;
"""

substitutions = [('^((?!SQLSTATE|sqltype|name:|table:).)*$', ''), ('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=5.0')
def test_1(act: Action):

    expected_stdout_5x = """
        01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
        : name: MON$COMPILED_STATEMENT_ID alias: MON$COMPILED_STATEMENT_ID
        : table: MON$COMPILED_STATEMENTS owner: SYSDBA
        02: sqltype: 520 BLOB Nullable scale: 0 subtype: 1 len: 8 charset: 4 UTF8
        : name: MON$SQL_TEXT alias: MON$SQL_TEXT
        : table: MON$COMPILED_STATEMENTS owner: SYSDBA
        03: sqltype: 520 BLOB Nullable scale: 0 subtype: 1 len: 8 charset: 4 UTF8
        : name: MON$EXPLAINED_PLAN alias: MON$EXPLAINED_PLAN
        : table: MON$COMPILED_STATEMENTS owner: SYSDBA
        04: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 252 charset: 4 UTF8
        : name: MON$OBJECT_NAME alias: MON$OBJECT_NAME
        : table: MON$COMPILED_STATEMENTS owner: SYSDBA
        05: sqltype: 500 SHORT Nullable scale: 0 subtype: 0 len: 2
        : name: MON$OBJECT_TYPE alias: MON$OBJECT_TYPE
        : table: MON$COMPILED_STATEMENTS owner: SYSDBA
        06: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 252 charset: 4 UTF8
        : name: MON$PACKAGE_NAME alias: MON$PACKAGE_NAME
        : table: MON$COMPILED_STATEMENTS owner: SYSDBA
        07: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
        : name: MON$STAT_ID alias: MON$STAT_ID
        : table: MON$COMPILED_STATEMENTS owner: SYSDBA

        01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
        : name: MON$COMPILED_STATEMENT_ID alias: MON$COMPILED_STATEMENT_ID
        : table: MON$STATEMENTS owner: SYSDBA
        01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
        : name: MON$COMPILED_STATEMENT_ID alias: MON$COMPILED_STATEMENT_ID
        : table: MON$CALL_STACK owner: SYSDBA
    """

    expected_stdout_6x = """
        01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
        : name: MON$COMPILED_STATEMENT_ID alias: MON$COMPILED_STATEMENT_ID
        : table: MON$COMPILED_STATEMENTS schema: SYSTEM owner: SYSDBA
        02: sqltype: 520 BLOB Nullable scale: 0 subtype: 1 len: 8 charset: 4 SYSTEM.UTF8
        : name: MON$SQL_TEXT alias: MON$SQL_TEXT
        : table: MON$COMPILED_STATEMENTS schema: SYSTEM owner: SYSDBA
        03: sqltype: 520 BLOB Nullable scale: 0 subtype: 1 len: 8 charset: 4 SYSTEM.UTF8
        : name: MON$EXPLAINED_PLAN alias: MON$EXPLAINED_PLAN
        : table: MON$COMPILED_STATEMENTS schema: SYSTEM owner: SYSDBA
        04: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 252 charset: 4 SYSTEM.UTF8
        : name: MON$OBJECT_NAME alias: MON$OBJECT_NAME
        : table: MON$COMPILED_STATEMENTS schema: SYSTEM owner: SYSDBA
        05: sqltype: 500 SHORT Nullable scale: 0 subtype: 0 len: 2
        : name: MON$OBJECT_TYPE alias: MON$OBJECT_TYPE
        : table: MON$COMPILED_STATEMENTS schema: SYSTEM owner: SYSDBA
        06: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 252 charset: 4 SYSTEM.UTF8
        : name: MON$PACKAGE_NAME alias: MON$PACKAGE_NAME
        : table: MON$COMPILED_STATEMENTS schema: SYSTEM owner: SYSDBA
        07: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
        : name: MON$STAT_ID alias: MON$STAT_ID
        : table: MON$COMPILED_STATEMENTS schema: SYSTEM owner: SYSDBA
        08: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 252 charset: 4 SYSTEM.UTF8
        : name: MON$SCHEMA_NAME alias: MON$SCHEMA_NAME
        : table: MON$COMPILED_STATEMENTS schema: SYSTEM owner: SYSDBA

        01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
        : name: MON$COMPILED_STATEMENT_ID alias: MON$COMPILED_STATEMENT_ID
        : table: MON$STATEMENTS schema: SYSTEM owner: SYSDBA
        01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
        : name: MON$COMPILED_STATEMENT_ID alias: MON$COMPILED_STATEMENT_ID
        : table: MON$CALL_STACK schema: SYSTEM owner: SYSDBA    
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
