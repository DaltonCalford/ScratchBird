#coding:utf-8

"""
ID:          2df0107bd2
ISSUE:       https://www.sqlite.org/src/tktview/2df0107bd2
TITLE:       Incorrect result from LEFT JOIN with a subquery on the LHS
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
    create table x1(x int, y int, z int);
    create table x2(a int);
    insert into x1 values(0,0,1);

    set count on;
    select avg(z) from x1 left join x2 on x is not null group by y;
    select avg(z) from (select * from x1) left join x2 on x is not null group by y;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    AVG 1
    Records affected: 1
    AVG 1
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
