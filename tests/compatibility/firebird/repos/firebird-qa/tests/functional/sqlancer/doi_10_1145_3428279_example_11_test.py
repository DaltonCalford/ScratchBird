#coding:utf-8

"""
ID:          n/a
ISSUE:       https://dl.acm.org/doi/pdf/10.1145/3428279
TITLE:       Non-deterministic output when using MAX() function
DESCRIPTION:
    Manuel Rigger and Zhendong Su
    Finding Bugs in Database Systems via Query Partitioning
    https://dl.acm.org/doi/pdf/10.1145/3428279
    page 14 listing 11
NOTES:
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table t0 ( c0 int );
    recreate table t1 ( c0 varchar(100) );
    insert into t1 values (0.9201898334673894);
    insert into t1 values (0);
    insert into t0 values (0);

    select *
    from t0 cross join t1
    group by t0.c0, t1.c0
    having t1.c0 != max(t1.c0)
    
    UNION ALL
    
    select *
    from t0 cross join t1
    group by t0.c0, t1.c0
    having not t1.c0 > max (t1.c0)
    ;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = """
        C0 0
        C0 0
        C0 0
        C0 0.9201898334673894
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
