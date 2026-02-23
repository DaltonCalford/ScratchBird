#coding:utf-8

"""
ID:          510cde2777
ISSUE:       https://www.sqlite.org/src/tktview/510cde2777
TITLE:       Endless loop on a query with window functions, ORDER BY, and LIMIT
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(id int, b varchar(1), c varchar(5));
    insert into t1 values(1, 'a', 'one');
    insert into t1 values(2, 'b', 'two');
    insert into t1 values(3, 'c', 'three');
    insert into t1 values(4, 'd', 'one');
    insert into t1 values(5, 'e', 'two');

    set count on;
    select id, b, lead(c,1) over(order by c) as x from t1 where id > 1 order by b rows 1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    ID 2
    B b
    X two
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
