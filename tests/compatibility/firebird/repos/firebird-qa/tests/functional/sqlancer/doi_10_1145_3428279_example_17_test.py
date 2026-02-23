#coding:utf-8

"""
ID:          n/a
ISSUE:       https://dl.acm.org/doi/pdf/10.1145/3428279
TITLE:       Conversion of character string to boolean value
DESCRIPTION:
    Manuel Rigger and Zhendong Su
    Finding Bugs in Database Systems via Query Partitioning
    https://dl.acm.org/doi/pdf/10.1145/3428279
    page 23 listing 17
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table t0 (c0 boolean);
    insert into t0 values (false);
    select * from t0 where not (c0 != 'true' and c0);
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = """
        C0 <false>
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
