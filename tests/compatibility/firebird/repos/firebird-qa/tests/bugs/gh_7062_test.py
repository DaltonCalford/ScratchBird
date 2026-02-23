#coding:utf-8

"""
ID:          issue-7062
ISSUE:       7062
TITLE:       Creation of expression index does not release its statement correctly
DESCRIPTION:
FBTEST:      bugs.gh_7062
NOTES:
    [04.07.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.894; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- case-1:
    create table test_1 (n integer);
    create table test_2 (n integer);
    create index test_1_expr on test_1 computed by ((select n from test_2));
    drop table test_1;
    commit;
    drop table test_2; -- this must NOT fail (raised "SQLSTATE = 42000 / object in use" before fix)

    -- case-2:
    create table test_a (n integer);
    create table test_b (n integer);
    insert into test_a values (0);
    commit;
    create index test_a_expr on test_a computed by (1 / 0 + (select 1 from test_b));
    drop table test_a;
    commit;
    drop table test_b; -- this must NOT fail (raised "SQLSTATE = 42000 / object in use" before fix)

"""

act = isql_act('db', test_script)

@pytest.mark.version('>=4.0.1')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 22012
        Expression evaluation error for index "***unknown***" on table {SQL_SCHEMA_PREFIX}"TEST_A"
        -arithmetic exception, numeric overflow, or string truncation
        -Integer divide by zero.  The code attempted to divide an integer value by an integer divisor of zero.
    """
    
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
