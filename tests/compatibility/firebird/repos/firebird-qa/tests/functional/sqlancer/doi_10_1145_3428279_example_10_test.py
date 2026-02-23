#coding:utf-8

"""
ID:          n/a
ISSUE:       https://dl.acm.org/doi/pdf/10.1145/3428279
TITLE:       Unnexpectedly optimized VARIANCE(0) to FALSE
DESCRIPTION:
    Manuel Rigger and Zhendong Su
    Finding Bugs in Database Systems via Query Partitioning
    https://dl.acm.org/doi/pdf/10.1145/3428279
    page 14 listing 10
NOTES:
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t0 (c0 int);
    insert into t0 (c0) values (0);
    select t0.c0
    from t0
    group by t0.c0
    having not ( (select var_pop(0) from rdb$database where false) is null );
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = """
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
