#coding:utf-8

"""
ID:          issue-5336
ISSUE:       5336
TITLE:       Regression: incorrect calculation of byte-length for view columns
DESCRIPTION:
JIRA:        CORE-5049
FBTEST:      bugs.core_5049
NOTES:
    [12.12.2023] pzotov
    Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
    ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
    Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    -- Confirmed:
    -- 1) FAULT on WI-V3.0.0.32208.
    -- 2) SUCCESS on LI-V3.0.0.32233, Rev: 62699.
    set bail on;
    create or alter view v_test as
    select
       cast(rdb$character_set_name as varchar(2000)) as test_f01
      ,cast(rdb$character_set_name as varchar(2000)) as test_f02
      ,cast(rdb$character_set_name as varchar(2000)) as test_f03
    from
      rdb$database
    rows 1;

    set sqlda_display on;
    set list on;
    select * from v_test;
"""

act = isql_act('db', test_script, substitutions=[('^((?!SQLSTATE|sqltype).)*$', ''), ('[ \t]+', ' ')])

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else 'SYSTEM.'
    expected_stdout = f"""
        01: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 8000 charset: 4 {SQL_SCHEMA_PREFIX}UTF8
        02: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 8000 charset: 4 {SQL_SCHEMA_PREFIX}UTF8
        03: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 8000 charset: 4 {SQL_SCHEMA_PREFIX}UTF8
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
