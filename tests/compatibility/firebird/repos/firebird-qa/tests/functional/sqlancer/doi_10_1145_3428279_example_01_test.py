#coding:utf-8

"""
ID:          n/a
ISSUE:       https://dl.acm.org/doi/pdf/10.1145/3428279
TITLE:       predicate 0 = -0 to incorrectly evaluate to FALSE.
DESCRIPTION:
    Manuel Rigger and Zhendong Su
    Finding Bugs in Database Systems via Query Partitioning
    https://dl.acm.org/doi/pdf/10.1145/3428279
    page 2 listing  1
NOTES:
    [01.06.2025] pzotov
    Bug exists on Firebird 3.0.13.33807 (18.04.2025).

    [28.12.2025] pzotov
    Changed substitutions list: value +/-0e0 can be displayed with 16 digits after decimal point.
    We have to suppress "excessive" 16th+ zeroes.
    Detected on Intel Xeon W-2123 ("Intel64 Family 6 Model 85 Stepping 4") // Windows-10
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table t0 ( c0 int );
    recreate table t1 ( c0 double precision );
    commit;

    insert into t0 values (0) ;
    insert into t1 values ( -0e0 );

    -- select (t0.c0 = t1.c0) is true from t0 cross join t1;
    set count on;

    select t0.c0 as q1_t0_c0, t1.c0 as q1_t1_c0
    from t0 cross join t1 where t0.c0 = t1.c0 ; -- expected: {0, -0}; found: {}
    ----------------------------------------------------------------------
    select t0.c0 as q2_t0_c0, t1.c0 as q2_t1_c0 from t0 cross join  t1 where t0.c0 = t1.c0
    union all
    select * from t0 cross join  t1 where not ( t0.c0 = t1.c0 )
    union all
    select * from t0 cross join  t1 where ( t0.c0 = t1.c0 ) is null ; -- -- expected: {0, -0}; found: {}
    ----------------------------------------------------------------------
"""

substitutions= [('[\t ]+', ' '), ('.0{15,}', '.000000000000000')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = """
        Q1_T0_C0 0
        Q1_T1_C0 -0.000000000000000
        Records affected: 1

        Q2_T0_C0 0
        Q2_T1_C0 -0.000000000000000
        Records affected: 1
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
