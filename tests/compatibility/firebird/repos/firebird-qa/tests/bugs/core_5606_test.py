#coding:utf-8

"""
ID:          issue-5872
ISSUE:       5872
TITLE:       Add expression index name to exception message if computation failed
DESCRIPTION:
JIRA:        CORE-5606
FBTEST:      bugs.core_5606
NOTES:
    [24.08.2020]
        Changed sequence of actions: one statement must violate requirements of only ONE index.
        Before this statement:
          insert into test(id, x, y, s) values( 4, 3, -7, 'qwerty' );
        -- did violate TWO indices: test_eval3 and test_eval5.
        The order of which of them will raise first is undefined, so this test could fail because of appearance
        "wrong" index name in its STDERR. Detected on 4.0.0.2173. Discussed with hvlad, letter 23.08.2020 16:58.
    [30.06.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test(
      id int
      ,x int
      ,y int
      ,r double precision
      ,s varchar(12) default 'DB_NAME'
    );
    create index test_eval1 on test computed by ( x/id );
    insert into test(id, x) values(0, 1);
    rollback;
    drop index test_eval1;

    create index test_eval2 on test computed by ( log10(r-15) );
    insert into test(id, r) values(1, 12);
    rollback;
    drop index test_eval2;

    create index test_eval3 on test computed by ( rdb$get_context('SYSTEM', s ) );
    insert into test(id, s) values(2, 'FOO_&_BAR');
    rollback;
    drop index test_eval3;

    create index test_eval4 on test computed by ( mod(x , (y-x) ) );
    insert into test(id, x, y) values(3, 10, 10);
    rollback;
    drop index test_eval4;

    create index test_eval5 on test computed by ( substring(s from x for id+y)  );
    insert into test(id, x, y, s) values( 4, 3, -7, 'qwerty' );
    drop index test_eval5;

    set count on;
    set list on;
    select id, s from test;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 22012
    Expression evaluation error for index "TEST_EVAL1" on table "TEST"
    -arithmetic exception, numeric overflow, or string truncation
    -Integer divide by zero.  The code attempted to divide an integer value by an integer divisor of zero.
    Statement failed, SQLSTATE = 42000
    Expression evaluation error for index "TEST_EVAL2" on table "TEST"
    -expression evaluation not supported
    -Argument for LOG10 must be positive
    Statement failed, SQLSTATE = HY000
    Expression evaluation error for index "TEST_EVAL3" on table "TEST"
    -Context variable 'FOO_&_BAR' is not found in namespace 'SYSTEM'
    Statement failed, SQLSTATE = 22012
    Expression evaluation error for index "TEST_EVAL4" on table "TEST"
    -arithmetic exception, numeric overflow, or string truncation
    -Integer divide by zero.  The code attempted to divide an integer value by an integer divisor of zero.
    Statement failed, SQLSTATE = 22011
    Expression evaluation error for index "TEST_EVAL5" on table "TEST"
    -Invalid length parameter -3 to SUBSTRING. Negative integers are not allowed.
    Records affected: 0
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 22012
    Expression evaluation error for index "PUBLIC"."TEST_EVAL1" on table "PUBLIC"."TEST"
    -arithmetic exception, numeric overflow, or string truncation
    -Integer divide by zero.  The code attempted to divide an integer value by an integer divisor of zero.
    Statement failed, SQLSTATE = 42000
    Expression evaluation error for index "PUBLIC"."TEST_EVAL2" on table "PUBLIC"."TEST"
    -expression evaluation not supported
    -Argument for LOG10 must be positive
    Statement failed, SQLSTATE = HY000
    Expression evaluation error for index "PUBLIC"."TEST_EVAL3" on table "PUBLIC"."TEST"
    -Context variable 'FOO_&_BAR' is not found in namespace 'SYSTEM'
    Statement failed, SQLSTATE = 22012
    Expression evaluation error for index "PUBLIC"."TEST_EVAL4" on table "PUBLIC"."TEST"
    -arithmetic exception, numeric overflow, or string truncation
    -Integer divide by zero.  The code attempted to divide an integer value by an integer divisor of zero.
    Statement failed, SQLSTATE = 22011
    Expression evaluation error for index "PUBLIC"."TEST_EVAL5" on table "PUBLIC"."TEST"
    -Invalid length parameter -3 to SUBSTRING. Negative integers are not allowed.
    Records affected: 0
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
