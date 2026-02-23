#coding:utf-8

"""
ID:          issue-2013
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/2013
TITLE:       Altering procedure parameters must be prohibited if there are dependent data in RDB$DEPENDENCIES
DESCRIPTION:
    Firebird should not allow the ALTER PROCEDURE statements if there are records in RDB$DEPENDENCIES
    that reference a parameter that is not going to exists after the statement.
JIRA:        CORE-1592
FBTEST:      bugs.core_1592
NOTES:
    [25.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.863; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    set term ^;
    create or alter procedure sp_caller as begin end
    ^
    commit
    ^
    create or alter procedure sp_worker returns ( x1 integer ) as begin
    x1 = 10; suspend;
    end
    ^
    create or alter procedure sp_caller returns ( x1 integer ) as begin
    for select x1 from sp_worker into :x1 do suspend;
    end
    ^

    -- This should FAIL and terminate script execution:
    alter procedure sp_worker returns ( x2 integer ) as begin
    x2 = 10; suspend;
    end
    ^
    set term ;^
    commit;

"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -PARAMETER SP_WORKER.X1
    -there are 1 dependencies
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -PARAMETER "PUBLIC"."SP_WORKER".X1
    -there are 1 dependencies
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
