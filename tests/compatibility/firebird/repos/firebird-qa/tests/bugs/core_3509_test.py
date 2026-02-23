#coding:utf-8

"""
ID:          issue-3867
ISSUE:       3867
TITLE:       Alter procedure allows to add the parameter with the same name
DESCRIPTION:
JIRA:        CORE-3509
FBTEST:      bugs.core_3509
NOTES:
    [30.09.2023] pzotov
    Expected error message become differ in FB 6.x, added splitting.
"""

import pytest
from firebird.qa import *

init_script = """
    set term ^;
    create or alter procedure duplicate_output_args returns (a_dup int) as
    begin
      a_dup = 1;
    Suspend;
    end^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set term ^;
    create or alter procedure duplicate_output_args returns( a_dup int, a_dup int) as
    begin
      a_dup = 1;
    Suspend;
    end
    ^
    set term ;^
    commit;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    CREATE OR ALTER PROCEDURE DUPLICATE_OUTPUT_ARGS failed
    -SQL error code = -901
    -duplicate specification of A_DUP - not supported
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    CREATE OR ALTER PROCEDURE "PUBLIC"."DUPLICATE_OUTPUT_ARGS" failed
    -Dynamic SQL Error
    -SQL error code = -637
    -duplicate specification of "A_DUP" - not supported
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
