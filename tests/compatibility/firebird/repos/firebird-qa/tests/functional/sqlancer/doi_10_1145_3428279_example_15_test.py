#coding:utf-8

"""
ID:          n/a
ISSUE:       https://dl.acm.org/doi/pdf/10.1145/3428279
TITLE:       Wrong evaluation of MIN when bitwise shift is applied to the source value
DESCRIPTION:
    Manuel Rigger and Zhendong Su
    Finding Bugs in Database Systems via Query Partitioning
    https://dl.acm.org/doi/pdf/10.1145/3428279
    page 15 listing 15
NOTES:
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table t0 ( c0 int );
    insert into t0 values (-1) ;
    commit;
    select min(bin_shl(cast(c0 as bigint ),63)) as min_shl_63 from t0;
    select min(bin_shl(cast(c0 as int128 ), 127)) as min_shl_127 from t0;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = """
        MIN_SHL_63  -9223372036854775808
        MIN_SHL_127 -170141183460469231731687303715884105728
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
