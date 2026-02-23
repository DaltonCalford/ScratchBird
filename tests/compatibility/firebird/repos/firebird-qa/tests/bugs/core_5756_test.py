#coding:utf-8

"""
ID:          issue-6019-A
ISSUE:       6019
TITLE:       Regression: FB crashes when trying to recreate table that is in use by DML (3.0.3; 3.0.4; 4.0.0)
DESCRIPTION:
JIRA:        CORE-5756
FBTEST:      bugs.core_5756
    [02.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Difference between current_connection values must be checked to be sure that there was no crash, see 'ATT_DIFF'

    Confirmed bug  (crash) on 3.0.3.32900.
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test(x int);
    insert into test values(1);
    select * from test;
    set term ^;
    execute block as
    begin
        rdb$set_context('USER_SESSION', 'INIT_ATT', current_connection);
    end ^
    set term ;^

    recreate table test(x int, y int); -- this led to crash

    select * from test;
    select current_connection - cast( rdb$get_context('USER_SESSION', 'INIT_ATT') as int) as att_diff from rdb$database;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    X 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -object TABLE "TEST" is in use
    X 1
    ATT_DIFF 0
"""

expected_stdout_6x = """
    X 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -object TABLE "PUBLIC"."TEST" is in use
    X 1
    ATT_DIFF 0
"""

@pytest.mark.version('>=3.0.4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
