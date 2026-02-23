#coding:utf-8

"""
ID:          issue-4627
ISSUE:       4627
TITLE:       Engine crashes when attempt to REcreate table with FK after syntax error before such recreating
DESCRIPTION:
JIRA:        CORE-4304
FBTEST:      bugs.core_4304
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
    set list on;
    recreate table test(x int);
     -- NB: there is no field `z` in this table:
    recreate table test(x int, constraint test_pk primary key(x), y int, constraint test_fk foreign key(y) references test(z));
    recreate table test(x int, constraint test_pk primary key(x), y int, constraint test_fk foreign key(y) references test(x));
    commit;
    insert into test(x, y) values(1, null);
    insert into test(x, y) values(2, 1);
    insert into test(x, y) values(3, 2);
    update test set y = 3 where x = 1;
    set count on;
    select * from test order by x;
"""

substitutions = [('[\t ]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -RECREATE TABLE TEST failed
    -could not find UNIQUE or PRIMARY KEY constraint in table TEST with specified columns
    X 1
    Y 3
    X 2
    Y 1
    X 3
    Y 2
    Records affected: 3
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -RECREATE TABLE "PUBLIC"."TEST" failed
    -could not find UNIQUE or PRIMARY KEY constraint in table "PUBLIC"."TEST" with specified columns
    X 1
    Y 3
    X 2
    Y 1
    X 3
    Y 2
    Records affected: 3
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
