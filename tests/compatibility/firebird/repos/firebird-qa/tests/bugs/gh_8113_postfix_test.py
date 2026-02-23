#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/d7ba33db3298902a31aecba0caef96819d3b0ec9
TITLE:       Postfix for #8113: UNION ALL optimization with constant false condition.
DESCRIPTION:
NOTES:
    [18.11.2025] pzotov
    Thanks to dimitr for provided example.
    Confirmed bug (regression ?) on 6.0.0.1338.
    Checked on 6.0.0.1356 5.0.4.1735 4.0.7.3237 3.0.14.33827.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    select 1 as unexpected_id
    from rdb$database
    where 1 >= all (
       select 1 from rdb$database
       union distinct
       select null from rdb$database
    );

    select 2 as unexpected_id
    from rdb$database
    where 1 <= all (
       select 1 from rdb$database
       union all
       select null from rdb$database
    );
"""

act = isql_act('db', test_script)

expected_stdout = """
    Records affected: 0
    Records affected: 0
"""

@pytest.mark.version('>=3.0')
def test_2(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
