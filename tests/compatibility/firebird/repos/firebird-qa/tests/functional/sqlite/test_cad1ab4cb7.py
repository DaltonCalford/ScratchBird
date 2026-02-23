#coding:utf-8

"""
ID:          cad1ab4cb7
ISSUE:       https://www.sqlite.org/src/tktview/cad1ab4cb7
TITLE:       Segfault due to LEFT JOIN flattening optimization
DESCRIPTION:
NOTES:
    [20.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    select *
    from (
       select 1 a from rdb$database
    ) s
    left join (
        select 1 b, x.* from (
            select x.* from (
                select 1 c from rdb$database
            ) x
        ) x
    ) x
    on s.a = x.b
    ;
    -----------------
    create table t1(c char primary key, a char(10000), b char (10000));
    select x.x, max(y.y) y_max, max(y.a) a_max, max(y.b) b_max
    from
    (
        select '222' x from rdb$database
    ) x left join (
        select c || '222' y, a, b from t1
    ) y on x.x = y.y
    group by 1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 1
    B 1
    C 1
    Records affected: 1

    X 222
    Y_MAX <null>
    A_MAX <null>
    B_MAX <null>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
