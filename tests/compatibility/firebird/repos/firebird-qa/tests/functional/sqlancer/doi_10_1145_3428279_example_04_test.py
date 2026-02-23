#coding:utf-8

"""
ID:          n/a
ISSUE:       https://dl.acm.org/doi/pdf/10.1145/3428279
TITLE:       Wrong result of AVG evaluation as result of SUM / COUNT
DESCRIPTION:
    Manuel Rigger and Zhendong Su
    Finding Bugs in Database Systems via Query Partitioning
    https://dl.acm.org/doi/pdf/10.1145/3428279
    page 11 listing 4
NOTES:
    [01.06.2025] pzotov
    Bug exists on Firebird 3.0.13.33807 (18.04.2025):
        Statement failed, SQLSTATE = 22003
        Integer overflow. The result of an integer operation caused the most significant bit of the result to carry.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    
    recreate table t0 ( c0 bigint );
    insert into t0 (c0) values (2) ;
    insert into t0 (c0) values (9223372036854775807) ;
    select avg (t0.c0) as avg_func from t0;
    commit;

    select sum (s)/ sum (c) as avg_eval
    from (
        select sum ( t0.c0 ) as s , count ( t0.c0 ) as c from t0 where c0 > 0
        union all
        select sum ( t0.c0 ) as s , count ( t0.c0 ) as c from t0 where not (c0 > 0)
        union all
        select sum ( t0.c0 ) as s , count ( t0.c0 ) as c from t0 where c0 is null
    ); -- { -4611686018427387903}
    commit;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = """
        AVG_FUNC 4611686018427387904
        AVG_EVAL 4611686018427387904
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
