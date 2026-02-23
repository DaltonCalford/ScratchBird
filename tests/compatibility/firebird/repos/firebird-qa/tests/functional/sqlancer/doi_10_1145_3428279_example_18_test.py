#coding:utf-8

"""
ID:          n/a
ISSUE:       https://dl.acm.org/doi/pdf/10.1145/3428279
TITLE:       Comparison of string and numeric literals
DESCRIPTION:
    Manuel Rigger and Zhendong Su
    Finding Bugs in Database Systems via Query Partitioning
    https://dl.acm.org/doi/pdf/10.1145/3428279
    page 23 listing 18
NOTES:
    [02.06.2025] pzotov
    This test issues only ONE row ('C0 -1') which differs from expected result shown in the source.
    Sent report to dimitr et al.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    recreate table t0 (c0 varchar(2) unique);
    insert into t0 values (-1);
    insert into t0 values (-2);
    select * from t0 where c0 >= -1;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = """
        C0 -1
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
