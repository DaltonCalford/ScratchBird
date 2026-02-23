#coding:utf-8

"""
ID:          6f2222d550
ISSUE:       https://www.sqlite.org/src/tktview/6f2222d550
TITLE:       Incorrect output on a LEFT JOIN
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
    create table x1(a int);
    create table x2(b int not null);    -- remove the not null and things work
    create table x3(c char(1), d int);

    insert into x1 values(1);
    insert into x3 values('a', null);
    insert into x3 values('b', null);
    insert into x3 values('c', null);

    set count on;
    select * from x1 left join x2 on x1.a = x2.b left join x3 on x3.d = x2.b;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 1
    B <null>
    C <null>
    D <null>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
