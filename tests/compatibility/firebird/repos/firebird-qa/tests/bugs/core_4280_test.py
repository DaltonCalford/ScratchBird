#coding:utf-8

"""
ID:          issue-4603
ISSUE:       4603
TITLE:       FB3: Stored function accepts duplicate input arguments
DESCRIPTION:
JIRA:        CORE-4280
FBTEST:      bugs.core_4280
NOTES:
    [30.09.2023] pzotov
    Expected error message become differ in FB 6.x, added splitting.

    [29.06.2025] pzotov
    Expected output on FB 6.x now has to include name of SQL schema.
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create function psql_func_test(a_x integer, a_y boolean, a_x integer) -- argument `a_x` appears twice
    returns integer as
    begin
        return a_x + 1;
    end
    ^
    set term ;^
    commit;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    CREATE FUNCTION PSQL_FUNC_TEST failed
    -SQL error code = -901
    -duplicate specification of A_X - not supported
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    CREATE FUNCTION "PUBLIC"."PSQL_FUNC_TEST" failed
    -Dynamic SQL Error
    -SQL error code = -637
    -duplicate specification of "A_X" - not supported
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
