#coding:utf-8

"""
ID:          54844eea3f
ISSUE:       https://www.sqlite.org/src/tktview/54844eea3f
TITLE:       Incorrect caching of sub-query results in the FROM clause of a scalar sub-query.
DESCRIPTION:
NOTES:
    [22.08.2025] pzotov
    Checked on 6.0.0.1244, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t4(a char, b int, c char(5));
    insert into t4 values('a', 1, 'one');
    insert into t4 values('a', 2, 'two');
    insert into t4 values('b', 1, 'three');
    insert into t4 values('b', 2, 'four');

    set count on;
    select 
        (
            select t.c from (
                select x.*
                from t4 x
                where x.a = out.a
                order by x.b offset 1 row fetch next 10 rows only
            ) t where t.b = out.b
        )
    from t4 as out;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C <null>
    C two
    C <null>
    C four
    Records affected: 4
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
