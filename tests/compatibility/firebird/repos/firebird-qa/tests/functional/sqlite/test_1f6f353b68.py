#coding:utf-8

"""
ID:          1f6f353b68
ISSUE:       https://www.sqlite.org/src/tktview/1f6f353b68
TITLE:       Segfault when running query that uses SUM()OVER()
DESCRIPTION:
NOTES:
    [14.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test(b int, c int);
    insert into test(b,c)
    select mod(r,11), mod(r,19)
    from (select row_number()over() r from rdb$types rows 20);
    set count on;
    select
         sum(coalesce((select max(c) from test), b)) over(order by c) as f01
        ,sum(b)over(order by c) as f02
    from test
    order by sum(coalesce((select max(b) from test), c))over(order by b)
    ;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    F01 234
    F02 72
    F01 54
    F02 18
    F01 252
    F02 73
    F01 72
    F02 20
    F01 270
    F02 75
    F01 90
    F02 23
    F01 288
    F02 78
    F01 108
    F02 27
    F01 306
    F02 82
    F01 126
    F02 32
    F01 324
    F02 87
    F01 144
    F02 38
    F01 342
    F02 93
    F01 162
    F02 45
    F01 360
    F02 100
    F01 18
    F02 8
    F01 180
    F02 53
    F01 54
    F02 18
    F01 198
    F02 62
    F01 216
    F02 72
    Records affected: 20
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
