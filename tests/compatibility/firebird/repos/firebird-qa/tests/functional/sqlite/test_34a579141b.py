#coding:utf-8

"""
ID:          34a579141b
ISSUE:       https://www.sqlite.org/src/tktview/34a579141b
TITLE:       Incorrect results with OR terms in the ON clause of a LEFT JOIN
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
    create table y1(a int, b int);
    create table y2(x int, y int);
    insert into y1 values(1, 1);
    insert into y2 values(3, 3);

    set count on;
    select * from y1 left join y2 on ((x=1 and y=b) or (x=2 and y=b));

    commit;
    create index y2xy on y2(x, y);

    select * from y1 left join y2 on ((x=1 and y=b) or (x=2 and y=b));
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 1
    B 1
    X <null>
    Y <null>
    Records affected: 1

    A 1
    B 1
    X <null>
    Y <null>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
