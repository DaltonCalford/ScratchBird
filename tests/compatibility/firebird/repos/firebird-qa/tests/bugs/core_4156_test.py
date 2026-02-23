#coding:utf-8

"""
ID:          issue-4483
ISSUE:       4483
TITLE:       RDB$GET_CONTEXT/RDB$SET_CONTEXT parameters incorrectly described as CHAR NOT NULL instead of VARCHAR NULLABLE
DESCRIPTION:
JIRA:        CORE-4156
FBTEST:      bugs.core_4156
NOTES:
    [12.12.2023] pzotov
    Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
    ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
    Added 'combine_output = True' in order to see SQLSTATE if any error occurs.

    [29.06.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set planonly;
    set sqlda_display on;
    select rdb$set_context( ?, ?, ?) x from rdb$database;
    -- NB: output in 3.0 will contain values of sqltype with ZERO in bit_0,
    -- so it will be: 448 instead of previous 449, and 496 instead of 497.
    -- Result is value that equal to constant from src/dsql/sqlda_pub.h, i.e:
    -- #define SQL_VARYING 448
    -- #define SQL_LONG    496
"""

act = isql_act('db', test_script, substitutions=[('^((?!(SQLSTATE|sqltype)).)*$', ''), ('[\t ]+', ' ')])


@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  'SYSTEM.'
    expected_stdout = f"""
        01: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 80 charset: 0 {SQL_SCHEMA_PREFIX}NONE
        02: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 80 charset: 0 {SQL_SCHEMA_PREFIX}NONE
        03: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 255 charset: 0 {SQL_SCHEMA_PREFIX}NONE
        01: sqltype: 496 LONG scale: 0 subtype: 0 len: 4
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

