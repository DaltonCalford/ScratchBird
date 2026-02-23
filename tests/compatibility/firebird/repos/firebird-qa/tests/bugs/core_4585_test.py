#coding:utf-8

"""
ID:          issue-4901
ISSUE:       4901
TITLE:       Can't create column check constraint when the column is domain based
DESCRIPTION:
JIRA:        CORE-4585
FBTEST:      bugs.core_4585
NOTES:
    [29.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create domain x int;
    create table test(
        x x constraint test_x_chk check(x>0)
    );
    insert into test(x) values(1);
    insert into test(x) values(0);
    set list on;
    select * from test;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 23000
    Operation violates CHECK constraint TEST_X_CHK on view or table TEST
    -At trigger 'CHECK_1'
    X                               1
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 23000
    Operation violates CHECK constraint "TEST_X_CHK" on view or table "PUBLIC"."TEST"
    -At trigger "PUBLIC"."CHECK_1"
    X                               1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
