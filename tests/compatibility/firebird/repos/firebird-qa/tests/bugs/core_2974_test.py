#coding:utf-8

"""
ID:          issue-3356
ISSUE:       3356
TITLE:       Unexpected "Invalid SIMILAR TO pattern" error
DESCRIPTION:
JIRA:        CORE-2974
FBTEST:      bugs.core_2974
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    -- Should raise "Invalid SIMILAR TO pattern" error, as minus sign should be ecaped
    select case when '-1' similar to '(-)%' then 1 else 0 end as chk_a
      from rdb$database
    ;

    -- Should raise "Invalid SIMILAR TO pattern" error because there is no "default" escape character:
    select case when '-1' similar to '(\\-)%' then 1 else 0 end as chk_b
      from rdb$database
    ;

    -- Should NOT raise error:
    select case when '-1' similar to '(\\-)%' escape '\\' then 1 else 0 end as chk_c
      from rdb$database
    ;
    -- works ok

    -- Should NOT raise error:
    select case when '-1' similar to '(\\+)%' then 1 else 0 end as chk_d
      from rdb$database
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CHK_C                           1
    CHK_D                           0
"""

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Invalid SIMILAR TO pattern

    Statement failed, SQLSTATE = 42000
    Invalid SIMILAR TO pattern
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

