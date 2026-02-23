#coding:utf-8

"""
ID:          issue-2796
ISSUE:       2796
TITLE:       ALTER TRIGGER / PROCEDURE wrong error message
DESCRIPTION:
JIRA:        CORE-2374
FBTEST:      bugs.core_2374
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    alter procedure test1 as
    begin
      if (a = b) then
       b = 1;
    end
    ^
    alter trigger trg1 as
    begin
      if (a = b) then
       b = 1;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER PROCEDURE TEST1 failed
    -Procedure TEST1 not found

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER TRIGGER TRG1 failed
    -Trigger TRG1 not found
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER PROCEDURE "PUBLIC"."TEST1" failed
    -Procedure "PUBLIC"."TEST1" not found

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER TRIGGER "PUBLIC"."TRG1" failed
    -Trigger "PUBLIC"."TRG1" not found
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
