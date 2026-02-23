#coding:utf-8

"""
ID:          490a4b7235
ISSUE:       https://www.sqlite.org/src/tktview/490a4b7235
TITLE:       Assertion when "WHERE 0" on the first element of a UNION present
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
    create table abc(a int, b int, c int);
    create table def(d int, e int, f int);
    insert into abc values(1,2,3);
    insert into def values(3,4,5);

    set count on;
    select * from abc
    where false
    union
    select * from def;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 3
    B 4
    C 5
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
