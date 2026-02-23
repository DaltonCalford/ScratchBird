#coding:utf-8

"""
ID:          02a8e81d44
ISSUE:       https://www.sqlite.org/src/tktview/02a8e81d44
TITLE:       LIMIT clause on sub-select in FROM clause of a SELECT in a UNION ALL interpreted incorrectly
DESCRIPTION:
NOTES:
    [22.08.2025] pzotov
    Checked on 6.0.0.1244, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a int);
    insert into t1 values(1);
    insert into t1 values(2);

    select 'q1' as msg from rdb$database;
    select x.* from (select * from t1 rows 1) x UNION ALL select t1.* from t1 rows 0;

    select 'q2' as msg from rdb$database;
    select x.* from (select * from t1 rows 1) x UNION select t1.* from t1 rows 0;

    select 'q3' as msg from rdb$database;
    select x.* from (select * from t1 rows 0) x UNION ALL select t1.* from t1 rows 1;

    select 'q4' as msg from rdb$database;
    select x.* from (select * from t1 rows 0) x UNION select t1.* from t1 rows 1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    MSG q1

    MSG q2
    
    MSG q3
    A 1
    
    MSG q4
    A 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
