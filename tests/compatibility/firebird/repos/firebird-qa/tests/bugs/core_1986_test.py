#coding:utf-8

"""
ID:          issue-2424
ISSUE:       2424
TITLE:       Altering domains name drops dependencies using the domain
DESCRIPTION:
JIRA:        CORE-1986
FBTEST:      bugs.core_1986
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create domain dm_int as int;
    set term ^;
    create or alter procedure sp_some(a_x dm_int)
    as
    begin
    end
    ^
    set term ;^
    alter domain dm_int to d_other;
    execute procedure sp_some (1);
    commit;
    execute procedure sp_some (1);
    commit;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -DOMAIN DM_INT
    -there are 1 dependencies
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -DOMAIN "PUBLIC"."DM_INT"
    -there are 1 dependencies
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
