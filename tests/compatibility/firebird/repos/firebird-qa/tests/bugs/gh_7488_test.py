#coding:utf-8

"""
ID:          issue-7488
ISSUE:       7488
TITLE:       Invalid real to string cast.
NOTES:
    [14.02.2023] pzotov
    Confirmed bug on 5.0.0.967; 4.0.3.2904
    Checked on intermediate builds 5.0.0.970; 4.0.3.2906
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select x, cast(x as varchar(10)) s
    from
    (
        select 345.12e-2 x from rdb$database union all
        select 4512e-4 from rdb$database union all
        select 3451.2e-3 from rdb$database union all
        select 34.512e-1 from rdb$database union all
        select 3.4512e0 from rdb$database union all
        select 0.34512e1 from rdb$database union all
        select 0.034512e2 from rdb$database union all
        select 0.0034512e3 from rdb$database
    );
"""

act = isql_act('db', test_script)

expected_stdout = """
    X                               3.451200000000000
    S                               3.4512000

    X                               0.4512000000000000
    S                               0.4512000

    X                               3.451200000000000
    S                               3.4512000

    X                               3.451200000000000
    S                               3.4512000

    X                               3.451200000000000
    S                               3.4512000

    X                               3.451200000000000
    S                               3.4512000

    X                               3.451200000000000
    S                               3.4512000

    X                               3.451200000000000
    S                               3.4512000
"""

@pytest.mark.version('>=4.0.3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
