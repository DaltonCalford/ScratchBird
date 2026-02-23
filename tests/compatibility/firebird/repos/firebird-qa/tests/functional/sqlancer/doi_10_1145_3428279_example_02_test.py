#coding:utf-8

"""
ID:          n/a
ISSUE:       https://dl.acm.org/doi/pdf/10.1145/3428279
TITLE:       Wrong result of UNION DISTINCT
DESCRIPTION:
    Manuel Rigger and Zhendong Su
    Finding Bugs in Database Systems via Query Partitioning
    https://dl.acm.org/doi/pdf/10.1145/3428279
    page 10 listing 2
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate view v0 as select 1 x from rdb$database;
    recreate table t0 ( c0 int );
    recreate view v0 as select cast ( t0.c0 as integer ) as c0 from t0;

    insert into t0 ( c0 ) values (0);

    set count on;

    select distinct t0.c0 as q1_table_c0, v0.c0 as q1_view_c0
    from t0 left outer join v0 on v0.c0 >= '0'; -- expected = found = {0|0}

    select t0.c0 as q2_table_c0, v0.c0 as q2_view_c0 from t0 left outer join v0 on v0.c0 >= '0' where true
    union
    select * from t0 left join v0 on v0.c0 >= '0' where not true
    union
    select * from t0 left join v0 on v0.c0 >= '0' where true is null ; -- expected: {0|0}, found: {0|null}
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = """
        Q1_TABLE_C0  0
        Q1_VIEW_C0   0
        Records affected: 1

        Q2_TABLE_C0  0
        Q2_VIEW_C0   0
        Records affected: 1
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
