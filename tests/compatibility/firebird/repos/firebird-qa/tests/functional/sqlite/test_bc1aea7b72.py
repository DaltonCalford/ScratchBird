#coding:utf-8

"""
ID:          bc1aea7b72
ISSUE:       https://www.sqlite.org/src/tktview/bc1aea7b72
TITLE:       Incorrect result on LEFT JOIN with OR constraints and an ORDER BY clause
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

    create table t1(a integer, b integer);
    insert into t1 values(1,2);
    insert into t1 values(3,4);

    -- correct answer when order by omitted
    set count on;
    select 'point-1' msg, x.*, y.*
    from t1 as x
    left join (select a as c, b as d from t1) as y on a=c
    where d=4 or d is null;

    -- incorrect answer when order by used
    select 'point-1' msg, x.*, y.*
    from t1 as x
    left join (select a as c, b as d from t1) as y on a=c
    where d=4 or d is null
    order by a;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    MSG point-1
    A 3
    B 4
    C 3
    D 4
    Records affected: 1

    MSG point-1
    A 3
    B 4
    C 3
    D 4
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
