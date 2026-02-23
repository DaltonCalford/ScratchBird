#coding:utf-8

"""
ID:          issue-8477
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8477
TITLE:       Inheritance of WINDOW does not work
DESCRIPTION:
NOTES:
    [19.03.2025] pzotov
    Confirmed bug (wrong data in 'error_sum' field) on 6.0.0.680-90d2983 (18-mar-2025 20:23).
    Checked on intermediate snapshot 6.0.0.680-9178ee6 (19-mar-2025 15:23) -- all fine.

    [20.03.2025] pzotov
    Checked on 5.0.3.1633-25a0817, 4.0.6.3192-91e3c11; reduced min_version to '4.0.6'.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test(i int, j int);
    insert into test values(1, 10);
    insert into test values(2, 20);
    insert into test values(3, 30);
    insert into test values(4, 40);
    insert into test values(5, 50);

    select i, j,
        sum(j) over (w2) as error_sum,    -- looks like no partition
        sum(j) over (w1 order by j) as correct1,
        sum(j) over (partition by i order by j) as correct2,
        sum(j) over (order by j) as like_error_sum
    from test
    window 
        w1 as (partition by i), 
        w2 as (w1 order by j)
    ;
"""

act = isql_act('db', test_script, substitutions = [('[ \t]+', ' ')])

expected_stdout = """
    I 1
    J 10
    ERROR_SUM 10
    CORRECT1 10
    CORRECT2 10
    LIKE_ERROR_SUM 10
    I 2
    J 20
    ERROR_SUM 20
    CORRECT1 20
    CORRECT2 20
    LIKE_ERROR_SUM 30
    I 3
    J 30
    ERROR_SUM 30
    CORRECT1 30
    CORRECT2 30
    LIKE_ERROR_SUM 60
    I 4
    J 40
    ERROR_SUM 40
    CORRECT1 40
    CORRECT2 40
    LIKE_ERROR_SUM 100
    I 5
    J 50
    ERROR_SUM 50
    CORRECT1 50
    CORRECT2 50
    LIKE_ERROR_SUM 150
"""

@pytest.mark.version('>=4.0.6')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

