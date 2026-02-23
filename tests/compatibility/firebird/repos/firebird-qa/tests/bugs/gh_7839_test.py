#coding:utf-8

"""
ID:          issue-7839
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7839
TITLE:       Potential bug in BETWEEN Operator
DESCRIPTION:
NOTES:
   Confirmed bug on 6.0.0.124.
   Checked on 6.0.0.127; 5.0.0.1270, 4.0.4.3021, 3.0.12.33724.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;

    with t (x, a, b) as
    (
        select 0, 10, 20 from rdb$database union all
        select 0, 20, 10 from rdb$database union all
        select 10, 0, 20 from rdb$database union all
        select 10, 20, 0 from rdb$database union all
        select 20, 0, 10 from rdb$database union all
        select 20, 10, 0 from rdb$database union all

        select null, 10, 20 from rdb$database union all
        select null, 20, 10 from rdb$database union all
        select 10, null, 20 from rdb$database union all
        select 20, null, 10 from rdb$database union all    -- wrong 
        select 10, 20, null from rdb$database union all    -- wrong  // ~  ticket: "SELECT (-1 BETWEEN 0 AND NULL) IS NULL FROM RDB$DATABASE;"
        select 20, 10, null from rdb$database union all

        select null, null, 20 from rdb$database union all
        select null, 20, null from rdb$database union all
        select 20, null, null from rdb$database union all


        select null, null, null from rdb$database
    ),

    b as
    (
        select
            t.*
            ,x between a and b as "x_btwn_a_and_b"
            ,a <= x and x <= b as "a_lss_x_and_x_lss_b"
            ,not (x between a and b) as "not_(x_btwn_a_and_b)"
            ,x < a or x > b as "x_<_a_or_x_>_b"
        from t
    )
    select * from b
    where
        "x_btwn_a_and_b" is distinct from "a_lss_x_and_x_lss_b"
        or
        "not_(x_btwn_a_and_b)" is distinct from "x_<_a_or_x_>_b"
    ;
"""

expected_stdout = """
    Records affected: 0
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0.12')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
