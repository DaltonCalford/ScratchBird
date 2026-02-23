#coding:utf-8

"""
ID:          n/a
ISSUE:       https://dl.acm.org/doi/pdf/10.1145/3428279
TITLE:       Failed to fetch a row from a view.
DESCRIPTION:
    Manuel Rigger and Zhendong Su
    Finding Bugs in Database Systems via Query Partitioning
    https://dl.acm.org/doi/pdf/10.1145/3428279
    page 12 listing 6
NOTES:
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table t0 (c0 int);
    recreate view v0 as select t0.c0, true as c1 from t0;
    insert into t0 values (0);
    select v0.c0 from v0 cross join t0 where v0.c1;
    commit;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = """
        C0 0
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
