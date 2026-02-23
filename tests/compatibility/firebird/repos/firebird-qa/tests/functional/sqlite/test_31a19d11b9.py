#coding:utf-8

"""
ID:          31a19d11b9
ISSUE:       https://www.sqlite.org/src/tktview/31a19d11b9
TITLE:       Name resolution issue with compound SELECTs and Common Table Expressions
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    with recursive
     t1(x) as (
         select 2 from rdb$database union all select x+2 from t1 where x<20
     )
    ,t2(y) as (
        select 3 from rdb$database union all select y+3 from t2 where y<20
    )
    select a.x 
    from t1 a
    where not exists(select 1 from t2 b where a.x = b.y)
    --except select y from t2 
    order by 1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    X 2
    X 4
    X 8
    X 10
    X 14
    X 16
    X 20
    Records affected: 7
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
