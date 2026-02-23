#coding:utf-8

"""
ID:          f97c463710
ISSUE:       https://www.sqlite.org/src/tktview/f97c463710
TITLE:       Incorrect ordering with ORDER BY and LIMIT
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
    create table t1(x int);
    insert into t1(x) values(1);
    insert into t1(x) values(5);
    insert into t1(x) values(3);
    insert into t1(x) values(4);
    insert into t1(x) values(2);

    set count on;
    select
       x, 01, 02, 03, 04, 05, 06, 07, 08, 09,
      10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
      20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
      30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
      40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
      50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60
    from t1
    order by x
    rows 1
    ;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    X 1
    CONSTANT 1
    CONSTANT 2
    CONSTANT 3
    CONSTANT 4
    CONSTANT 5
    CONSTANT 6
    CONSTANT 7
    CONSTANT 8
    CONSTANT 9
    CONSTANT 10
    CONSTANT 11
    CONSTANT 12
    CONSTANT 13
    CONSTANT 14
    CONSTANT 15
    CONSTANT 16
    CONSTANT 17
    CONSTANT 18
    CONSTANT 19
    CONSTANT 20
    CONSTANT 21
    CONSTANT 22
    CONSTANT 23
    CONSTANT 24
    CONSTANT 25
    CONSTANT 26
    CONSTANT 27
    CONSTANT 28
    CONSTANT 29
    CONSTANT 30
    CONSTANT 31
    CONSTANT 32
    CONSTANT 33
    CONSTANT 34
    CONSTANT 35
    CONSTANT 36
    CONSTANT 37
    CONSTANT 38
    CONSTANT 39
    CONSTANT 40
    CONSTANT 41
    CONSTANT 42
    CONSTANT 43
    CONSTANT 44
    CONSTANT 45
    CONSTANT 46
    CONSTANT 47
    CONSTANT 48
    CONSTANT 49
    CONSTANT 50
    CONSTANT 51
    CONSTANT 52
    CONSTANT 53
    CONSTANT 54
    CONSTANT 55
    CONSTANT 56
    CONSTANT 57
    CONSTANT 58
    CONSTANT 59
    CONSTANT 60
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
