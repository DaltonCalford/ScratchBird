#coding:utf-8

"""
ID:          issue-7993
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7993
TITLE:       Unexpected results when using CASE WHEN with RIGHT JOIN
NOTES:
    [06.02.2024] pzotov
    Confirmed bug on 6.0.0.247
    Checked on 6.0.0.249 -- all OK.
    Checked on 5.0.1.1331 (commit 86902a69), 4.0.5.3054 (commit 147bff1a)
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t0(c0 boolean);
    recreate table t1(c1 boolean);

    insert into t0 (c0) values (true);
    insert into t1 (c1) values (false);

    set count on;
    set list on;
    select t1.c1 as q1_c1, t0.c0 as q1_c0 from t1 right join t0 on t0.c0; -- false true
    select t1.c1 as q2_c1, t0.c0 as q2_c0 from t1 right join t0 on t0.c0 where (case t1.c1 when t1.c1 then null else true end); -- null true (unexpected)
    select (case t1.c1 when t1.c1 then null else true end ) as q3_result from t1 right join t0 on t0.c0; -- null
"""

act = isql_act('db', test_script, substitutions = [('[ \t]+', ' ')])

expected_stdout = """
    Q1_C1                           <false>
    Q1_C0                           <true>
    Records affected: 1

    Records affected: 0
    
    Q3_RESULT                       <null>
    Records affected: 1
"""

@pytest.mark.version('>=4.0.5')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
